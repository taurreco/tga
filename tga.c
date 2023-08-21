#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "tga.h"

#define MAPPED          1
#define RGB             2
#define RLE_MAPPED      9
#define RLE_RGB         10

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

/**********
 * mapped *
 **********/

void
mapped(uint32_t* data, struct tga* tga)
{
    struct header* header;
    uint8_t* bytes;
    int n_bytes;
    
    header = &tga->header;
    n_bytes = header->color_depth >> 3;

    for (int i = 0; i < header.w * header.h; i++) {
        bytes = tga.colors + tga.pixels[i] * n_bytes;
        data[i] = unpack(bytes, n_bytes);
    }
}

/*******
 * rgb *
 *******/

void
rgb(uint32_t* data, struct tga* tga)
{
    struct header* header;
    uint8_t* bytes;
    int n_bytes;
    
    header = &tga->header;
    bytes = tga->pixels;
    n_bytes = header->pixel_depth >> 3;
    
    for (int i = 0; i < header.w * header.h; i++) {
        data[i] = unpack(bytes, n_bytes);
        bytes += n_bytes;
    }
}

/*******
 * rle *
 *******/

void
rle(uint32_t* data, struct tga* tga, uint8_t* bytes, int stride)
{
    int pixel_bytes;
    uint8_t* packet;
    
    pixel_bytes = tga->pixel_depth >> 3;
    packet = tga->pixels;

    for (int i = 0; i < header.w * header.h; i++) {
        
	int len;
        len = (*packet & 0x7F) + 1;
        
	if (*packet & 0x80) {    /* run length packet */
            
            uint32_t color;
            
	    color = unpack(bytes, stride);
            for (int j = 0; j < len; j++) {
                data[i + j] = color;
            }

            /* next packet */

            packet += pixel_bytes + 1;
        } else {                /* raw packet */
            for (int j = 0; j < len; j++) {
                data[i + j] = unpack(bytes, stride);
                bytes += pixel_bytes;
            }

            /* next packet */
            packet += len * pixel_bytes + 1;  
        }
        i += len - 1;
    }
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

int
tga_load(char* file, uint32_t** data_p, int* width_p, int* height_p)
{
    FILE* fp;
    struct tga tga;
    struct header header;
    uint32_t* data;
    uint8_t* bytes;
    int stride;

    /* read file into memory */

    fp = fopen(file, "rb");
    read(&tga, fp);
    fclose(fp);

    header = tga.header;

    pixel_depth = header.pixel_depth / 8;
    color_depth = header.color_depth / 8;

    data = calloc(header.w * header.h, 4);
   
    stride = color_bytes;
    bytes = tga.colors + packet[1];

    switch (header.data_type) {
	
	/* uncompressed color mapped */
	case MAPPED:
	    mapped(data, &tga);	
	    break;
	/* uncompressed RGB */
	case RGB:
            rgb(data, &tga);
	    break;
        case RLE_MAPPED:    /* run length encoded & color mapped */
            stride = pixel_bytes;
            bytes = packet + 1;
        case RLE_RGB:   /* run length encoded RGB */
            rle(data, tga, bytes, stride);
            break;
        default:
            free(colors);
            return 0;
    }

    /* return */
    texture->height = tga.header.height;
    free(tga.color_map);
    free(tga.image_data);
    return 1;
}
