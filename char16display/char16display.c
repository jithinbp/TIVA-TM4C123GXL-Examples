/******************************************************************************
 * Project  : 16 character LCD display
 * File     : char16display.c
 *
 * Description:
 *   Example code for displaying text
 *   on a 16x2 HD44780-compatible character LCD in 4-bit mode.
 *
 * Author   : Jithin B.P.
 * Affiliation: CSpark Research
 * Email    : jithinuser@gmail.com
 *
 * License  : This source code is released under an open-source license.
 *            You may use, modify, and distribute it freely, provided that
 *            proper attribution is given to the original author.
 *
 * Created  : 2025
 ******************************************************************************/
//https://deepbluembedded.com/lcd-custom-character-arduino/
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"

// ------------ LCD pins (4-bit mode) ------------
// RS = PA6, EN = PA7, D4-D7 = PD0..PD3
#define LCD_RS_PORT GPIO_PORTA_BASE
#define LCD_RS_PIN  GPIO_PIN_6
#define LCD_EN_PORT GPIO_PORTA_BASE
#define LCD_EN_PIN  GPIO_PIN_7
#define LCD_DATA_PORT GPIO_PORTD_BASE
#define LCD_DMASK   (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)

void ADCSeq0Handler(void) {}

// --- Small delay helpers ---
void delay_ms(uint32_t ms) {
    SysCtlDelay((SysCtlClockGet()/3000) * ms); // approx ms
}
void delay_us(uint32_t us) {
    SysCtlDelay((SysCtlClockGet()/3000000) * us); // approx us
}

// ------------ LCD low-level functions ------------
void LCD_pulseEnable(void) {
    GPIOPinWrite(LCD_EN_PORT, LCD_EN_PIN, LCD_EN_PIN);
    delay_us(1);
    GPIOPinWrite(LCD_EN_PORT, LCD_EN_PIN, 0);
    delay_us(100);
}

void LCD_write4bits(uint8_t value) {
    GPIOPinWrite(LCD_DATA_PORT, LCD_DMASK, (value & 0x0F));
    LCD_pulseEnable();
}

void LCD_command(uint8_t cmd) {
    GPIOPinWrite(LCD_RS_PORT, LCD_RS_PIN, 0); // RS=0
    LCD_write4bits(cmd >> 4);
    LCD_write4bits(cmd & 0x0F);
    delay_ms(2);
}

void LCD_data(uint8_t data) {
    GPIOPinWrite(LCD_RS_PORT, LCD_RS_PIN, LCD_RS_PIN); // RS=1
    LCD_write4bits(data >> 4);
    LCD_write4bits(data & 0x0F);
    delay_ms(2);
}

void LCD_init(void) {
    // Enable ports
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOD));

    // RS, EN as outputs
    GPIOPinTypeGPIOOutput(LCD_RS_PORT, LCD_RS_PIN | LCD_EN_PIN);
    // D4-D7 as outputs
    GPIOPinTypeGPIOOutput(LCD_DATA_PORT, LCD_DMASK);

    delay_ms(50); // wait LCD power-up

    // 4-bit mode init sequence
    LCD_write4bits(0x03);
    delay_ms(5);
    LCD_write4bits(0x03);
    delay_us(150);
    LCD_write4bits(0x03);
    LCD_write4bits(0x02); // Set 4-bit mode

    LCD_command(0x28); // 4-bit, 2 line, 5x8 font
    LCD_command(0x0C); // Display ON, cursor off
    LCD_command(0x06); // Entry mode: auto increment
    LCD_command(0x01); // Clear
    delay_ms(2);
}

void LCD_setCursor(uint8_t col, uint8_t row) {
    uint8_t row_offsets[] = {0x00, 0x40};
    LCD_command(0x80 | (col + row_offsets[row]));
}

void LCD_print(char *s) {
    while(*s) {
        LCD_data(*s++);
    }
}


// ------------ Main ------------
int main(void) {
    // 40 MHz clock
    SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL |
                   SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    // LCD
    LCD_init();

    LCD_command(0x01); // Clear
    LCD_setCursor(0,0);
    LCD_print("HELLO THERE");
    LCD_setCursor(0,1);
    LCD_print("DISPLAY WORKS");
}
