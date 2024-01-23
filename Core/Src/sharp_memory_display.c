/*
 * sharp_memory_display.c
 *
 *  Created on: Jan 12, 2024
 *      Author: slst
 */

#include "sharp_memory_display.h"
#include "stm32wbxx_hal.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Fonts/k2d_bold_48.h"
#include <stdio.h>

extern SPI_HandleTypeDef hspi1;
uint8_t clear_command = 0x20;  // M2 clear flag
uint8_t write_mode = 0x80; // M0 write mode command
uint8_t vcom_bit = 0x40;
uint8_t dummy_16bit = 0;
uint8_t dummy_8bit = 0x00;
//uint8_t frontBuffer[DISPLAY_HEIGHT][DISPLAY_WIDTH / 8];
uint8_t backBuffer[DISPLAY_HEIGHT][DISPLAY_WIDTH / 8] __attribute__((aligned(4)));
uint8_t (*currentBuffer)[DISPLAY_WIDTH / 8] = backBuffer;
uint8_t sendToDisplayBuffer[TOTAL_DATA_SIZE];

void clearDisplay(void) {
	SCS_HIGH();
	HAL_SPI_Transmit(&hspi1, &clear_command, 1, HAL_MAX_DELAY);
	//HAL_SPI_Transmit(&hspi1, &dummy_8bit, 1, HAL_MAX_DELAY);
	SCS_LOW();
	toggle_vcom();
}

void init_display(void) {
	// Step 1
	HAL_Delay(10);
	DISP_LOW();
	SCS_LOW();
	EXTCOMIN_LOW();
	// Step 2
	clearDisplay();
	HAL_Delay(50);
	// Step 3
	DISP_HIGH();
	// Step 4
}

unsigned int toggle_vcom(void) {
	SCS_HIGH();
	vcom_bit ^= 0x40;
	HAL_SPI_Transmit(&hspi1, &vcom_bit, 1, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi1, &dummy_8bit, 1, HAL_MAX_DELAY);
	SCS_LOW();
	return vcom_bit;
}

// Working perfectly, slowest SPI write method
void writeDisplayBuffer(void) {
	SCS_HIGH();
	RED_LED_ON();
	HAL_SPI_Transmit(&hspi1, &write_mode, 1, HAL_MAX_DELAY);

	for (uint8_t line = 1; line <= DISPLAY_HEIGHT; line++) {
	    // Send line address inverted
	    uint8_t line_address = (uint8_t)(__RBIT((uint8_t)(line)) >> 24);
	    HAL_SPI_Transmit(&hspi1, &line_address, 1, HAL_MAX_DELAY);

	    // Send pixel data for the line
	    HAL_SPI_Transmit(&hspi1, (uint8_t*)currentBuffer[line -1], 30, HAL_MAX_DELAY);

	    // Send 8 dummy bits after each line's pixel data
	    HAL_SPI_Transmit(&hspi1, &dummy_8bit, 1, HAL_MAX_DELAY);
	}
	HAL_SPI_Transmit(&hspi1, &dummy_16bit, 2, HAL_MAX_DELAY);

	SCS_LOW();
	RED_LED_OFF();
	toggle_vcom();
}

// This uses a more efficient SPI transfer method
unsigned int sendToDisplay(void) {
	uint8_t* sendBufferPtr = sendToDisplayBuffer;
	*sendBufferPtr++ = write_mode;

	for (uint8_t line = 1; line <= DISPLAY_HEIGHT; line++) {
	    // Send line address inverted
	    uint8_t line_address = (uint8_t)(__RBIT((uint8_t)(line)) >> 24);
	    *sendBufferPtr++ = line_address;

	    // Add pixel data from displayBuffer -> to improve this all the gfx code could modify only the bufferPtr/sendToDisplayBuffer
	    memcpy(sendBufferPtr, currentBuffer[line - 1], DISPLAY_WIDTH / 8);
	    sendBufferPtr += DISPLAY_WIDTH / 8;

	    // Send 8 dummy bits after each line's pixel data
	    *sendBufferPtr++ = dummy_8bit;
	}
	*sendBufferPtr++ = dummy_16bit;

	RED_LED_ON();
	SCS_HIGH();

	HAL_SPI_Transmit(&hspi1, sendToDisplayBuffer, TOTAL_DATA_SIZE, HAL_MAX_DELAY);
	SCS_LOW();
	//updateBuffer();
	//currentBuffer = (currentBuffer == frontBuffer) ? backBuffer : frontBuffer;
	initCurrentBuffer();

	RED_LED_OFF();

	int vcom_bit = toggle_vcom();
	return vcom_bit;
}

