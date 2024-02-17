#include "driverlib.h"
#include "main.h"
#include "lcdlib.h"


uint8_t encoderCWCount, encoderCCWCount;
uint32_t si5351FreqOut;
uint32_t freqMultiplier;
uint8_t selectedBand;
uint8_t selectedFilter;
uint8_t selectedSideband;
uint16_t batteryVoltage;
uint32_t maxBandFreq;
uint32_t minBandFreq;
uint8_t selectedMenuFunction;
uint8_t txKeyState;  // indicates whether key is down or up (pressed or released)
uint8_t keyStateChanged;  // indicates that the key was either pressed or released
uint8_t ritState; // indicates whether receiver is in RIT mode
int16_t ritOffset;
uint8_t wpm;
uint8_t audioState; // indicates whether receiver is muted or not
uint8_t spotMode;  // enables sidetone for setting zero beat with incoming signal
uint8_t txMode;  // enable or disable transmitter
uint8_t receiveMode;  // indicates whether receiver is set for receiving CW (rxoffset added) or SSB (no offset)
uint16_t qskDelay = 300;  // breakin delay in milliseconds
uint8_t tuneMode; // for when tune button is pressed


int main(void) {


    extern uint8_t volatile buttonPressed;

    WDT_A_hold(WDT_A_BASE);
    initGPIO();
    initClocks();
    lcdInit();
    initSideToneTimer();
    initQSKTimer(qskDelay);  // initialize for 300ms delay
    wpm = 15;
    initKeyTimer(wpm);  // initialize keyer timer for 15 WMP
    initADC(BATTERY_MEASUREMENT);
    setTRSwitch(RECEIVE);

    si5351_start();

    // disable TX and RIT
    keyStateChanged = 0;
    txKeyState = TX_KEY_UP; // means not transmitting
    ritState = DISABLED;
    ritOffset = 0;
    txMode = DISABLED;

    // set defaults

    receiveMode = RXMODE_CW;
    tuneMode = DISABLED;
    spotMode = DISABLED;
    freqMultiplier = 1000;
    selectedBand = BAND_40M;
    selectedFilter = SSB_FILTER;
    selectedSideband = LOWER_SIDEBAND;
    selectedMenuFunction = MENU_FUNCTION_BATVOLTAGE;
    selectBand();
    selectFilter();
    selectSideband();
    selectMenuFunction();

    updateDisplay(MODE_DISPLAY);

    // set initial si5351 clock to 7MHz
    si5351FreqOut = BAND_40M_LOWER;
    si5351_set_RX_freq(si5351FreqOut);
    si5351_set_TX_freq(si5351FreqOut);

    // now unmute audio
    audioState = UNMUTE;
    selectAudioState(UNMUTE);

    // setup encoder variables
    encoderCWCount = 0;
    encoderCCWCount = 0;

    buttonPressed = BTN_PRESSED_NONE;
    __bis_SR_register(GIE);
    while (1)
    {
        // check if keyed
        if (keyStateChanged == 1)
        {
            keyStateChanged = 0;
            // sidetone
            (txKeyState == TX_KEY_DOWN) ?(Timer_A_startCounter(TIMER_A0_BASE,TIMER_A_UP_MODE)) : (Timer_A_stop(TIMER_A0_BASE));  //start sidetone timer
            if (txMode == ENABLED)  // transmitter is enabled
            {
                selectAudioState(MUTE);
                if (txKeyState == TX_KEY_DOWN)
                {
                    setTRSwitch(TRANSMIT);
                    GPIO_setOutputHighOnPin(CW_OUT);
                }
                else  // txKeyState is TX_KEY_UP
                {
                    GPIO_setOutputLowOnPin(CW_OUT);
                    Timer_A_clear(TIMER_A1_BASE);  // reset QSK timer
                    Timer_A_startCounter( TIMER_A1_BASE,TIMER_A_CONTINUOUS_MODE);
                    setTRSwitch(RECEIVE);
                }
                si5351_RXTX_enable();
            }
        }

        // check encoder
        if ((encoderCWCount != 0) || (encoderCCWCount != 0))  // if true, the encoder knob was turned
        {
            if (selectedMenuFunction == MENU_FUNCTION_QSK_DELAY)
            {
                updateQSKDelay();
                initQSKTimer(qskDelay);
            }
            else if (selectedMenuFunction == MENU_FUNCTION_CWSPEED)
            {
                updateCWSpeed();
                initKeyTimer(wpm);  // update timer with new speed
            }
            else
                updateFrequency();
        }

        // check buttons
        switch (buttonPressed)
        {
        case BTN_PRESSED_NONE :
            break;
        case BTN_PRESSED_TXMODE :
            if (txMode == DISABLED)
            {
                txMode = ENABLED;
                GPIO_setOutputHighOnPin(TXMODE_LED); // turn on LED to indicate transmit mode enabled
            }
            else
            {
                txMode = DISABLED;
                GPIO_setOutputLowOnPin(TXMODE_LED); // turn off LED to indicate transmit mode disabled
            }
            buttonPressed = BTN_PRESSED_NONE;
            break;
        case BTN_PRESSED_MUTE :
            (audioState == MUTE) ? (audioState = UNMUTE) : (audioState = MUTE);
            selectAudioState(audioState);
            updateDisplay(MENU_DISPLAY);
            buttonPressed = BTN_PRESSED_NONE;
            break;
        case BTN_PRESSED_FILTER_SELECT :
            (selectedFilter == CW_FILTER) ? (selectedFilter = SSB_FILTER) : (selectedFilter = CW_FILTER);
            selectFilter();
            buttonPressed = BTN_PRESSED_NONE;
            break;
        case BTN_PRESSED_BAND_SELECT :
            selectAudioState(MUTE);
            (selectedBand == BAND_15M) ? (selectedBand = BAND_40M) : (selectedBand++);
            selectBand();
            if (audioState != MUTE)
                selectAudioState(UNMUTE);
            buttonPressed = BTN_PRESSED_NONE;
            break;
        case BTN_PRESSED_SPOT :
            if ( txMode == DISABLED)
            {
                if ( spotMode == DISABLED)  // button pressed to now put it in spot mode
                {
                    Timer_A_startCounter(TIMER_A0_BASE,TIMER_A_UP_MODE);  // start side tone
                    spotMode = ENABLED;
                }
                else
                {
                    Timer_A_stop(TIMER_A0_BASE);  // stop side tone
                    spotMode = DISABLED;
                }
            }
            else
            {
                (tuneMode == DISABLED) ? (tuneMode = ENABLED) : (tuneMode = DISABLED);
                setTuneMode();
            }
            buttonPressed = BTN_PRESSED_NONE;
            break;
        case BTN_PRESSED_RXMODE :
            if (receiveMode == RXMODE_LSB)
            {
                receiveMode = RXMODE_CW;
                (selectedBand == BAND_40M) ? (selectedSideband= LOWER_SIDEBAND) : (selectedSideband = UPPER_SIDEBAND);
            }
            else if (receiveMode == RXMODE_USB)
            {
                receiveMode = RXMODE_LSB;
                selectedSideband = LOWER_SIDEBAND;
            }
            else
            {
                receiveMode = RXMODE_USB;
                selectedSideband = UPPER_SIDEBAND;
            }
            selectAudioState(MUTE);
            selectSideband();
            updateDisplay(MODE_DISPLAY);
            encoderCWCount = encoderCCWCount = 0;
            si5351_set_RX_freq(si5351FreqOut);
            selectAudioState(UNMUTE);
            buttonPressed = BTN_PRESSED_NONE;
            break;
        case BTN_PRESSED_MENU :
            (selectedMenuFunction == MENU_FUNCTION_QSK_DELAY) ? (selectedMenuFunction = MENU_FUNCTION_BATVOLTAGE) : (selectedMenuFunction++);
            selectMenuFunction();
            buttonPressed = BTN_PRESSED_NONE;
            break;
        case BTN_PRESSED_DIGIT_SELECT :
            (freqMultiplier == 10000) ? (freqMultiplier = 10) : (freqMultiplier *= 10);
            moveFreqCursor();
            buttonPressed = BTN_PRESSED_NONE;
            break;
        case BTN_PRESSED_ENCODER :
            (ritState == DISABLED) ? (ritState = ENABLED) : (ritState = DISABLED);
            ritOffset = 0;
            si5351_set_RX_freq(si5351FreqOut);  //put RX frequency back to same as TX freq.
            freqMultiplier = 100;
            updateDisplay(MENU_DISPLAY);
            buttonPressed = BTN_PRESSED_NONE;
            break;
        case BTN_PRESSED_DIT :
            buttonPressed = BTN_PRESSED_NONE;
            ditdah(DIT);
            break;
        case BTN_PRESSED_DAH :
            buttonPressed = BTN_PRESSED_NONE;
            ditdah(DAH);
            break;
        default :
            // if no buttons have been pressed and selected menu function is CW speed, need to check if pot has moved and update display if so
            break;
        }
    }
}

