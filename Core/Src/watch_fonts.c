/*
 * watch_fonts.c
 *
 *  Created on: Feb 12, 2024
 *      Author: slst
 */

#include "watch_fonts.h"
#include "Fonts/k2d_bold_48.h"
#include "Fonts/k2d_extrabold_64.h"
#include "Fonts/test_font.h"
#include <stdint.h>

Font k2d_extrabold_64 = {
    .char_width = k2d_extrabold_64_width,
    .char_addr = k2d_extrabold_64_addr,
    .font_height = k2d_extrabold_64_height,
	.char_index = k2d_extrabold_64_index,
};

Font k2d_bold_48 = {
    .char_width = char_width,
    .char_addr = char_addr,
    .font_height = font_height,
	.char_index = char_index,
};

Font test_font = {
    .char_width = test_font_width,
    .char_addr = test_font_addr,
    .font_height = test_font_height,
	.char_index = test_font_index,
};

extern Font* currentFont;

void setFont(Font* font) {
    currentFont = font;
}
