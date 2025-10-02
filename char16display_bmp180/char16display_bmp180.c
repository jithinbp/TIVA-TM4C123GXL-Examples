/******************************************************************************
 * Project  : 16 character LCD display for BMP180 readings
 * File     : char16display_bmp180.c
 *
 * Description:
 * Example code for displaying Pressure (P) and Temperature (T) readings from
 * the BMP180 barometric sensor on a 16x2 HD44780-compatible character LCD
 * in 4-bit mode.
 *
 * I2C Pin Connections for TM4C123G (using I2C0 module):
 * - SCL (BMP180) -> Tiva C **PB2**
 * - SDA (BMP180) -> Tiva C **PB3**
 *
 * Author   : Jithin B.P.
 * Affiliation: CSpark Research
 * Email    : jithinuser@gmail.com
 *
 * License  : This source code is released under an open-source license.
 * You may use, modify, and distribute it freely, provided that
 * proper attribution is given to the original author.
 *
 * Created  : 2025
 ******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h> // Required for floating point math
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/i2c.h" // New header for I2C
#include "driverlib/pin_map.h"
#include "driverlib/adc.h" // Keeping original headers just in case, but ADC is not used

void ADCSeq0Handler(void) {}

// ------------ LCD pins (4-bit mode) ------------
// RS = PA6, EN = PA7, D4-D7 = PD0..PD3
#define LCD_RS_PORT GPIO_PORTA_BASE
#define LCD_RS_PIN  GPIO_PIN_6
#define LCD_EN_PORT GPIO_PORTA_BASE
#define LCD_EN_PIN  GPIO_PIN_7
#define LCD_DATA_PORT GPIO_PORTD_BASE
#define LCD_DMASK   (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)

// ------------ BMP180 I2C Definitions ------------
#define BMP180_I2C_ADDRESS  0x77
#define BMP180_BASE         I2C0_BASE
#define BMP180_OSS          3           // Oversampling setting (0 to 3)

// Register addresses
#define REG_CALIB_START     0xAA
#define REG_CONTROL         0xF4
#define REG_MSB             0xF6
#define REG_LSB             0xF7
#define REG_XLSB            0xF8

// Control register commands
#define CMD_READ_TEMP       0x2E
#define CMD_READ_PRESSURE   (0x34 + (BMP180_OSS << 6))

// Structure to hold 11 calibration coefficients
typedef struct {
    int16_t AC1;
    int16_t AC2;
    int16_t AC3;
    uint16_t AC4;
    uint16_t AC5;
    uint16_t AC6;
    int16_t B1;
    int16_t B2;
    int16_t MB;
    int16_t MC;
    int16_t MD;
} bmp180_calib_data_t;

bmp180_calib_data_t calib;

// Global B5 value for pressure calculation (calculated during temperature compensation)
int32_t B5;

// --- Small delay helpers ---
void delay_ms(uint32_t ms) {
    SysCtlDelay((SysCtlClockGet()/3000) * ms); // approx ms
}
void delay_us(uint32_t us) {
    SysCtlDelay((SysCtlClockGet()/3000000) * us); // approx us
}

// ===============================================
//          LCD Functions (Retained)
// ===============================================

void LCD_pulseEnable(void) {
    GPIOPinWrite(LCD_EN_PORT, LCD_EN_PIN, LCD_EN_PIN);
    delay_us(1);
    GPIOPinWrite(LCD_EN_PORT, LCD_EN_PIN, 0);
    delay_us(100);
}

void LCD_write4bits(uint8_t value) {
    // Write value to PD0-PD3
    GPIOPinWrite(LCD_DATA_PORT, LCD_DMASK, (value & 0x0F));
    LCD_pulseEnable();
}

void LCD_command(uint8_t cmd) {
    GPIOPinWrite(LCD_RS_PORT, LCD_RS_PIN, 0); // RS=0 (Command)
    LCD_write4bits(cmd >> 4);
    LCD_write4bits(cmd & 0x0F);
    delay_ms(2);
}

void LCD_data(uint8_t data) {
    GPIOPinWrite(LCD_RS_PORT, LCD_RS_PIN, LCD_RS_PIN); // RS=1 (Data)
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
    GPIOPinTypeGPIOOutput(LCD_RS_PORT, LCD_EN_PIN | LCD_RS_PIN);
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

void LCD_setCursor(uint8_t row, uint8_t col) {
    // DDRAM offsets for a 16x2 LCD
    static const uint8_t row_offsets[] = {0x00, 0x40}; 
    
    // Check for standard 16x2 (only use rows 0 and 1)
    if (row > 1) row = 1;
    if (col > 15) col = 15; 
    
    // Set DDRAM address command (0x80 is the Set DDRAM Address instruction)
    LCD_command(0x80 | (row_offsets[row] + col));
}

void LCD_print(char *s) {
    while(*s) {
        LCD_data(*s++);
    }
}

// ===============================================
//          I2C Functions (New)
// ===============================================

// Generic function to write a command to a register
void I2C_write(uint8_t reg, uint8_t data) {
    // Specify the slave address
    I2CMasterSlaveAddrSet(BMP180_BASE, BMP180_I2C_ADDRESS, false); 
    
    // Tell the master to write the register address
    I2CMasterDataPut(BMP180_BASE, reg);
    I2CMasterControl(BMP180_BASE, I2C_MASTER_CMD_BURST_SEND_START);
    while(I2CMasterBusy(BMP180_BASE));

    // Send the data
    I2CMasterDataPut(BMP180_BASE, data);
    I2CMasterControl(BMP180_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
    while(I2CMasterBusy(BMP180_BASE));
}

// Generic function to read one or more bytes starting from a register
uint32_t I2C_read_multiple(uint8_t reg, uint8_t count, uint8_t *data) {
    if (count == 0) return 0;
    
    // 1. Send register address (write mode)
    I2CMasterSlaveAddrSet(BMP180_BASE, BMP180_I2C_ADDRESS, false); // false = transmit
    I2CMasterDataPut(BMP180_BASE, reg);
    I2CMasterControl(BMP180_BASE, I2C_MASTER_CMD_SINGLE_SEND);
    while(I2CMasterBusy(BMP180_BASE));

    // 2. Switch to read mode
    I2CMasterSlaveAddrSet(BMP180_BASE, BMP180_I2C_ADDRESS, true); // true = receive

    if (count == 1) {
        // Single byte read
        I2CMasterControl(BMP180_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);
        while(I2CMasterBusy(BMP180_BASE));
        *data = I2CMasterDataGet(BMP180_BASE);
        return 1;
    } else {
        // Multi-byte read (first byte needs special start command)
        I2CMasterControl(BMP180_BASE, I2C_MASTER_CMD_BURST_RECEIVE_START);
        while(I2CMasterBusy(BMP180_BASE));
        *data++ = I2CMasterDataGet(BMP180_BASE);
        count--;

        while (count > 1) {
            // Middle bytes (continue)
            I2CMasterControl(BMP180_BASE, I2C_MASTER_CMD_BURST_RECEIVE_CONT);
            while(I2CMasterBusy(BMP180_BASE));
            *data++ = I2CMasterDataGet(BMP180_BASE);
            count--;
        }
        
        // Last byte (finish)
        I2CMasterControl(BMP180_BASE, I2C_MASTER_CMD_BURST_RECEIVE_FINISH);
        while(I2CMasterBusy(BMP180_BASE));
        *data = I2CMasterDataGet(BMP180_BASE);
        return 1; // Success
    }
}

// Reads 2 bytes (MSB, LSB) from the BMP180
int16_t I2C_read_int16(uint8_t reg) {
    uint8_t data[2];
    I2C_read_multiple(reg, 2, data);
    // BMP180 is big-endian: MSB is first
    return (int16_t)((data[0] << 8) | data[1]);
}

// Reads 3 bytes (MSB, LSB, XLSB) for uncompensated pressure
uint32_t I2C_read_UP(uint8_t reg) {
    uint8_t data[3];
    I2C_read_multiple(reg, 3, data);
    // UP is 19-bit value: (MSB << 16) | (LSB << 8) | (XLSB) >> (8 - OSS)
    return (uint32_t)(((data[0] << 16) | (data[1] << 8) | data[2]) >> (8 - BMP180_OSS));
}

void I2C_init(void) {
    // Enable I2C0 and GPIOB peripherals
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    
    // Wait for the peripherals to be ready
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_I2C0));
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB));

    // PB2 (SCL), PB3 (SDA)
    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
    GPIOPinConfigure(GPIO_PB3_I2C0SDA);
    
    // Set PB2 and PB3 for I2C function
    GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
    GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);
    
    // Initialize I2C0 master module
    I2CMasterInitExpClk(BMP180_BASE, SysCtlClockGet(), false); // false = 100kbps (standard mode)
}

// ===============================================
//          BMP180 Logic (New)
// ===============================================

void BMP180_read_calib_data(void) {
    uint8_t buffer[22];
    
    // Read 22 bytes starting from 0xAA (AC1_MSB)
    I2C_read_multiple(REG_CALIB_START, 22, buffer);

    // Parse the big-endian data into the structure (11 x 16-bit values)
    calib.AC1 = (buffer[0] << 8) | buffer[1];
    calib.AC2 = (buffer[2] << 8) | buffer[3];
    calib.AC3 = (buffer[4] << 8) | buffer[5];
    calib.AC4 = (buffer[6] << 8) | buffer[7];
    calib.AC5 = (buffer[8] << 8) | buffer[9];
    calib.AC6 = (buffer[10] << 8) | buffer[11];
    calib.B1  = (buffer[12] << 8) | buffer[13];
    calib.B2  = (buffer[14] << 8) | buffer[15];
    calib.MB  = (buffer[16] << 8) | buffer[17];
    calib.MC  = (buffer[18] << 8) | buffer[19];
    calib.MD  = (buffer[20] << 8) | buffer[21];
}

// Reads uncompensated temperature (UT)
int32_t BMP180_read_UT(void) {
    I2C_write(REG_CONTROL, CMD_READ_TEMP);
    delay_ms(5); // Wait 4.5ms for conversion
    return I2C_read_int16(REG_MSB);
}

// Reads uncompensated pressure (UP)
int32_t BMP180_read_UP(void) {
    I2C_write(REG_CONTROL, CMD_READ_PRESSURE);
    // Wait time depends on OSS setting
    // OSS=0: 4.5ms, OSS=1: 7.5ms, OSS=2: 13.5ms, OSS=3: 25.5ms
    delay_ms(28); 
    return I2C_read_UP(REG_MSB);
}

// BMP180 Compensation Algorithm (Calculates B5, Temp, and Pressure)
// Reference: BMP180 datasheet
void BMP180_compensate(int32_t UT, int32_t UP, float *temperature, float *pressure) {
    int32_t X1, X2, X3, B3, B6, P;
    uint32_t B4, B7;
    int32_t T;

    // ----------------- Step 1: Calculate B5 & True Temperature -----------------
    X1 = ((int32_t)UT - calib.AC6) * calib.AC5 / pow(2, 15);
    X2 = ((int32_t)calib.MC * pow(2, 11)) / (X1 + calib.MD);
    B5 = X1 + X2;
    
    T = (B5 + 8) / pow(2, 4);
    *temperature = (float)T / 10.0f; // Temperature in degrees C

    // ----------------- Step 2: Calculate True Pressure -----------------
    B6 = B5 - 4000;
    X1 = (calib.B2 * (B6 * B6 / pow(2, 12))) / pow(2, 11);
    X2 = (calib.AC2 * B6) / pow(2, 11);
    X3 = X1 + X2;

    B3 = ((((int32_t)calib.AC1 * 4 + X3) << BMP180_OSS) + 2) / 4;
    
    X1 = (calib.AC3 * B6) / pow(2, 13);
    X2 = (calib.B1 * (B6 * B6 / pow(2, 12))) / pow(2, 16);
    X3 = ((X1 + X2) + 2) / pow(2, 2);

    B4 = (uint32_t)calib.AC4 * (uint32_t)(X3 + pow(2, 15)) / pow(2, 15);

    B7 = ((uint32_t)UP - B3) * (50000 >> BMP180_OSS);

    if (B7 < 0x80000000) {
        P = (B7 * 2) / B4;
    } else {
        P = (B7 / B4) * 2;
    }

    X1 = (P / pow(2, 8)) * (P / pow(2, 8));
    X1 = (X1 * 3038) / pow(2, 16);
    X2 = (-7357 * P) / pow(2, 16);

    *pressure = (float)(P + (X1 + X2 + 3791) / pow(2, 4)); // Pressure in Pa
}


// ===============================================
//          Main Loop (Modified)
// ===============================================

int main(void) {
    char buffer[17];
    int32_t uncomp_T, uncomp_P;
    float temperature, pressure;

    // Set up system clock (40 MHz)
    SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL |
                   SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    // Initialize LCD
    LCD_init();
    delay_ms(100);

    // Initialize I2C
    I2C_init();
    
    // Read Calibration Data from BMP180 EEPROM
    BMP180_read_calib_data();

    // Initial Display Message
    LCD_command(0x01); // Clear
    LCD_setCursor(0,0);
    LCD_print("BMP180 Sensor");
    LCD_setCursor(1,0);
    LCD_print("Initializing...");

    delay_ms(2000);
    LCD_command(0x01); // Clear

    while(1)
    {
        // 1. Read Uncompensated Temperature
        uncomp_T = BMP180_read_UT();
        
        // 2. Read Uncompensated Pressure
        uncomp_P = BMP180_read_UP();

        // 3. Compensate and Calculate P and T
        BMP180_compensate(uncomp_T, uncomp_P, &temperature, &pressure);

        // 4. Display Temperature (Row 0)
        LCD_setCursor(0,0);
        // Format: T: XX.XC (e.g., T: 25.4C)
        // pressure is in Pa, convert to hPa (mbar) by dividing by 100
        snprintf(buffer, 17, "T: %.1f C        ", temperature);
        LCD_print(buffer);

        // 5. Display Pressure (Row 1)
        LCD_setCursor(1,0);
        // Format: P: XXXX.X hPa (e.g., P: 1013.2 hPa)
        snprintf(buffer, 17, "P: %.1f hPa  ", pressure / 100.0f); 
        LCD_print(buffer);

        delay_ms(1000); // Update display every 1 second
    }
    
    // Should never reach here
    // return 0;
}
