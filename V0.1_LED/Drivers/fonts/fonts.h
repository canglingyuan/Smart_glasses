#ifndef __FONTS_H
#define __FONTS_H

#include <stdint.h>

typedef struct {
    const uint8_t width;
    const uint8_t height;
    const uint8_t *data;
} FontDef;

extern FontDef Font_6x8;

#endif