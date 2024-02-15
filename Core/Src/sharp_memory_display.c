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
#include <stdio.h>
#include "watch_fonts.h"
#include <math.h>

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
Font* currentFont = NULL;


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
	vcom_bit ^= 0x40;
	uint8_t buffer[2] = {vcom_bit, 0x00};
	SCS_HIGH();
	HAL_SPI_Transmit(&hspi1, buffer, 2, HAL_MAX_DELAY);
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

unsigned int updateDisplay(uint8_t y_start, uint8_t y_end) {
	uint8_t* sendBufferPtr = sendToDisplayBuffer;
	*sendBufferPtr++ = write_mode;

	// 1 ms for the loop
	for (uint8_t line = y_start - 1; line <= y_end; line++) {
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
	HAL_SPI_Transmit(&hspi1, sendToDisplayBuffer, TOTAL_DATA_SIZE, HAL_MAX_DELAY); // 16 ms for the SPI transfer
	SCS_LOW();
	//updateBuffer();
	//currentBuffer = (currentBuffer == frontBuffer) ? backBuffer : frontBuffer;
	//initCurrentBuffer();

	resetCurrentBufferLines(y_start, y_end); // 1 ms
	RED_LED_OFF();

	int vcom_bit = toggle_vcom();
	return vcom_bit;
}

// Should be even faster, unblocks the CPU from the transfer task
// !!! Unfinished function, needs interrupt and transfer handling !!!
void sendToDisplay_DMA(void) {

}

void fillSquare(int start_position_x, int start_position_y, int square_size, bool color) {
	int col = start_position_x - 1;
	int lastBytePosition = (col + square_size) >> 3; // (col + square_size) / 8
	int lastBitPosition = 7 - ((col + square_size) & 7); // 7 - ((col + square_size) % 8
	int byteIndex = col >> 3; // byteIndex = x / 8
	int bitIndex = 7 - (col & 7); // bitIndex = 7 - (x % 8)
	int columnsToMemset = 0;
	uint8_t memsetColor = color ? 0x00 : 0xFF;

	if (lastBitPosition == 7) {
		columnsToMemset = lastBytePosition - byteIndex;
	} else {
		columnsToMemset = lastBytePosition - byteIndex - 1;
	}

    // Loop over each row of the square
    for (int row = start_position_y - 1; row < start_position_y + square_size; row++) {
        // Loop over each column of the square
        for (col = start_position_x - 1; col < start_position_x + square_size; col++) {
        	byteIndex = col >> 3; // byteIndex = x / 8
        	bitIndex = 7 - (col & 7); // bitIndex = 7 - (x % 8)

            if (bitIndex == 7 && byteIndex != lastBytePosition) {
            	memset(&currentBuffer[row][byteIndex], memsetColor, columnsToMemset);
            	col = col - 1 + (columnsToMemset * 8);
            	continue;
            }
            // Calculate the byte offset within the buffer
            uint32_t byte_offset = (uint32_t)&currentBuffer[row][byteIndex] - SRAM_BASE;

            // Calculate the bit_word_offset and bit_band_alias_address
            uint32_t bit_word_offset = (byte_offset << 5) + (bitIndex << 2); // bit_word_offset = (byte_offset) * 32 + (bitIndex * 4)
            uint32_t bit_band_alias_address = SRAM_BB_BASE + bit_word_offset;
            *(volatile uint32_t *)bit_band_alias_address = (color ? 0 : 1);
        }
    }
}

void fillRectangle(int start_position_x, int start_position_y, int length_x, int length_y, bool color) {
	int col = start_position_x - 1;
	int lastBytePosition = (col + length_x) >> 3; // (col + square_size) / 8
	int lastBitPosition = 7 - ((col + length_x) & 7); // 7 - ((col + square_size) % 8
	int byteIndex = col >> 3; // byteIndex = x / 8
	int bitIndex = 7 - (col & 7); // bitIndex = 7 - (x % 8)
	int columnsToMemset = 0;
	uint8_t memsetColor = color ? 0x00 : 0xFF;

	if (lastBitPosition == 7) {
		columnsToMemset = lastBytePosition - byteIndex;
	} else {
		columnsToMemset = lastBytePosition - byteIndex - 1;
	}

	for (int row = start_position_y - 1; row < start_position_y + length_y; row++) {

		// Loop over each column of the square
		for (col = start_position_x - 1; col < start_position_x + length_x; col++) {
			byteIndex = col >> 3; // byteIndex = x / 8
			bitIndex = 7 - (col & 7); // bitIndex = 7 - (x % 8)

			if (bitIndex == 7 && byteIndex != lastBytePosition) {
				memset(&currentBuffer[row][byteIndex], memsetColor, columnsToMemset);
				col = col - 1 + (columnsToMemset * 8);
				continue;
			}

			// Calculate the byte offset within the buffer
			uint32_t byte_offset = (uint32_t)&currentBuffer[row][byteIndex] - SRAM_BASE;

			// Calculate the bit_word_offset and bit_band_alias_address
			uint32_t bit_word_offset = (byte_offset << 5) + (bitIndex << 2); // bit_word_offset = (byte_offset) * 32 + (bitIndex * 4)
			uint32_t bit_band_alias_address = SRAM_BB_BASE + bit_word_offset;
			*(volatile uint32_t *)bit_band_alias_address = (color ? 0 : 1);
		}
	}
}

// Add thickness calculation to loop
void drawRectangle(int start_position_x, int start_position_y, int length_x, int length_y, int thickness, bool color) {
	for (int i = 0; i < thickness; i++) {
		drawLine_H(start_position_x + i, start_position_y + i, length_x - (i * 2), color); // Top H line
		drawLine_H(start_position_x + i, start_position_y + length_y - i, length_x - (i * 2), color); // Bottom H line
		drawLine_V(start_position_x + i, start_position_y + 1 + i, length_y - 1 - (i * 2), color); // Left V line
		drawLine_V(start_position_x + length_x - i, start_position_y + 1 + i, length_y - 1 - (i * 2), color); // Right V line
	}
}

void drawLine_H(int start_position_x, int start_position_y, int length, bool color) {
	int col = start_position_x - 1;
	uint8_t memsetColor = color ? 0x00 : 0xFF;
	int lastBytePosition = (col + length) >> 3; // (col + length) / 8
	//int lastBitPosition = 7 - ((col + length) & 7); // 7 - ((col + length) % 8
	int byteIndex = col >> 3; // byteIndex = x / 8
	int bitIndex = 7 - (col & 7); // bitIndex = 7 - (x % 8)
	int columnsToMemset = 0;

	if (bitIndex == 7) {
		columnsToMemset = lastBytePosition - byteIndex;
	} else {
		columnsToMemset = lastBytePosition - byteIndex - 1;
	}

	for (col = start_position_x - 1; col < start_position_x + length; col++) {
		byteIndex = col >> 3; // byteIndex = x / 8
		bitIndex = 7 - (col & 7); // bitIndex = 7 - (x % 8)

		if (bitIndex == 7 && byteIndex < lastBytePosition) {
			memset(&currentBuffer[start_position_y - 1][byteIndex], memsetColor, columnsToMemset);
			col = col - 1 + (columnsToMemset * 8);
			continue;
		}
		// Calculate the byte offset within the buffer
		uint32_t byte_offset = (uint32_t)&currentBuffer[start_position_y - 1][byteIndex] - SRAM_BASE;

		// Calculate the bit_word_offset and bit_band_alias_address
		uint32_t bit_word_offset = (byte_offset << 5) + (bitIndex << 2); // bit_word_offset = (byte_offset) * 32 + (bitIndex * 4)
		uint32_t bit_band_alias_address = SRAM_BB_BASE + bit_word_offset;
		*(volatile uint32_t *)bit_band_alias_address = (color ? 0 : 1);
	}
}

// Faster for a line < 16 px
void drawShortLine_H(int start_position_x, int start_position_y, int length, bool color) {
	for (int col = start_position_x - 1; col < start_position_x + length; col++) {
		int byteIndex = col >> 3; // byteIndex = x / 8
		int bitIndex = 7 - (col & 7); // bitIndex = 7 - (x % 8)

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
	for (int row = start_position_y - 1; row < start_position_y + length; row++) {
		int byteIndex = (start_position_x - 1) >> 3; // byteIndex = x / 8
		int bitIndex = 7 - ((start_position_x - 1) & 7); // bitIndex = 7 - (x % 8)

		// Calculate the byte offset within the buffer
		uint32_t byte_offset = (uint32_t)&currentBuffer[row][byteIndex] - SRAM_BASE;

		// Calculate the bit_word_offset and bit_band_alias_address
		uint32_t bit_word_offset = (byte_offset << 5) + (bitIndex << 2); // bit_word_offset = (byte_offset) * 32 + (bitIndex * 4)
		uint32_t bit_band_alias_address = SRAM_BB_BASE + bit_word_offset;
		*(volatile uint32_t *)bit_band_alias_address = (color ? 0 : 1);
	}
}

void drawLine(int x0, int y0, int x1, int y1, bool color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2; /* error value e_xy */
    for (;;) {  /* loop */
        setPixel_BB(x0, y0, color);
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
	//int charIndex = c - 33; // Assuming '!' (char 33) is the first character in your font
	int charIndex = c - currentFont->char_index;

	// Get the character width and bitmap address
	//int width = char_width[charIndex];
	int width = currentFont->char_width[charIndex];
	//const char* bitmap = char_addr[charIndex];
	const char* bitmap = currentFont->char_addr[charIndex];

	// Iterate over each vertical slice (column) in the character's bitmap
	for (int col = 0; col < width; col++) {
		int displayX = x + col;  // X position is based on the column (width)

		int prevRowDivisionResult = -1;
		int prevRowDivisionResultTimesWidth = -1;

		// Iterate over each row in the character's bitmap
		for (int row = 0; row < currentFont->font_height; row++) {
			int displayY = y + row;  // Y position is based on the row (height)

			// Optimize division and multiplication
			int rowDivisionResult = row >> 3;
			if (rowDivisionResult != prevRowDivisionResult) {
				prevRowDivisionResult = rowDivisionResult;
				prevRowDivisionResultTimesWidth = rowDivisionResult * width;
			}

			// Calculate the position in the bitmap array and the bit index
			int bitmapIndex = col + prevRowDivisionResultTimesWidth;
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
        //x += char_width[*str - 33] + 1; // Move x to the next character position
        x += currentFont->char_width[*str - currentFont->char_index] + 1;
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
        //x += char_width[*string_pointer - 33] + 1; // Move x to the next character position
        x += currentFont->char_width[*str - 33] + 1;
        string_pointer++; // Next character
    }
}

void drawThickLine(int x0, int y0, int x1, int y1, int thickness, bool color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int len = round(sqrt(dx * dx + dy * dy));
    int ux = (thickness * dy) >> len; // equivalent to thickness * dy / len
    int uy = (thickness * dx) >> len; // equivalent to thickness * dx / len

    int x2 = x0 - ux;
    int y2 = y0 + uy;
    int x3 = x1 - ux;
    int y3 = y1 + uy;
    int x4 = x1 + ux;
    int y4 = y1 - uy;
    int x5 = x0 + ux;
    int y5 = y0 - uy;

    drawFilledPolygon(x2, y2, x3, y3, x4, y4, x5, y5, color);
}

void drawFilledPolygon(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, bool color) {
    // Sort the vertices by y-coordinate
    int yMin = min(min(y1, y2), min(y3, y4));
    int yMax = max(max(y1, y2), max(y3, y4));

    for (int y = yMin; y <= yMax; y++) {
        // Find the intersections of the scanline with all sides of the polygon
        int xMin = INT32_MAX, xMax = INT32_MIN;
        if (y >= min(y1, y2) && y <= max(y1, y2))
            xMin = min(xMin, x1 + (int)((float)(y - y1) * (x2 - x1) / (y2 - y1)));
        if (y >= min(y3, y4) && y <= max(y3, y4))
            xMin = min(xMin, x3 + (int)((float)(y - y3) * (x4 - x3) / (y4 - y3)));
        if (y >= min(y1, y4) && y <= max(y1, y4))
            xMax = max(xMax, x1 + (int)((float)(y - y1) * (x4 - x1) / (y4 - y1)));
        if (y >= min(y2, y3) && y <= max(y2, y3))
            xMax = max(xMax, x2 + (int)((float)(y - y2) * (x3 - x2) / (y3 - y2)));

        // Draw a horizontal line from xMin to xMax
        drawLine_H(xMin, y, xMax - xMin, color);
    }
}

void drawPolygon(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, bool color) {
    // Array to hold the vertices
    int vertices[4][2] = {{x1, y1}, {x2, y2}, {x3, y3}, {x4, y4}};

    // Sort the vertices by y-coordinate, then by x-coordinate
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            if (vertices[j][1] < vertices[i][1] || (vertices[j][1] == vertices[i][1] && vertices[j][0] < vertices[i][0])) {
                // Swap vertices[i] and vertices[j]
                int tempX = vertices[i][0];
                int tempY = vertices[i][1];
                vertices[i][0] = vertices[j][0];
                vertices[i][1] = vertices[j][1];
                vertices[j][0] = tempX;
                vertices[j][1] = tempY;
            }
        }
    }

    // Draw the lines - point to point line is not always closed
    drawLine(vertices[0][0], vertices[0][1], vertices[1][0], vertices[1][1], color); // Line from v1 to v2
    drawLine(vertices[0][0], vertices[0][1] + 1, vertices[2][0], vertices[2][1], color); // Line from v1 to v3
    drawLine(vertices[1][0], vertices[1][1], vertices[3][0], vertices[3][1], color); // Line from v2 to v4
    drawLine(vertices[2][0] + 1, vertices[2][1], vertices[3][0] - 1, vertices[3][1], color); // Line from v3 to v4
}

//32 bit optimized, 3 ms instead of 20 ms (6.5x speedup)
void invertDisplayBuffer(void) {
    uint32_t* buffer32 = (uint32_t*)currentBuffer;
    for (int i = 0; i < (DISPLAY_HEIGHT * DISPLAY_WIDTH / 8) / 4; i++) {
        buffer32[i] = ~buffer32[i];
    }
}

// Slow, needs optimization
/*void invertDisplayBuffer(void) {
	for (int row = 0; row < DISPLAY_HEIGHT; row++) {
	    for (int col = 0; col < DISPLAY_WIDTH / 8; col++) {
	    	currentBuffer[row][col] = ~currentBuffer[row][col];
	    }
	}
}*/

void initDisplayBuffer(void) {
	//memset(frontBuffer, 0xFF, DISPLAY_HEIGHT * (DISPLAY_WIDTH / 8));
	memset(currentBuffer, 0xFF, DISPLAY_HEIGHT * (DISPLAY_WIDTH / 8));
}

void initCurrentBuffer(void) {
	memset(currentBuffer, 0xFF, DISPLAY_HEIGHT * (DISPLAY_WIDTH / 8));
}

void resetCurrentBufferLines(uint8_t y_start, uint8_t y_end) {
	for (int i = y_start; i <= y_end; i++) {
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
        setPixel_BB(centerX + x, centerY + y, color); // Octant 1
        setPixel_BB(centerX + y, centerY + x, color); // Octant 2
        setPixel_BB(centerX - y, centerY + x, color); // Octant 3
        setPixel_BB(centerX - x, centerY + y, color); // Octant 4
        setPixel_BB(centerX - x, centerY - y, color); // Octant 5
        setPixel_BB(centerX - y, centerY - x, color); // Octant 6
        setPixel_BB(centerX + y, centerY - x, color); // Octant 7
        setPixel_BB(centerX + x, centerY - y, color); // Octant 8
        y++;

        if (decisionOver2 <= 0) {
            decisionOver2 += 2 * y + 1;   // Change in decision criterion for y -> y+1
        } else {
            x--;
            decisionOver2 += 2 * (y - x) + 1;   // Change for y -> y+1, x -> x-1
        }
    }
}

void drawThickCircle(int centerX, int centerY, int radius, int thickness, bool color) {
    for (int r = radius; r < radius + thickness; r++) {
        int x = r;
        int y = 0;
        int decisionOver2 = 1 - x;   // Decision criterion divided by 2 evaluated at x=r, y=0

        while (y <= x) {
            setPixel_BB(centerX + x, centerY + y, color); // Octant 1
            setPixel_BB(centerX + y, centerY + x, color); // Octant 2
            setPixel_BB(centerX - y, centerY + x, color); // Octant 3
            setPixel_BB(centerX - x, centerY + y, color); // Octant 4
            setPixel_BB(centerX - x, centerY - y, color); // Octant 5
            setPixel_BB(centerX - y, centerY - x, color); // Octant 6
            setPixel_BB(centerX + y, centerY - x, color); // Octant 7
            setPixel_BB(centerX + x, centerY - y, color); // Octant 8
            y++;

            if (decisionOver2 <= 0) {
                decisionOver2 += 2 * y + 1;   // Change in decision criterion for y -> y+1
            } else {
                x--;
                decisionOver2 += 2 * (y - x) + 1;   // Change for y -> y+1, x -> x-1
            }
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


