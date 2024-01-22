/*
 * sharp_memory_display.h
 *
 *  Created on: Jan 12, 2024
 *      Author: slst
 */

#ifndef INC_SHARP_MEMORY_DISPLAY_H_
#define INC_SHARP_MEMORY_DISPLAY_H_

#define DISPLAY_WIDTH	240
#define DISPLAY_HEIGHT	240

#define LINE_DATA_SIZE (1 /* line address */ + DISPLAY_WIDTH / 8 /* pixel data */ + 1 /* dummy bits */)
#define TOTAL_DATA_SIZE (1 /* write mode */ + DISPLAY_HEIGHT * LINE_DATA_SIZE + 2 /* final dummy bits */)

#include <stdbool.h>

#define DISP_PORT		GPIOA
#define DISP_PIN		GPIO_PIN_8
#define DISP_LOW()		HAL_GPIO_WritePin(DISP_PORT, DISP_PIN, GPIO_PIN_RESET)
#define DISP_HIGH()		HAL_GPIO_WritePin(DISP_PORT, DISP_PIN, GPIO_PIN_SET)

#define EXTCOMIN_PORT	GPIOA
#define EXTCOMIN_PIN	GPIO_PIN_6
#define EXTCOMIN_LOW()	HAL_GPIO_WritePin(EXTCOMIN_PORT, EXTCOMIN_PIN, GPIO_PIN_RESET)
#define EXTCOMIN_HIGH()	HAL_GPIO_WritePin(EXTCOMIN_PORT, EXTCOMIN_PIN, GPIO_PIN_SET)

#define SCS_PORT		GPIOA
#define SCS_PIN			GPIO_PIN_4
#define SCS_LOW()		HAL_GPIO_WritePin(SCS_PORT, SCS_PIN, GPIO_PIN_RESET)
#define SCS_HIGH()		HAL_GPIO_WritePin(SCS_PORT, SCS_PIN, GPIO_PIN_SET)

// Nucleo WB55 LEDs
#define BLUE_LED_ON()	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET)
#define BLUE_LED_OFF()	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET)
#define GREEN_LED_ON()	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET)
#define GREEN_LED_OFF() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET)
#define RED_LED_ON()	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET)
#define RED_LED_OFF()	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET)

// Optimizations
#define SRAM_BB_BASE 0x22000000

// Sharp memory display functions
void init_display(void);
void clearDisplay(void);
void toggle_vcom(void);
void writeDisplayBuffer(void);
void drawSomething(void);
void invertDisplayBuffer(void);
void initDisplayBuffer(void);
void initCurrentBuffer(void);
void sendToDisplay(void);
void sendToDisplay_DMA(void);

// GFX functions - will probably get their own header file
void fillSquare(int, int, int, bool);
void fillRectangle(int, int, int, int, bool);
void drawLine_H(int, int, int, bool);
void drawLine_V(int, int, int, bool);
void drawLine(int, int, int, int, bool);
void setPixel(int, int, bool);
void setPixel_BB(int x, int y, bool);
void drawCircle(int, int, int, bool);
void fillCircle(int, int, int, bool);
void drawChar(int x, int y, char c, bool color);
void drawChar_optimized(int x, int y, char c, bool color);
void drawString(int x, int y, const char* str, bool color);

// Debug, test and performance functions


#endif /* INC_SHARP_MEMORY_DISPLAY_H_ */