unsigned int updateDisplay(uint8_t x_start, uint8_t x_end) {
	uint8_t* sendBufferPtr = sendToDisplayBuffer;
	*sendBufferPtr++ = write_mode;

	for (uint8_t line = x_start; line <= x_end; line++) {
	    // Send line address inverted
	    uint8_t line_address = (uint8_t)(__RBIT((uint8_t)(line)) >> 24);
	    *sendBufferPtr++ = line_address;

	    // Add pixel data from displayBuffer -> to improve this all the gfx code could modify only the bufferPtr/sendToDisplayBuffer
	    memcpy(sendBufferPtr, currentBuffer[line - 1], DISPLAY_WIDTH / 8);
	    sendBufferPtr += DISPLAY_WIDTH / 8;

	    // Send 8 dummy bits after each line's pixel data
	    *sendBufferPtr++ = dummy_8bit;
	}
	*sendBufferPtr++ = dummy_16bit;

	RED_LED_ON();
	SCS_HIGH();

	HAL_SPI_Transmit(&hspi1, sendToDisplayBuffer, TOTAL_DATA_SIZE, HAL_MAX_DELAY);
	SCS_LOW();
	//updateBuffer();
	//currentBuffer = (currentBuffer == frontBuffer) ? backBuffer : frontBuffer;
	//initCurrentBuffer();
	resetCurrentBuffer(x_start, x_end);
	RED_LED_OFF();

	int vcom_bit = toggle_vcom();
	return vcom_bit;
}

// Should be even faster, unblocks the CPU from the transfer task
// !!! Unfinished function, needs interrupt and transfer handling !!!
void sendToDisplay_DMA(void) {

}

void fillSquare(int start_position_x, int start_position_y, int square_size, bool color) {
    // Loop over each row of the square
    for (int row = start_position_y - 1; row < start_position_y - 1 + square_size; row++) {
        // Check if row is within the display bounds
        if (row < 0 || row >= DISPLAY_HEIGHT) continue;

        // Loop over each column of the square
        for (int col = start_position_x - 1; col < start_position_x - 1 + square_size; col++) {
            // Check if col is within the display bounds
            if (col < 0 || col >= DISPLAY_WIDTH) continue;

            int byteIndex = col / 8;  // Find the byte in which the pixel resides
            int bitIndex = 7 - (col % 8);   // Find the position of the pixel within that byte

            if (color) {
				// Set the bit to draw a pixel (assuming 0 is the color for drawing)
            	currentBuffer[row][byteIndex] &= ~(1 << bitIndex);
			} else {
				// Clear the bit to erase a pixel (assuming 1 is the color for erasing)
				currentBuffer[row][byteIndex] |= (1 << bitIndex);
			}
        }
    }
}

void fillRectangle(int start_position_x, int start_position_y, int length_x, int length_y, bool color) {
	// Loop over each row of the square
	for (int row = start_position_y - 1; row < start_position_y - 1 + length_y; row++) {
		// Check if row is within the display bounds
		if (row < 0 || row >= DISPLAY_HEIGHT) continue;

		// Loop over each column of the square
		for (int col = start_position_x - 1; col < start_position_x - 1 + length_x; col++) {
			// Check if col is within the display bounds
			if (col < 0 || col >= DISPLAY_WIDTH) continue;

			int byteIndex = col / 8;  // Find the byte in which the pixel resides
			int bitIndex = 7 - (col % 8);   // Find the position of the pixel within that byte

			if (color) {
				// Set the bit to draw a pixel (assuming 0 is the color for drawing)
				currentBuffer[row][byteIndex] &= ~(1 << bitIndex);

			} else {
				// Clear the bit to erase a pixel (assuming 1 is the color for erasing)
				currentBuffer[row][byteIndex] |= (1 << bitIndex);
			}
		}
	}
}

