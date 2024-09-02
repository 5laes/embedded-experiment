/*
 * File:   main.c
 * Author: Claes
 *
 * Created on July 13, 2024, 7:43 PM
 */

#include "mcc_generated_files/system.h"
#include "mcc_generated_files/usb/usb_device_cdc.h"
#include "mcc_generated_files/usb/usb_device.h"
#include "mcc_generated_files/usb/usb_hal_pic24f.h"

#define FCY 16000000
#include <libpic30.h>
#include <xc.h>

#include <string.h>
#include <stdio.h>

void blink_led();
void send_uart_message(const char *buffer, const uint8_t size);
void get_uart_message();
void three_sec_hold();
uint16_t readADC();

int main(void)
{
    // initialize the device
    SYSTEM_Initialize();
    
    // <editor-fold defaultstate="collapsed" desc="AD Converter Config">
    
    // AD1CON1 Config
    AD1CON1bits.ADON = 0;
    AD1CON1bits.SSRC = 0b111;
    AD1CON1bits.FORM = 0b00;
    
    // AD1CON2 Config
    AD1CON2bits.NVCFG0 = 0;
    AD1CON2bits.PVCFG = 0b00;
    
    // AD1CON3 Config
    AD1CON3bits.ADCS = 0b11111111;
    AD1CON3bits.SAMC = 0b11111;
    
    // AD1CHS Config
    AD1CHSbits.CH0SA = 2;
    
    AD1CON1bits.ADON = 1; // Activate AD converstion
    
    // </editor-fold>
    
    // <editor-fold defaultstate="collapsed" desc="Timer Config">
    
    T2CONbits.T32 = 1;
    T2CONbits.TCKPS = 0b01;
    T2CONbits.TCS = 0;
    T2CONbits.TGATE = 0;
    PR3 = 0x001E;
    PR2 = 0xC350;
    IFS0bits.T3IF = 0;
    IEC0bits.T3IE = 1;
    IPC2bits.T3IP = 4;
    T2CONbits.TON = 0;
    
    // </editor-fold>

    // <editor-fold defaultstate="collapsed" desc="Pin Config">
    
    // LED-pin configurations
    ANSELCbits.ANSC1 = 1;
    TRISCbits.TRISC1 = 0;
    LATCbits.LATC1 = 1;
    
    // Button-pin configurations
    TRISAbits.TRISA12 = 1;
    IOCPUAbits.IOCPUA12 = 1;
    
    // Analog pin (AN2)
    ANSBbits.ANSB0 = 1;
    TRISBbits.TRISB0 = 1;
    
    // </editor-fold>

    uint8_t sendBuffer[] = {0x4D, 0x61, 0x72, 0x65, 0x6E, 0x69, 0x75, 0x73,
    '\n', '\r', '\0'};
    
    while(1)
    {
        USBDeviceTasks();
        if((USBGetDeviceState() < CONFIGURED_STATE) ||
           (USBIsDeviceSuspended() == true))
        {
            continue;
        }
        else
        {
            //Keep trying to send data to the PC as required
            CDCTxService();

            //Run application code.
            if(PORTAbits.RA12 == 0)
            {
                blink_led();
                if(PORTAbits.RA12 == 0)
                {
                    while(PORTAbits.RA12 == 0);
                    __delay_ms(20);
                    T2CONbits.TON = !T2CONbits.TON;
                    
                }
                else
                {
                    send_uart_message(sendBuffer, sizeof(sendBuffer) - 1);
                }
            }
            else
            {
                get_uart_message();
            }
        }
    }

    return 1;
}

void blink_led()
{
    for(unsigned int i = 0; i < 3; i++)
    {
        LATCbits.LATC1 = 0;
        __delay_ms(500);
        LATCbits.LATC1 = 1;
        __delay_ms(500);
    }
    LATCbits.LATC1 = 1;
}

void send_uart_message(const char *buffer, const uint8_t size)
{
    if(USBUSARTIsTxTrfReady())
    {
        putUSBUSART(buffer, size);
    }
}

void get_uart_message()
{
    /* Tried having the buffer as a global variable but that came with some 
     problems. Keeping it here for the moment*/
    uint8_t getBuffer[64];
    uint8_t numBytes = getsUSBUSART(getBuffer, sizeof(getBuffer) - 1);
    if(numBytes > 0)
    {
        uint8_t *ptr = strtok(getBuffer, '\n');
        if(strcmp("CMD\r\n", ptr) == 0)
        {
            send_uart_message("OK\r\n", 3);
        }
        else
        {
            send_uart_message("NOK\r\n", 4);
        }
    }
}

uint16_t readADC()
{
    AD1CON1bits.SAMP = 1;
    while(!AD1CON1bits.DONE);
    return ADC1BUF0;
}

void __attribute__((__interrupt__, no_auto_psv)) _T3Interrupt(void)
{
    /*
     * Feels like i should not need to do all the USB checking here
     * but the message gets all random if i don't. Has probably something 
     * to do with interrupts happening in the middle of these tasks in the .
     * main loop
     */
    USBDeviceTasks();
    while((USBGetDeviceState() < CONFIGURED_STATE) ||
       (USBIsDeviceSuspended() == true));
    uint16_t num = readADC();
    uint8_t str[64];
    uint8_t size = sprintf(str, "The value of AN2 is: %d\r\n", num);
    send_uart_message(str, size);
    CDCTxService();
    IFS0bits.T3IF = 0;
}
