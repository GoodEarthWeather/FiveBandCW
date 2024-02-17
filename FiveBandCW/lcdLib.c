#include "driverlib.h"
#include "lcdLib.h"
#include "main.h"
#include <stdlib.h>
#include <math.h>

#define	LOWNIB(x)	P2OUT = (P2OUT & 0xF0) + (x & 0x0F)

#define BUFFER_SIZE 12
static char buffer[BUFFER_SIZE];  /* must be static to be able to return it */
static char freqBuffer[16];
static char batVoltBuffer[8]; /* for holding battery voltage text */
static char cwSpeedBuffer[8]; /* for holding cw speed text */
static char ritStateBuffer[16];

void setCWSpeedText(void);
void setBatVoltText(uint32_t);


void lcdInit() {
	delay_ms(100);
	// Wait for 100ms after power is applied.

	setData(0x03);  // Start LCD (send 0x03)

	lcdTriggerEN(); // Send 0x03 3 times at 5ms then 100 us
	delay_ms(5);
	lcdTriggerEN();
	delay_ms(5);
	lcdTriggerEN();
	delay_ms(5);

	setData(0x02); // Switch to 4-bit mode
	lcdTriggerEN();
	delay_ms(5);

	lcdWriteCmd(0x28); // 4-bit, 2 line, 5x8
	lcdWriteCmd(0x08); // Instruction Flow
	lcdWriteCmd(0x01); // Clear LCD
	lcdWriteCmd(0x06); // Auto-Increment
	lcdWriteCmd(0x0E); // Display On, Cursor On, No blink
}

void lcdTriggerEN() {
    GPIO_setOutputHighOnPin(LCD_CLK);  // toggle clock (enable) bit
    GPIO_setOutputLowOnPin(LCD_CLK);
}

void lcdWriteData(unsigned char data) {
	GPIO_setOutputHighOnPin(LCD_RS); // Set RS to data
	setData(data >> 4); // Upper nibble
	lcdTriggerEN();
	setData(data); // Lower nibble
	lcdTriggerEN();
	delay_us(50); // Delay > 47 us
}

void lcdWriteCmd(unsigned char cmd) {
    GPIO_setOutputLowOnPin(LCD_RS); // Set RS to command
	setData(cmd >> 4); // Upper nibble
	lcdTriggerEN();
	setData(cmd); // Lower nibble
	lcdTriggerEN();
	delay_ms(5); // Delay > 1.5ms
}

void lcdSetText(char* text, int x, int y) {
	uint8_t i;
	if (x < 16) {
		x |= 0x80; // Set LCD for first line write
		switch (y){
		case 1:
			x |= 0x40; // Set LCD for second line write
			break;
		case 2:
			x |= 0x60; // Set LCD for first line write reverse
			break;
		case 3:
			x |= 0x20; // Set LCD for second line write reverse
			break;
		}
		lcdWriteCmd(x);
	}
	i = 0;

	while (text[i] != '\0') {
		lcdWriteData(text[i]);
		i++;
	}
}

void lcdSetInt(uint32_t val, int x, int y){
	char *result;
	result = number_to_string(val);
	lcdSetText(result, x, y);
}

void lcdClear() {
	lcdWriteCmd(CLEAR);
}

// This function will take a 4 bit data nibble and split it such that the bits
// in position 2,3 are shifted up to positions 5,6 to match the mapping of the GPIO
// to the LCD module.  It will then set the P2OUT ports to the resulting value
void setData(uint8_t data)
{
    (data & 0x01) ? (GPIO_setOutputHighOnPin(LCD_D4)) : (GPIO_setOutputLowOnPin(LCD_D4));
    (data & 0x02) ? (GPIO_setOutputHighOnPin(LCD_D5)) : (GPIO_setOutputLowOnPin(LCD_D5));
    (data & 0x04) ? (GPIO_setOutputHighOnPin(LCD_D6)) : (GPIO_setOutputLowOnPin(LCD_D6));
    (data & 0x08) ? (GPIO_setOutputHighOnPin(LCD_D7)) : (GPIO_setOutputLowOnPin(LCD_D7));
}

char *number_to_string(uint32_t number)
{
    char *p;
    uint32_t digit;

    p = &buffer[BUFFER_SIZE - 1];
    *p-- = '\0';  /* end of string */
    do {
        digit = number % 10;
        *p-- = '0' + digit;
        number /= 10;
    } while (number > 0);
    return p+1;  /* first digit might not be at the start of the buffer */
}

// routine to move cursor for frequency readout; always on line 1
void moveFreqCursor(void)
{
    uint8_t address;
    extern uint32_t freqMultiplier;
    extern uint8_t selectedBand;

    switch (freqMultiplier) {
    case 1 :
        address = 0x08;
        break;
    case 10 :
        address = 0x07;
        break;
    case 100 :
        address = 0x06;
        break;
    case 1000 :
        address = 0x04;
        break;
    case 10000 :
        address = 0x03;
        break;
    }
    if ( selectedBand != BAND_40M )
        address++;

    lcdWriteCmd(MOVE_CURSOR + address);
}

