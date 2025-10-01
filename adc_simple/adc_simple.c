/******************************************************************************
 * Project  : Read ADC from A0 (PE3)
 * File     : adc_simple.c
 *
 * Description:
 *   Example code for reading ADC
 *   and dumping results over UART 115200 8n1.
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
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/adc.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"

void ADCSeq0Handler(void) {}

// Configure UART0 for 115200 baud, 8N1
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

int main(void)
{
    uint32_t adcValue;

    // Run system clock at 40 MHz from PLL (16 MHz crystal / 400 MHz PLL / div10)
    SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL |
                   SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    ConfigureUART();
    UARTprintf("Simple ADC + UART demo\n");

    // Enable ADC0 and GPIOE (for PE3 / AIN0)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);   // PE3 is AIN0

    // Configure ADC0, sequence 3, processor trigger, highest priority
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, 3);
    ADCIntClear(ADC0_BASE, 3);

    while(1)
    {
        // Trigger the ADC conversion
        ADCProcessorTrigger(ADC0_BASE, 3);

        // Wait until conversion complete
        while(!ADCIntStatus(ADC0_BASE, 3, false)) {}

        ADCIntClear(ADC0_BASE, 3);

        // Read the ADC value
        ADCSequenceDataGet(ADC0_BASE, 3, &adcValue);

        // Print result
        UARTprintf("ADC Value: %4d\n", adcValue);

        SysCtlDelay(SysCtlClockGet() / 300); // ~1 second delay at 40MHz
    }
}
