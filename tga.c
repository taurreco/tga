#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "tga.h"

/*********************************************************************
 *                                                                   *
 *                      private data structures                      *
 *                                                                   *
 *********************************************************************/

/**********
 * header *
 **********/

/* holds the header of a tga in memory */

struct header {
   uint8_t id_len;
   uint8_t color_type;
   uint8_t data_type;
   int16_t color_start;
   int16_t color_len;
   uint8_t color_depth;
   int16_t x;
   int16_t y;
   int16_t w;
   int16_t h;
   uint8_t pixel_depth;
   uint8_t desc;
};

/*******
 * tga *
 *******/

/* represents the tga file in memory */

struct tga {
   struct header header;
   uint8_t* colors;
   uint8_t* pixels;
};


/*********************************************************************
 *                                                                   *
 *                          private helpers                          *
 *                                                                   *
 *********************************************************************/

/**********
 * readu8 *
 **********/

void
readu8(uint8_t* dst, FILE* fp)
{
    fread(dst, 1, 1, fp);
}

/***********
 * readu16 *
 ***********/

void
readu16(uint16_t* dst, FILE* fp)
{
    fread(dst, 1, 2, fp);
}

/***********
 * readbuf *
 ***********/

void
readbuf(void* dst, int len, FILE* fp)
{
    fread(dst, 1, len, fp);
}

/**********
 * unpack *
 **********/

/* unpacks given bit representation of color to uint32  */

uint32_t
unpack(uint8_t* bytes, int n_bytes)
{
    uint8_t a, r, g, b;

    a = 0;
    r = 0;
    g = 0;
    b = 0;

    switch (n_bytes) {
        
	case 2:
            /* ARRRRRGG GGGBBBBB */
            
	    /* grab 5-bit representation of each color */
         
	    r = (bytes[1] >> 2) & 0x1F;
            g = ((bytes[1] << 3) & 0x18) | ((bytes[0] >> 5) & 0x07);
            b = (bytes[0]) & 0x1F;

            /* scale channels by 8.2258 */
         
	    r = (r << 3) | (r >> 2);
            g = (g << 3) | (g >> 2);
            b = (b << 3) | (b >> 2);

            /* attribute channel */
         
	    a = 255 * ((bytes[0] & 0x80) >> 7);
           
	    break;

        case 3:   
	    /* RRRRRRRR GGGGGGGG BBBBBBBB */

            a = 255;
            r = bytes[2];
            g = bytes[1];
            b = bytes[0];
            break;
        
	case 4:  
            /* AAAAAAAA RRRRRRRR GGGGGGGG BBBBBBBB */
            
	    a = bytes[3];
            r = bytes[2];
            g = bytes[1];
            b = bytes[0];
            break;
   }

   return a << 24 | r << 16 | g << 8 | b;
}

/********
 * read *
 ********/

/* reads tga data into RAM for quick parsing */

void
read(struct tga* tga, FILE* fp)
{
    struct header* header;
    uint8_t *colors, *pixels;
    int pixel_bytes, color_bytes;
    
    /* fill header */
    
    readu8(&header->id_len, fp);
    readu8(&header->color_type, fp);
    readu8(&header->data_type, fp);
    readu16(&header->color_origin, fp);
    readu16(&header->color_len, fp);
    readu8(&header->color_depth, fp);
    readu16(&header->x, fp);
    readu16(&header->y, fp);
    readu16(&header->w, fp);
    readu16(&header->h, fp);
    readu8(&header->pixel_depth, fp);
    readu8(&header->desc, fp);

    pixel_bytes = header->pixel_depth / 8;
    color_bytes = header->color_depth / 8;

    /* fill color map and image data */

    colors = calloc(header->color_len * color_bytes, 1);
    pixels = calloc(header->w * header->h * pixel_bytes, 1);

    fseek(fp, header->id_len, SEEK_CUR);
    readbuf(colors, header->color_len * color_bytes, fp);
    readbuf(pixels, header->w * header->h * pixel_bytes, fp);

    tga->header = *header;
    tga->colors = colors;
    tga->pixels = pixels;
}

/*********************************************************************
 *                                                                   *
 *                         public definition                         *
 *                                                                   *
 *********************************************************************/

/************
 * tga_load *
 ************/

/* placeholder */

void
tga_load(char* file, uint32_t** colors, int* width, int* height)
{
   struct tga tga;

   /* open file */
   FILE* fp = fopen(file, "rb");
   if (!fp)
      return 0;

   read(&tga, fp);
   fclose(fp);

   int pixel_depth = tga.header.bits_per_pixel / 8;
   int color_depth = tga.header.color_map_depth / 8;
   uint8_t* color_tga;    /* raw color bytes read from tga */
   uint8_t* packet;

   uint32_t* colors = calloc(tga.header.width * tga.header.height, 4);
   

   switch (tga.header.data_type_code) {
      case 1:    /* uncompressed color mapped */

         for (int i = 0; i < tga.header.width * tga.header.height; i++) {
            color_tga = tga.color_map + tga.image_data[i] * color_depth;
            colors[i] = unpack(color_tga, color_depth);
         }
         break;

      case 2:    /* uncompressed RGB */

         color_tga = tga.image_data;
         for (int i = 0; i < tga.header.width * tga.header.height; i++) {
            colors[i] = unpack(color_tga, pixel_depth);
            color_tga += pixel_depth;
         }
         break;

      case 9:    /* run length encoded & color mapped */
      case 10:   /* run length encoded RGB */

         packet = tga.image_data;

         for (int i = 0; i < tga.header.width * tga.header.height; i++) {

            int len = (*packet & 0x7F) + 1;

            int depth = tga.header.color_map_type ? color_depth : pixel_depth;
            uint8_t* color_addr = tga.header.color_map_type ? tga.color_map + packet[1] : packet + 1;

            if (*packet & 0x80) {    /* run length packet */

               uint32_t color = unpack(color_addr, depth);
               for (int j = 0; j < len; j++) {
                  colors[i + j] = color;
               }

               /* next packet */
               packet += pixel_depth + 1;  
            } else {                /* raw packet */

               for (int j = 0; j < len; j++) {
                  colors[i + j] = unpack(color_addr, depth);
                  color_addr += pixel_depth;
               }

               /* next packet */
               packet += len * pixel_depth + 1;  
            }

            i += len - 1;
         }
         break;

      default:
         printf("unsupported tga data type code");
         free(colors);
         return 0;
   }

   /* return */
   struct sr_texture* texture = malloc(sizeof(struct sr_texture)); 
   texture->colors = colors;
   texture->width = tga.header.width;
   texture->height = tga.header.height;
   free(tga.color_map);
   free(tga.image_data);

   return texture;
}

/*******************
 * sr_texture_free *
 *******************/

/* frees a heap allocated sr_texture struct */
extern void
sr_texture_free(struct sr_texture* texture)
{
   free(texture->colors);
   free(texture);
}


