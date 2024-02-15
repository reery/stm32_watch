/*
 * test_font.h
 *
 *  Created on: Feb 12, 2024
 *      Author: slst
 */

#ifndef INC_FONTS_TEST_FONT_H_
#define INC_FONTS_TEST_FONT_H_

const int test_font_height = 16; // Divided by 8 tells us how many byte rows we have, here 2
const int test_font_index = 48;
// 255 = 11111111
//   1 = 00000001   bits are read from right to left
// 128 = 10000000
// First 16 bytes is the top line, second 16 bytes are the bottom line
const char test_font_48[] = {255,1, 1,1, 1,1, 1,1, 1,1, 1,1, 1,1, 1,255, 255,128, 128,128, 128,128, 128,128, 128,128, 128,128, 128,128, 128,255}; //number 0, 16x16 px square
const char test_font_width[] = {16}; // The first 16 bytes belong to the top 8 bits, then the next row is started
const char* test_font_addr[] = {test_font_48};


#endif /* INC_FONTS_TEST_FONT_H_ */
