/*
 * key.c
 *
 *  Created on: Feb 23, 2022
 *      Author: fishi
 */

#include "driverlib.h"
#include "main.h"
#include "lcdLib.h"

uint8_t iambicMode;
cwMem_t cwMem;  // to contain cw memories

// This routine is to handle the dit and dah key
// 'key' is either DIT or DAH (i.e. 1 or 3)
uint8_t * ditdah_record(uint8_t key, uint8_t *memX)
{
    uint8_t done;
    uint8_t count;
    uint8_t ditKeyState;
    uint8_t dahKeyState;
    extern uint8_t txMode;
    extern uint8_t paddleOrientation;  // paddles configured for dah-dit or dit-dah

    iambicMode = DISABLED;

    do {
        if (paddleOrientation == PADDLE_DAH_DIT){
            ditKeyState = GPIO_getInputPinValue(DIT_KEY);
            dahKeyState = GPIO_getInputPinValue(DAH_KEY);
        }
        else {
            ditKeyState = GPIO_getInputPinValue(DAH_KEY);
            dahKeyState = GPIO_getInputPinValue(DIT_KEY);
        }
        // iambic test - both paddles are pressed
        if ( (dahKeyState == GPIO_INPUT_PIN_LOW) && (ditKeyState == GPIO_INPUT_PIN_LOW) )  // iambic mode, so alternate dit/dah
        {
            (key == DIT) ? (key = DAH) : (key = DIT);
            iambicMode = ENABLED;
        } else {
            iambicMode = DISABLED;
            // not in iambic mode, so set key to whatever key remains pressed
            (dahKeyState == GPIO_INPUT_PIN_LOW) ? (key = DAH) : (key = DIT);
            // end of iambic test
        }

        if ((ditKeyState == GPIO_INPUT_PIN_HIGH) && (dahKeyState == GPIO_INPUT_PIN_HIGH))
            break;

        // start sidetone timer (A0)
        Timer_A_startCounter(TIMER_A0_BASE,TIMER_A_UP_MODE);  // start side tone
        // init dit-dah timer (A2) and then delay for a dit or dah time
        Timer_A_clearCaptureCompareInterrupt(TIMER_A2_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
        Timer_A_clear(TIMER_A2_BASE);  // clear timer
        count = 0;
        while (count < key)
        {
            done = Timer_A_getCaptureCompareInterruptStatus(TIMER_A2_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_0,TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG);
            if (done == TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG)
            {
                count++;
                Timer_A_clearCaptureCompareInterrupt(TIMER_A2_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
            }
        }
        Timer_A_stop(TIMER_A0_BASE);  // stop side tone
        // record dit or dah
        *memX++ = key;

        //  wait one unit
        Timer_A_clearCaptureCompareInterrupt(TIMER_A2_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
        do {
            done = Timer_A_getCaptureCompareInterruptStatus(TIMER_A2_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_0,TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG);
        } while (done != TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG);
        Timer_A_clearCaptureCompareInterrupt(TIMER_A2_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);

        if (paddleOrientation == PADDLE_DAH_DIT){
            ditKeyState = GPIO_getInputPinValue(DIT_KEY);
            dahKeyState = GPIO_getInputPinValue(DAH_KEY);
        }
        else {
            ditKeyState = GPIO_getInputPinValue(DAH_KEY);
            dahKeyState = GPIO_getInputPinValue(DIT_KEY);
        }

        if ((key == DIT) && (ditKeyState == GPIO_INPUT_PIN_HIGH))
            break;
        if ((key == DAH) && (dahKeyState == GPIO_INPUT_PIN_HIGH))
            break;

    } while (((dahKeyState) == GPIO_INPUT_PIN_LOW) || (ditKeyState == GPIO_INPUT_PIN_LOW));
    return(memX);
}

//
// Routine to record a CW message into memory
// This routine "takes over" from the main loop:
// This routine is active for the entire recording session.
// The routine will only respond to the paddle keys (dit/dah)
// to record, and the BTN_PRESSED_ENCODER button is pressed
// to end the recording - all other functions are ignored.

uint8_t * recordCWMessage(uint8_t *memX) {
    extern uint8_t volatile buttonPressed;
    extern uint8_t txMode;
    extern uint8_t paddleOrientation;  // paddles configured for dah-dit or dit-dah

    uint8_t done;
    uint8_t spaceCount; // number of dits of space between characters
    uint8_t spaceCounter;  // flag to enable or disable space counter
    /*
     * define:
     *   DAH = 0x3
     *   DIT = 0x1
     *   SPACE = 0x80 + delay bits - i.e. bit 7 is set to indicate no sidetone
     */

    txMode = DISABLED;  // disable transmitting while recording CW message
    GPIO_setOutputLowOnPin(TXMODE_LED); // turn off LED to indicate transmit mode disabled
    spaceCount = 0;
    spaceCounter = DISABLED;
    // waiting for paddle dit or dah to start recording
    while (buttonPressed != BTN_PRESSED_ENCODER)
    {
        switch (buttonPressed)
        {
        case BTN_PRESSED_DIT :
            if (spaceCounter == ENABLED)
            {
                spaceCount |= 0x80; // set msb high to indicate no sidetone
                *memX++ = spaceCount;
            }
            buttonPressed = BTN_PRESSED_NONE;
            (paddleOrientation == PADDLE_DAH_DIT) ? ditdah_record(DIT,memX) : ditdah_record(DAH,memX);
            spaceCounter = ENABLED;
            spaceCount = 0;
            break;
        case BTN_PRESSED_DAH :
            if (spaceCounter == ENABLED)
            {
                spaceCount |= 0x80; // set msb high to indicate no sidetone
                *memX++ = spaceCount;
            }
            buttonPressed = BTN_PRESSED_NONE;
            (paddleOrientation == PADDLE_DAH_DIT) ? ditdah_record(DAH,memX) : ditdah_record(DIT,memX);
            spaceCounter = ENABLED;
            spaceCount = 0;
            break;
        case BTN_PRESSED_NONE :
            // delay one dit time
            if (spaceCounter == ENABLED)
            {
                Timer_A_clear(TIMER_A2_BASE);  // clear timer
                Timer_A_clearCaptureCompareInterrupt(TIMER_A2_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
                do {
                    done = Timer_A_getCaptureCompareInterruptStatus(TIMER_A2_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_0,TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG);
                } while (done != TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG);
                Timer_A_clearCaptureCompareInterrupt(TIMER_A2_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
                spaceCount++;
            }
            break;
        }
    }
    return(memX);
}
