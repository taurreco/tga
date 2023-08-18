#ifndef TGA_H
#define TGA_H

#include <stdint.h>

/**********
 * header *
 **********/

struct header {
   uint8_t id_len;
   uint8_t color_map_type;
   uint8_t data_type_code;
   int16_t color_map_origin;
   int16_t color_map_len;
   uint8_t color_map_depth;
   int16_t x_origin;
   int16_t y_origin;
   int16_t width;
   int16_t height;
   uint8_t bits_per_pixel;
   uint8_t image_desc;
};

/*******
 * tga *
 *******/

struct tga {
   struct header header;
   uint8_t* color_map;
   uint8_t* image_data;
};

void tga_load(char* file, uint32_t* colors, int* width, int* height);

#endif    /* TGA_H */
