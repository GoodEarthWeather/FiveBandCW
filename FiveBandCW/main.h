/*
 * main.h
 *
 *  Created on: May 16, 2018
 *      Author: dmcneill
 */

/*
 * Change BTN_SB_SELECT to BTN_DIGIT_SELECT - this makes the sideband select button now be the digit select button for tuning
 */

#ifndef MYINCLUDE_MAIN_H_
#define MYINCLUDE_MAIN_H_

#include "driverlib.h"

void initClocks(void);
void initGPIO(void);
void moveFreqCursor(void);
void updateDisplay(uint8_t);
void selectBand(void);
void selectFilter(void);
void selectSideband(void);
void selectAudioState(uint8_t);
void setBatVoltText(uint32_t);
void getBatteryVoltage(void);
void initADC(uint8_t);
void selectMenuFunction(void);
void initSideToneTimer(void);
void updateFrequency(void);
void initQSKTimer(uint16_t);
void updateQSKDelay(void);
void initKeyTimer(uint8_t);
void updateCWSpeed(void);
void ditdah(uint8_t);
void setTuneMode(void);
void setTRSwitch(uint8_t);

void i2cSendRegister(uint8_t reg, uint8_t data);

//  SI5351 Declarations
void si5351_start(void);
void si5351_set_RX_freq(unsigned long);
void si5351_set_TX_freq(unsigned long);
void si5351_RXTX_enable(void);
void initsi5351(void);

/////////////////////
//Defines for Si5351
/////////////////////
#define SI5351_ADDRESS 0x60

//Set of Si5351A register addresses
#define DEVICE_STATUS            0
#define CLK_ENABLE_CONTROL       3
#define PLLX_SRC                15
#define CLK0_CONTROL            16
#define CLK1_CONTROL            17
#define CLK2_CONTROL            18
#define SYNTH_PLL_A             26
#define SYNTH_PLL_B             34
#define SYNTH_MS_0              42
#define SYNTH_MS_1              50
#define SYNTH_MS_2              58
#define PLL_RESET              177
#define XTAL_LOAD_CAP          183
#define SSEN                   149


#define RESET_PLL 1
#define NO_RESET_PLL 0

#define ENABLED 1
#define DISABLED 0

// Define all I/O
#define ENCODER_A GPIO_PORT_P4, GPIO_PIN5
#define ENCODER_B GPIO_PORT_P2, GPIO_PIN2
#define ENCODER_SWITCH GPIO_PORT_P4, GPIO_PIN6

//#define LED1 GPIO_PORT_P1, GPIO_PIN1
#define VBAT_SNS GPIO_PORT_P1, GPIO_PIN0
#define CWSPEED_SNS GPIO_PORT_P1, GPIO_PIN4

#define BTN_TXMODE GPIO_PORT_P5, GPIO_PIN7
#define BTN_MUTE GPIO_PORT_P3, GPIO_PIN3
#define BTN_DIGIT_SELECT GPIO_PORT_P2, GPIO_PIN3
#define BTN_FILTER_SELECT GPIO_PORT_P3, GPIO_PIN4
#define BTN_BAND_SELECT GPIO_PORT_P3, GPIO_PIN1
#define BTN_SPOT GPIO_PORT_P2, GPIO_PIN4
#define BTN_RXMODE GPIO_PORT_P3, GPIO_PIN7
#define BTN_MENU GPIO_PORT_P4, GPIO_PIN0
#define DIT_KEY GPIO_PORT_P4, GPIO_PIN1
#define DAH_KEY GPIO_PORT_P4, GPIO_PIN2
#define STRAIGHT_KEY GPIO_PORT_P2, GPIO_PIN7
#define TR_SWITCH GPIO_PORT_P3, GPIO_PIN5
#define CW_OUT GPIO_PORT_P3, GPIO_PIN2
#define TR_MUTE GPIO_PORT_P3, GPIO_PIN6
#define BAND_40M_SELECT GPIO_PORT_P6, GPIO_PIN1
#define BAND_30M_SELECT GPIO_PORT_P6, GPIO_PIN2
#define BAND_20M_SELECT GPIO_PORT_P4, GPIO_PIN7
#define BAND_17M_SELECT GPIO_PORT_P5, GPIO_PIN0
#define BAND_15M_SELECT GPIO_PORT_P5, GPIO_PIN1
#define FILTER_SELECT GPIO_PORT_P5, GPIO_PIN4
#define SIDEBAND_SELECT GPIO_PORT_P5, GPIO_PIN5
#define SIDETONE_OUTPUT GPIO_PORT_P1, GPIO_PIN1
#define TXMODE_LED GPIO_PORT_P6, GPIO_PIN0


// define frequency bands
#define BAND_40M_LOWER 7000000
#define BAND_40M_UPPER 7300000
#define BAND_30M_LOWER 10100000
#define BAND_30M_UPPER 10150000
#define BAND_20M_LOWER 14000000
#define BAND_20M_UPPER 14350000
#define BAND_17M_LOWER 18068000
#define BAND_17M_UPPER 18168000
#define BAND_15M_LOWER 21000000
#define BAND_15M_UPPER 21450000

// define names for button presses
#define BTN_PRESSED_NONE 0x0
#define BTN_PRESSED_TXMODE 0x1
#define BTN_PRESSED_MUTE 0x2
#define BTN_PRESSED_DIGIT_SELECT 0x3
#define BTN_PRESSED_FILTER_SELECT 0x4
#define BTN_PRESSED_BAND_SELECT 0x5
#define BTN_PRESSED_SPOT 0x6
#define BTN_PRESSED_RXMODE 0x7
#define BTN_PRESSED_MENU 0x8
#define BTN_PRESSED_ENCODER 0x9
#define BTN_PRESSED_DIT 0xA
#define BTN_PRESSED_DAH 0xB

// define bands
#define BAND_40M 0x0
#define BAND_30M 0x1
#define BAND_20M 0x2
#define BAND_17M 0x3
#define BAND_15M 0x4

// define menu functions
#define MENU_FUNCTION_BATVOLTAGE 0x0
#define MENU_FUNCTION_CWSPEED 0x1
#define MENU_FUNCTION_QSK_DELAY 0x2

#define MAX_QSK_DELAY 800   // 800 ms
#define MIN_QSK_DELAY 5    // 5 ms
#define MAX_CW_SPEED 30   // wpm
#define MIN_CW_SPEED 5    / wpm

#define UPPER_SIDEBAND 0x0
#define LOWER_SIDEBAND 0x1
#define SSB_FILTER 0x0
#define CW_FILTER 0x1

#define MUTE 0x1
#define UNMUTE 0x0

#define DIT 0x1
#define DAH 0x3

#define TX_KEY_UP 0x0
#define TX_KEY_DOWN 0x1

#define BATTERY_MEASUREMENT 0x0
#define CWSPEED_MEASUREMENT 0x1

#define FREQ_DISPLAY 0x1
#define BAND_DISPLAY 0x2
#define MENU_DISPLAY 0x3
#define MODE_DISPLAY 0x4

#define RXMODE_CW 0x0
#define RXMODE_USB 0x1
#define RXMODE_LSB 0x2

// for tr switch
#define TRANSMIT 0x0
#define RECEIVE 0x1

#endif /* MYINCLUDE_MAIN_H_ */