// routine to select band
void selectBand(void)
{
    switch (selectedBand)
    {
    case BAND_40M :
        GPIO_setOutputHighOnPin(BAND_40M_SELECT);
        GPIO_setOutputLowOnPin(BAND_30M_SELECT);
        GPIO_setOutputLowOnPin(BAND_20M_SELECT);
        GPIO_setOutputLowOnPin(BAND_17M_SELECT);
        GPIO_setOutputLowOnPin(BAND_15M_SELECT);
        si5351FreqOut = BAND_40M_LOWER;
        si5351_set_RX_freq(si5351FreqOut);
        si5351_set_TX_freq(si5351FreqOut);
        selectedSideband = LOWER_SIDEBAND;
        selectSideband();
        maxBandFreq = BAND_40M_UPPER;
        minBandFreq = BAND_40M_LOWER;
        break;
    case BAND_30M :
        GPIO_setOutputLowOnPin(BAND_40M_SELECT);
        GPIO_setOutputHighOnPin(BAND_30M_SELECT);
        GPIO_setOutputLowOnPin(BAND_20M_SELECT);
        GPIO_setOutputLowOnPin(BAND_17M_SELECT);
        GPIO_setOutputLowOnPin(BAND_15M_SELECT);
        si5351FreqOut = BAND_30M_LOWER;
        si5351_set_RX_freq(si5351FreqOut);
        si5351_set_TX_freq(si5351FreqOut);
        selectedSideband = UPPER_SIDEBAND;
        selectSideband();
        maxBandFreq = BAND_30M_UPPER;
        minBandFreq = BAND_30M_LOWER;
        break;
    case BAND_20M :
        GPIO_setOutputLowOnPin(BAND_40M_SELECT);
        GPIO_setOutputLowOnPin(BAND_30M_SELECT);
        GPIO_setOutputHighOnPin(BAND_20M_SELECT);
        GPIO_setOutputLowOnPin(BAND_17M_SELECT);
        GPIO_setOutputLowOnPin(BAND_15M_SELECT);
        si5351FreqOut = BAND_20M_LOWER;
        si5351_set_RX_freq(si5351FreqOut);
        si5351_set_TX_freq(si5351FreqOut);
        selectedSideband = UPPER_SIDEBAND;
        selectSideband();
        maxBandFreq = BAND_20M_UPPER;
        minBandFreq = BAND_20M_LOWER;
        break;
    case BAND_17M :
        GPIO_setOutputLowOnPin(BAND_40M_SELECT);
        GPIO_setOutputLowOnPin(BAND_30M_SELECT);
        GPIO_setOutputLowOnPin(BAND_20M_SELECT);
        GPIO_setOutputHighOnPin(BAND_17M_SELECT);
        GPIO_setOutputLowOnPin(BAND_15M_SELECT);
        si5351FreqOut = BAND_17M_LOWER;
        si5351_set_RX_freq(si5351FreqOut);
        si5351_set_TX_freq(si5351FreqOut);
        selectedSideband = UPPER_SIDEBAND;
        selectSideband();
        maxBandFreq = BAND_17M_UPPER;
        minBandFreq = BAND_17M_LOWER;
        break;
    case BAND_15M :
        GPIO_setOutputLowOnPin(BAND_40M_SELECT);
        GPIO_setOutputLowOnPin(BAND_30M_SELECT);
        GPIO_setOutputLowOnPin(BAND_20M_SELECT);
        GPIO_setOutputLowOnPin(BAND_17M_SELECT);
        GPIO_setOutputHighOnPin(BAND_15M_SELECT);
        si5351FreqOut = BAND_15M_LOWER;
        si5351_set_RX_freq(si5351FreqOut);
        si5351_set_TX_freq(si5351FreqOut);
        selectedSideband = UPPER_SIDEBAND;
        selectSideband();
        maxBandFreq = BAND_15M_UPPER;
        minBandFreq = BAND_15M_LOWER;
        break;
    default :
        break;
    }
    // reset menu function
    ritState = DISABLED;
    ritOffset = 0;
    receiveMode = RXMODE_CW;
    initADC(BATTERY_MEASUREMENT);
    selectedMenuFunction = MENU_FUNCTION_BATVOLTAGE;
    updateDisplay(BAND_DISPLAY);
    updateDisplay(MENU_DISPLAY);
    updateDisplay(FREQ_DISPLAY);
    updateDisplay(MODE_DISPLAY);
}