void drawLine_H(int start_position_x, int start_position_y, int length, bool color) {
	for (int col = start_position_x - 1; col < start_position_x - 1 + length; col++) {
		if (col < 0 || col >= DISPLAY_WIDTH) continue;

		int byteIndex = col / 8;
		int bitIndex = 7 - (col % 8);

		if (color) {
			// Set the bit to draw a pixel (assuming 0 is the color for drawing)
			currentBuffer[start_position_y - 1][byteIndex] &= ~(1 << bitIndex);

		} else {
			// Clear the bit to erase a pixel (assuming 1 is the color for erasing)
			currentBuffer[start_position_y - 1][byteIndex] |= (1 << bitIndex);
		}
	}
}

void drawLine_V(int start_position_x, int start_position_y, int length, bool color) {
	for (int row = start_position_y - 1; row < start_position_y - 1 + length; row++) {
		if (row < 0 || row >= DISPLAY_HEIGHT) continue;

		int byteIndex = (start_position_x - 1) / 8;
		int bitIndex = 7 - ((start_position_x - 1) % 8);

		if (color) {
			// Set the bit to draw a pixel (assuming 0 is the color for drawing)
			currentBuffer[row][byteIndex] &= ~(1 << bitIndex);

		} else {
			// Clear the bit to erase a pixel (assuming 1 is the color for erasing)
			currentBuffer[row][byteIndex] |= (1 << bitIndex);
		}
	}
}

void drawLine(int x0, int y0, int x1, int y1, bool color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2; /* error value e_xy */
    for (;;) {  /* loop */
        setPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; } /* e_xy+e_x > 0 */
        if (e2 <= dx) { err += dx; y0 += sy; } /* e_xy+e_y < 0 */
    }
}

void setPixel(int x, int y, bool color) {
	int byteIndex = (x) / 8;
	int bitIndex = 7 - ((x) % 8);
	if (color) {
		// Set the bit to draw a pixel (assuming 0 is the color for drawing)
		currentBuffer[y][byteIndex] &= ~(1 << bitIndex);

	} else {
		// Clear the bit to erase a pixel (assuming 1 is the color for erasing)
		currentBuffer[y][byteIndex] |= (1 << bitIndex);
	}
}

void setPixel_BB(int x, int y, bool color) {
	int byteIndex = x >> 3; // byteIndex = x / 8
	int bitIndex = 7 - (x & 7); // bitIndex = 7 - (x % 8)

	// Calculate the byte offset within the buffer
	uint32_t byte_offset = (uint32_t)&currentBuffer[y][byteIndex] - SRAM_BASE;

	// Calculate the bit_word_offset and bit_band_alias_address
	uint32_t bit_word_offset = (byte_offset << 5) + (bitIndex << 2); // bit_word_offset = (byte_offset) * 32 + (bitIndex * 4)
	uint32_t bit_band_alias_address = SRAM_BB_BASE + bit_word_offset;

	// Use bit-banding to set or clear the bit
	*(volatile uint32_t *)bit_band_alias_address = (color ? 0 : 1);
}

void drawChar(int x, int y, char c, bool color) {
	// Get the index of the character in the font arrays
	int charIndex = c - 33; // Assuming '!' (char 33) is the first character in your font

	// Get the character width and bitmap address
	int width = char_width[charIndex];
	const char* bitmap = char_addr[charIndex];

	// Iterate over each vertical slice (column) in the character's bitmap
	for (int col = 0; col < width; col++) {
		int displayX = x + col;  // X position is based on the column (width)
		int bitmapColOffset = col; // Pre-calculate column offset in the bitmap

		int prevRowDivisionResult = -1;
		int prevRowDivisionResultTimesWidth = -1;

		// Iterate over each row in the character's bitmap
		for (int row = 0; row < 48; row++) { // Assuming 48 pixels in height
			int displayY = y + row;  // Y position is based on the row (height)

			// Optimize division and multiplication
			int rowDivisionResult = row >> 3;
			if (rowDivisionResult != prevRowDivisionResult) {
				prevRowDivisionResult = rowDivisionResult;
				prevRowDivisionResultTimesWidth = rowDivisionResult * width;
			}

			// Calculate the position in the bitmap array and the bit index
			int bitmapIndex = bitmapColOffset + prevRowDivisionResultTimesWidth;
			int bitIndex = row & 7;  // Bit index within the byte, assuming LSB to MSB ordering

			// Check if the pixel should be drawn (based on the bitmap data)
			if (bitmap[bitmapIndex] & (1 << bitIndex)) {
				setPixel_BB(displayX, displayY, color); // Draw the pixel
			}
		}
	}
}


