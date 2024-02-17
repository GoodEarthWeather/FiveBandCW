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
static void keyDown(void);
static void keyUp(void);

// This routine is to handle the dit and dah key
// 'key' is either DIT or DAH (i.e. 1 or 3)
void ditdah(uint8_t key)
{
    uint8_t done;
    uint8_t count;
    uint8_t ditKeyState;
    uint8_t dahKeyState;
    extern uint8_t txMode;

    iambicMode = DISABLED;

    do {
        ditKeyState = GPIO_getInputPinValue(DIT_KEY);
        dahKeyState = GPIO_getInputPinValue(DAH_KEY);
        // iambic test
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

        Timer_A_startCounter(TIMER_A0_BASE,TIMER_A_UP_MODE);  // start side tone
        // if transmitter enabled, turn on
        if (txMode == ENABLED)
            keyDown();
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
        //  wait one unit
        if (txMode == ENABLED)
            keyUp();
        Timer_A_stop(TIMER_A0_BASE);  // stop side tone
        Timer_A_clearCaptureCompareInterrupt(TIMER_A2_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
        do {
            done = Timer_A_getCaptureCompareInterruptStatus(TIMER_A2_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_0,TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG);
        } while (done != TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG);
        Timer_A_clearCaptureCompareInterrupt(TIMER_A2_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);


        ditKeyState = GPIO_getInputPinValue(DIT_KEY);
        dahKeyState = GPIO_getInputPinValue(DAH_KEY);

        if ((key == DIT) && (ditKeyState == GPIO_INPUT_PIN_HIGH))
            break;
        if ((key == DAH) && (dahKeyState == GPIO_INPUT_PIN_HIGH))
            break;

    } while (((dahKeyState) == GPIO_INPUT_PIN_LOW) || (ditKeyState == GPIO_INPUT_PIN_LOW));

}

// This routine will start transmission
static void keyDown(void)
{
    extern uint8_t txKeyState;

    selectAudioState(MUTE);
    // stop qsk timer
    Timer_A_stop(TIMER_A1_BASE);
    Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_0);
    Timer_A_disableCaptureCompareInterrupt(TIMER_A1_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_0);
    setTRSwitch(TRANSMIT);

    GPIO_setOutputHighOnPin(CW_OUT);
    txKeyState = TX_KEY_DOWN;
    si5351_RXTX_enable();
}

// This routine will stop transmission
static void keyUp(void)
{
    extern uint8_t txKeyState;

    Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_0);
    Timer_A_clear(TIMER_A1_BASE);  // reset QSK timer
    Timer_A_enableCaptureCompareInterrupt(TIMER_A1_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_0);
    Timer_A_startCounter( TIMER_A1_BASE,TIMER_A_CONTINUOUS_MODE);
    txKeyState = TX_KEY_UP;
    GPIO_setOutputLowOnPin(CW_OUT);
    delay_ms(5);
    setTRSwitch(RECEIVE);
    si5351_RXTX_enable();
}

// This routine will turn on the transmitter for tuning
void setTuneMode(void)
{
    extern uint8_t tuneMode;
    extern uint8_t txKeyState;
    extern uint8_t txMode;

    if (txMode == ENABLED)  // tune mode only if in txmode
    {
        if (tuneMode == ENABLED)
        {
            keyDown();
            Timer_A_startCounter(TIMER_A0_BASE,TIMER_A_UP_MODE);  // start side tone
            /*
            selectAudioState(MUTE);
            setTRSwitch(TRANSMIT);
            Timer_A_startCounter(TIMER_A0_BASE,TIMER_A_UP_MODE);  // start side tone
            GPIO_setOutputHighOnPin(CW_OUT);
            txKeyState = TX_KEY_DOWN;
            si5351_RXTX_enable();
            */
        }
        else
        {
            keyUp();
            Timer_A_stop(TIMER_A0_BASE);  // stop side tone
            /*
            txKeyState = TX_KEY_UP;
            GPIO_setOutputLowOnPin(CW_OUT);
            Timer_A_stop(TIMER_A0_BASE);  // stop side tone
            delay_ms(5);
            setTRSwitch(RECEIVE);
            si5351_RXTX_enable();
            selectAudioState(UNMUTE);
            */
        }
    }
}

// This routine will set the state of the tr switch
void setTRSwitch(uint8_t state)
{
    if (state == RECEIVE)
        GPIO_setOutputHighOnPin(TR_SWITCH); // receive mode
    else if (state == TRANSMIT)
        GPIO_setOutputLowOnPin(TR_SWITCH); // transmit mode
}