// routine to select filter
void selectFilter(void)
{
    if (selectedFilter == CW_FILTER)
        GPIO_setOutputLowOnPin(FILTER_SELECT);  // set low for CW filter
    else
        GPIO_setOutputHighOnPin(FILTER_SELECT); // set high for SSB filter
}

// routine to select sideband
void selectSideband(void)
{
    if (selectedSideband == UPPER_SIDEBAND)
        GPIO_setOutputHighOnPin(SIDEBAND_SELECT);  // need to check
    else
        GPIO_setOutputLowOnPin(SIDEBAND_SELECT); // need to check
}

// routine to set audio state - mute or unmute
void selectAudioState(uint8_t state)
{
    if ( state == MUTE )
        GPIO_setOutputHighOnPin(TR_MUTE); // set high for mute pin
    else if (state == UNMUTE)
        GPIO_setOutputLowOnPin(TR_MUTE); // set low for unmute pin
}

// routine to select desired menu functions
void selectMenuFunction(void)
{
    switch ( selectedMenuFunction )
    {
    case MENU_FUNCTION_BATVOLTAGE :
        initADC(BATTERY_MEASUREMENT);
        break;
    case MENU_FUNCTION_CWSPEED :
        break;
    case MENU_FUNCTION_QSK_DELAY :
        break;
    default :
        break;
    }
    updateDisplay(MENU_DISPLAY);
}

