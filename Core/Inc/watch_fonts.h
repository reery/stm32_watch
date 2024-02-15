/*
 * watch_fonts.h
 *
 *  Created on: Feb 12, 2024
 *      Author: slst
 */

#ifndef INC_WATCH_FONTS_H_
#define INC_WATCH_FONTS_H_

#include <stdint.h>


typedef struct {
    const char* char_width;
    const char** char_addr;
    const int font_height;
    const int char_index;
} Font;

extern Font k2d_bold_48;
extern Font k2d_extrabold_64;
extern Font test_font;

void setFont(Font* font);

#endif /* INC_WATCH_FONTS_H_ */