// routine to display frequency
void updateDisplay(uint8_t field)
{
    char *result;
    extern uint8_t selectedBand;
    extern uint32_t si5351FreqOut;
    extern uint8_t selectedFilter;
    extern uint8_t selectedSideband;
    extern uint16_t batteryVoltage;
    extern uint8_t selectedMenuFunction;
    extern int16_t ritOffset;
    extern uint8_t ritState;
    extern uint8_t audioState;
    extern uint8_t receiveMode;
    extern uint16_t qskDelay;
    uint8_t i;
    float z;
    uint32_t batV;


    //lcdClear();

    // display frequency
    //lcdSetInt(si5351FreqOut, 0, 0);
    if ( field == FREQ_DISPLAY)  // update frequency field
    {
        result = number_to_string(si5351FreqOut);
        if (selectedBand == BAND_40M)
        {
            freqBuffer[0] = *result++;
            freqBuffer[1] = '.';
            freqBuffer[2] = *result++;
            freqBuffer[3] = *result++;
            freqBuffer[4] = *result++;
            freqBuffer[5] = '.';
            freqBuffer[6] = *result++;
            freqBuffer[7] = *result++;
            freqBuffer[8] = *result++;
            freqBuffer[9] = '\0';
        } else {
            freqBuffer[0] = *result++;
            freqBuffer[1] = *result++;
            freqBuffer[2] = '.';
            freqBuffer[3] = *result++;
            freqBuffer[4] = *result++;
            freqBuffer[5] = *result++;
            freqBuffer[6] = '.';
            freqBuffer[7] = *result++;
            freqBuffer[8] = *result++;
            freqBuffer[9] = *result++;
            freqBuffer[10] = '\0';
        }
        lcdSetText("          ",0,0);  // clear field
        lcdSetText(freqBuffer,0,0);
    }

    else if (field == BAND_DISPLAY) // update band field
    {
        // display band
        lcdSetText("   ",0xD,0); // clear field
        switch (selectedBand)
        {
        case  BAND_40M :
            lcdSetText("40M",0xD,0);  // put band info at position 13 (0xD) on first row
            break;
        case  BAND_30M :
            lcdSetText("30M",0xD,0);  // put band info at position 13 (0xD) on first row
            break;
        case  BAND_20M :
            lcdSetText("20M",0xD,0);  // put band info at position 13 (0xD) on first row
            break;
        case  BAND_17M :
            lcdSetText("17M",0xD,0);  // put band info at position 13 (0xD) on first row
            break;
        case  BAND_15M :
            lcdSetText("15M",0xD,0);  // put band info at position 13 (0xD) on first row
            break;
        default :
            break;
        }
    }

    else if (field == MENU_DISPLAY)
    {
        lcdSetText("            ",0,1);
        // display menu function if RIT is not enabled
        if (ritState == ENABLED)
        {
            lcdSetText("RIT: ",0,1);
            result = number_to_string((uint32_t)(abs(ritOffset)));
            (ritOffset < 0) ? (ritStateBuffer[0] = '-') : (ritStateBuffer[0] = '+');
            i = 1;
            while (*result != '\0')
                ritStateBuffer[i++] = *result++;
            ritStateBuffer[i] = '\0';
            lcdSetText(ritStateBuffer,5,1);
        }
        else if (audioState == MUTE)
        {
            lcdSetText("MUTE",0,1);
        }
        else
        {
            switch (selectedMenuFunction)
            {
            case MENU_FUNCTION_BATVOLTAGE :
                getBatteryVoltage();
                // assume external resistor divider is 0.1 and full scale is 1.5V
                // ((batteryVoltage * 1.5/0.1) * 65536)/4096 = batteryVoltage * 240
                // Actual resistor divider is about 0.098, not 0.1, so 245 is used instead of 240
                z = ((float)batteryVoltage/4096.0)*1.5/0.098;
                batV = (uint32_t)(round(z*10.0));
                setBatVoltText(batV);
                break;
            case MENU_FUNCTION_CWSPEED :
                setCWSpeedText();
                break;
            case MENU_FUNCTION_QSK_DELAY :
                lcdSetText("QSK: ",0,1);
                result = number_to_string((uint32_t)qskDelay);
                i = 0;
                while (*result != '\0')  // use ritStateBuffer to hold qsk delay string
                    ritStateBuffer[i++] = *result++;
                ritStateBuffer[i] = '\0';
                lcdSetText(ritStateBuffer,5,1);
                break;
            default :
                break;
            }
        }
    }
    else if (field == MODE_DISPLAY)
    {
        lcdSetText("   ",0xD,1);
        // display filter selection
        if (receiveMode == RXMODE_CW)
            lcdSetText(" CW",0xD,1);
        else if (receiveMode == RXMODE_USB)
            lcdSetText("USB",0xD,1);
        else
            lcdSetText("LSB",0xD,1);
    }
    moveFreqCursor();
}

/* This routine will take the battery voltage and convert it to text for the LCD */
void setBatVoltText(uint32_t result)
{
   char *txt;

    /* now convert to string */
    txt = number_to_string(result);

    batVoltBuffer[0] = *txt++;
    if ( result < 100)
    {
        batVoltBuffer[1] = '.';
        batVoltBuffer[2] = *txt++;
        batVoltBuffer[3] = ' ';
        batVoltBuffer[4] = 'V';
        batVoltBuffer[5] = '\0';
    }
    else
    {
        batVoltBuffer[1] = *txt++;
        batVoltBuffer[2] = '.';
        batVoltBuffer[3] = *txt++;
        batVoltBuffer[4] = ' ';
        batVoltBuffer[5] = 'V';
        batVoltBuffer[6] = '\0';
    }
    lcdSetText(batVoltBuffer,0,1);
}

/* This routine will take the battery voltage and convert it to text for the LCD */
void setCWSpeedText(void)
{
    extern uint8_t wpm;
    char *txt;


    lcdSetText("CWSPD: ",0,1);

    /* now convert to string */
    txt = number_to_string((uint32_t)wpm);

    cwSpeedBuffer[0] = *txt++;
    if (wpm > 9)
    {
        cwSpeedBuffer[1] = *txt++;
        cwSpeedBuffer[2] = '\0';
    }  else {
        cwSpeedBuffer[1] = '\0';
    }
    lcdSetText(cwSpeedBuffer,7,1);
}