// routine to measure battery voltage
void getBatteryVoltage(void)
{

    ADC_enable(ADC_BASE);
    // start ADC conversion
    ADC_startConversion(ADC_BASE,ADC_SINGLECHANNEL);
    while (ADC_isBusy(ADC_BASE) == ADC_BUSY) {;}

    // get results
    batteryVoltage = (uint16_t)ADC_getResults(ADC_BASE);
    ADC_disable(ADC_BASE);
}

// This routine will update the frequency setting and display
void updateFrequency(void)
{
    uint32_t deltaFreq;

    if ( encoderCWCount > encoderCCWCount ) // net count indicates frequency increase
    {
        deltaFreq = encoderCWCount - encoderCCWCount;
        if ( (si5351FreqOut + deltaFreq*freqMultiplier) <= maxBandFreq )
        {
            (ritState == ENABLED) ? (ritOffset += deltaFreq*freqMultiplier) : (si5351FreqOut += deltaFreq*freqMultiplier);
            si5351_set_RX_freq(si5351FreqOut+ritOffset);
            si5351_set_TX_freq(si5351FreqOut);
            updateDisplay(FREQ_DISPLAY);
            if (ritState == ENABLED)
                updateDisplay(MENU_DISPLAY);
        }
    }
    else if ( encoderCWCount < encoderCCWCount )  // frequency decrease
    {
        deltaFreq = encoderCCWCount - encoderCWCount;
        if ( (si5351FreqOut - deltaFreq*freqMultiplier) >= minBandFreq )
        {
            (ritState == ENABLED) ? (ritOffset -= deltaFreq*freqMultiplier) : (si5351FreqOut -= deltaFreq*freqMultiplier);
            si5351_set_RX_freq(si5351FreqOut+ritOffset);
            si5351_set_TX_freq(si5351FreqOut);
            updateDisplay(FREQ_DISPLAY);
            if (ritState == ENABLED)
                updateDisplay(MENU_DISPLAY);
        }
    }
encoderCWCount = encoderCCWCount = 0;

}

// This routine will update the qskDelay setting and display
void updateQSKDelay(void)
{
    uint32_t deltaDelay;

    if ( encoderCWCount > encoderCCWCount ) // net count indicates increase
    {
        deltaDelay = encoderCWCount - encoderCCWCount;
        if ( (qskDelay + 10*deltaDelay) <= MAX_QSK_DELAY )
        {
            qskDelay += 10*deltaDelay;
            updateDisplay(MENU_DISPLAY);
        }
    }
    else if ( encoderCWCount < encoderCCWCount )  // decrease
    {
        deltaDelay = encoderCCWCount - encoderCWCount;
        if ( qskDelay >= MIN_QSK_DELAY )
        {
            qskDelay -= 10*deltaDelay;
            updateDisplay(MENU_DISPLAY);
        }
    }
encoderCWCount = encoderCCWCount = 0;
}

// This routine will update the CW speed setting and display
void updateCWSpeed(void)
{
    uint32_t delta;

    if ( encoderCWCount > encoderCCWCount ) // net count indicates increase
    {
        delta = encoderCWCount - encoderCCWCount;
        if ( (wpm + delta) <= MAX_CW_SPEED )
        {
            wpm += delta;
            updateDisplay(MENU_DISPLAY);
        }
    }
    else if ( encoderCWCount < encoderCCWCount )  // decrease
    {
        delta = encoderCCWCount - encoderCWCount;
        if ( wpm >= MIN_CW_SPEED )
        {
            wpm -= delta;
            updateDisplay(MENU_DISPLAY);
        }
    }
encoderCWCount = encoderCCWCount = 0;
}
