/******************************************************************************
 * Project  : Ultrasonic Distance Measurement (HC-SR04 + UART)
 * File     : sr04.c
 *
 * Description:
 *   Example code for measuring distance using an HC-SR04 ultrasonic sensor
 *   on the TI Tiva C (TM4C123) microcontroller, and displaying the results
 *   over UART 8N1
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
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_timer.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "driverlib/pin_map.h"

#define TRIG_PORT   GPIO_PORTB_BASE
#define TRIG_PIN    GPIO_PIN_2        // PB2 as Trigger
#define ECHO_PORT   GPIO_PORTB_BASE
#define ECHO_PIN    GPIO_PIN_3        // PB3 as Echo

// Speed of sound: 343 m/s = 0.0343 cm/us
#define SOUND_SPEED_CM_PER_US 0.0343

void ADCSeq0Handler(){}

// UART configuration
void ConfigureUART(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);
    UARTStdioConfig(0, 115200, 16000000);
}

// Initialize GPIO and Timer
void SR04Init(void)
{
    // Enable GPIOB
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

    // Trigger pin as output
    GPIOPinTypeGPIOOutput(TRIG_PORT, TRIG_PIN);

    // Echo pin as input
    GPIOPinTypeGPIOInput(ECHO_PORT, ECHO_PIN);

    // Timer0 as free-running 16-bit up-counter (to measure echo pulse width)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_ONE_SHOT_UP);
}

uint32_t SR04GetDistanceCM(void)
{
    uint32_t startTime, endTime, pulseWidth;
    float distance;

    // Trigger pulse
    GPIOPinWrite(TRIG_PORT, TRIG_PIN, 0);
    SysCtlDelay(SysCtlClockGet() / 300000); // ~3us
    GPIOPinWrite(TRIG_PORT, TRIG_PIN, TRIG_PIN);
    SysCtlDelay(SysCtlClockGet() / 100000); // ~10us
    GPIOPinWrite(TRIG_PORT, TRIG_PIN, 0);

    // Wait for echo HIGH
    while(GPIOPinRead(ECHO_PORT, ECHO_PIN) == 0);

    // Setup timer
    TimerConfigure(TIMER0_BASE, TIMER_CFG_ONE_SHOT);
    TimerLoadSet(TIMER0_BASE, TIMER_A, 0xFFFFFFFF);
    TimerEnable(TIMER0_BASE, TIMER_A);

    startTime = TimerValueGet(TIMER0_BASE, TIMER_A);

    // Wait for echo LOW
    while(GPIOPinRead(ECHO_PORT, ECHO_PIN) != 0);

    endTime = TimerValueGet(TIMER0_BASE, TIMER_A);
    TimerDisable(TIMER0_BASE, TIMER_A);

    // Timer counts down → pulse width = start - end
    pulseWidth = startTime - endTime;

    // Convert to µs
    float time_us = (float)pulseWidth / (SysCtlClockGet() / 1000000.0);

    // Distance in cm
    distance = (time_us * SOUND_SPEED_CM_PER_US) / 2.0;

    return (uint32_t)distance;
}

int main(void)
{
    // Run system clock at 40 MHz from PLL (16 MHz crystal / 400 MHz PLL / div10)
    SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL |
                   SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    ConfigureUART();
    UARTprintf("SR04 Ultrasonic Sensor Demo\n");

    SR04Init();

    while(1)
    {
        uint32_t distance = SR04GetDistanceCM();
        UARTprintf("Distance: %d cm\n", distance);

        SysCtlDelay(SysCtlClockGet() / 60);  // ~50ms delay at 40MHz
    }
}