void drawString(int x, int y, const char* str, bool color) {
    while (*str) {
        drawChar(x, y, *str, color);
        x += char_width[*str - 33] + 1; // Move x to the next character position
        str++; // Next character
    }
}

void numToString(int x, int y, int number, char *format, bool color) {
	char str[16];
	char *string_pointer = str;
	char finalFormat[8];
	snprintf(finalFormat, sizeof(finalFormat), "%%%s", format);
	sprintf(str, finalFormat, number);
	while (*string_pointer) {
        drawChar(x, y, *string_pointer, color);
        x += char_width[*string_pointer - 33] + 1; // Move x to the next character position
        string_pointer++; // Next character
    }
}

// Slow, needs optimization
void invertDisplayBuffer(void) {
	for (int row = 0; row < DISPLAY_HEIGHT; row++) {
	    for (int col = 0; col < DISPLAY_WIDTH / 8; col++) {
	    	currentBuffer[row][col] = ~currentBuffer[row][col];
	    }
	}
}

void initDisplayBuffer(void) {
	//memset(frontBuffer, 0xFF, DISPLAY_HEIGHT * (DISPLAY_WIDTH / 8));
	memset(currentBuffer, 0xFF, DISPLAY_HEIGHT * (DISPLAY_WIDTH / 8));
}

void initCurrentBuffer(void) {
	memset(currentBuffer, 0xFF, DISPLAY_HEIGHT * (DISPLAY_WIDTH / 8));
}

void resetCurrentBuffer(uint8_t x_start, uint8_t x_end) {
	for (int i = x_start; i <= x_end; i++) {
		memset(currentBuffer[i], 0xFF, sizeof(currentBuffer[i]));
	}
}

void drawSomething(void) {
	for (uint8_t row = 0; row < 240; row++) {
		for (uint8_t col = 100; col < 150; col++) {
			uint8_t byteIndex = col / 8;     // Find the byte in which the pixel resides
			uint8_t bitIndex = 7 - (col % 8); // Find the position of the pixel within that byte (7 - bitIndex due to big-endian bit order)

			// Clear the bit to draw a black pixel (assuming 0 is black and 1 is white)
			currentBuffer[row][byteIndex] &= ~(1 << bitIndex);
		}
	}
}

void drawCircle(int centerX, int centerY, int radius, bool color) {
    int x = radius;
    int y = 0;
    int decisionOver2 = 1 - x;   // Decision criterion divided by 2 evaluated at x=r, y=0

    while (y <= x) {
        setPixel(centerX + x, centerY + y, color); // Octant 1
        setPixel(centerX + y, centerY + x, color); // Octant 2
        setPixel(centerX - y, centerY + x, color); // Octant 3
        setPixel(centerX - x, centerY + y, color); // Octant 4
        setPixel(centerX - x, centerY - y, color); // Octant 5
        setPixel(centerX - y, centerY - x, color); // Octant 6
        setPixel(centerX + y, centerY - x, color); // Octant 7
        setPixel(centerX + x, centerY - y, color); // Octant 8
        y++;

        if (decisionOver2 <= 0) {
            decisionOver2 += 2 * y + 1;   // Change in decision criterion for y -> y+1
        } else {
            x--;
            decisionOver2 += 2 * (y - x) + 1;   // Change for y -> y+1, x -> x-1
        }
    }
}

void fillCircle(int centerX, int centerY, int radius, bool color) {
    int x = radius;
    int y = 0;
    int decisionOver2 = 1 - x;

    while (y <= x) {
    	drawLine_H(centerX - x, centerY + y, 2 * x + 1, color); // Span across circle at y from left to right
    	drawLine_H(centerX - y, centerY + x, 2 * y + 1, color); // Span across circle at x from left to right
    	drawLine_H(centerX - x, centerY - y, 2 * x + 1, color); // Span across circle at -y from left to right
    	drawLine_H(centerX - y, centerY - x, 2 * y + 1, color); // Span across circle at -x from left to right
        y++;

        if (decisionOver2 <= 0) {
            decisionOver2 += 2 * y + 1;
        } else {
            x--;
            decisionOver2 += 2 * (y - x) + 1;
        }
    }
}


