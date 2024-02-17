/*
 * DK7IH_si5351.c
 *
 *  Created on: Jan 15, 2022
 *      Author: fishi
 */
/*****************************************************************/
/*             RF generator with Si5153 and ATMega8              */
/*  ************************************************************ */
/*  Mikrocontroller:  ATMEL AVR ATmega8, 8 MHz                   */
/*                                                               */
/*  Compiler:         GCC (GNU AVR C-Compiler)                   */
/*  Author:           Peter Rachow (DK7IH)                       */
/*  Last Change:      2017-FEB-23                                */
/*****************************************************************/
//Important:
//This is an absolute minimum software to generate a 10MHz signal with
//an ATMega8 and the SI5351 chip. Only one CLK0 and CLK1 are used
//to supply rf to RX and TX module seperately.

//I have tested this software with my RIGOL 100Mhz scope. Up to this
//frequency the Si5331 produced output.

//The software is more for educational purposes but can be modfied
//to get more stuff out of the chip.
//
//73 de Peter (DK7IH)

// Modified by David McNeill 16-Jan-2022

#include "main.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define delay_ms(x)     __delay_cycles((long) x* 1000 * 8)

/* In CW mode, receiver frequency is always set to RXOFFSET Hertz above transmit frequency
 * RXOFFSET should equal the sidetone frequency so that when spotting is performed the correct
 * transmit frequency is determined.
 * In SSB mode, there is no receiver offset so that transmit and receive frequency is the same
 */
#define RXOFFSET 608
#define XTAL_FREQ 24998855
//#define XTAL_FREQ 26999386

////////////////////////////////
//
// Si5351A commands
//
/////////////////////////////

// Set PLLs (VCOs) to internal clock rate of 900 MHz
// Equation fVCO = fXTAL * (a+b/c) (=> AN619 p. 3
void si5351_start(void)
{
  unsigned long a, b, c;
  unsigned long p1, p2, p3;

  // wait until si5351 device status register (0x00) indicates system is ready
  initsi5351();

  // Init clock chip
  i2cSendRegister(XTAL_LOAD_CAP, 0xD2);      // Set crystal load capacitor to 10pF (default),
  //i2cSendRegister(XTAL_LOAD_CAP, 0x92);      // Set crystal load capacitor to 10pF (default),
                                          // for bits 5:0 see also AN619 p. 60
  i2cSendRegister(CLK_ENABLE_CONTROL, 0xFF); // Disable all outputs
  i2cSendRegister(CLK0_CONTROL, 0x0F);       // Set PLLA to CLK0, 8 mA output
  i2cSendRegister(CLK1_CONTROL, 0x2F);       // Set PLLB to CLK1, 8 mA output
  i2cSendRegister(CLK2_CONTROL, 0x2F);       // Set PLLB to CLK2, 8 mA output
  i2cSendRegister(PLL_RESET, 0xA0);          // Reset PLLA and PLLB
  i2cSendRegister(CLK_ENABLE_CONTROL, 0xFE); // Enable only CLK0
  i2cSendRegister(SSEN, 0x0);                // Disable spread spectrum

  // Set VCOs of PLLA and PLLB to 891 MHz
  a = 35;           // Division factor 900/25 MHz
  b = 0;            // Numerator, sets b/c=0
  c = 1048575;      //Max. resolution, but irrelevant in this case (b=0)

  //Formula for splitting up the numbers to register data, see AN619
  p1 = 128 * a + (unsigned long) (128 * b / c) - 512;
  p2 = 128 * b - c * (unsigned long) (128 * b / c);
  p3  = c;

  //Write data to registers PLLA and PLLB so that both VCOs are set to 891MHz intermal freq
  i2cSendRegister(SYNTH_PLL_A, (p3 & 0xFF00) >> 8);
  i2cSendRegister(SYNTH_PLL_A + 1, (p3 & 0xFF));
  i2cSendRegister(SYNTH_PLL_A + 2, (p1 & 0x00030000) >> 16);
  i2cSendRegister(SYNTH_PLL_A + 3, (p1 & 0x0000FF00) >> 8);
  i2cSendRegister(SYNTH_PLL_A + 4, (p1 & 0x000000FF));
  i2cSendRegister(SYNTH_PLL_A + 5, 0xF0 | ((p2 & 0x000F0000) >> 16));
  i2cSendRegister(SYNTH_PLL_A + 6, (p2 & 0x0000FF00) >> 8);
  i2cSendRegister(SYNTH_PLL_A + 7, (p2 & 0x000000FF));

  i2cSendRegister(SYNTH_PLL_B, (p3 & 0xFF00) >> 8);
  i2cSendRegister(SYNTH_PLL_B + 1, (p3 & 0xFF));
  i2cSendRegister(SYNTH_PLL_B + 2, (p1 & 0x00030000) >> 16);
  i2cSendRegister(SYNTH_PLL_B + 3, (p1 & 0x0000FF00) >> 8);
  i2cSendRegister(SYNTH_PLL_B + 4, (p1 & 0x000000FF));
  i2cSendRegister(SYNTH_PLL_B + 5, 0xF0 | ((p2 & 0x000F0000) >> 16));
  i2cSendRegister(SYNTH_PLL_B + 6, (p2 & 0x0000FF00) >> 8);
  i2cSendRegister(SYNTH_PLL_B + 7, (p2 & 0x000000FF));

}

void si5351_set_RX_freq(unsigned long freq)
{

  unsigned long  a, b, c;
  unsigned long f_xtal = XTAL_FREQ;
  double fdiv;  //division factor fvco/freq (will be integer part of a+b/c)
  double rm; //remainder
  unsigned long p1, p2, p3;
  extern uint8_t selectedSideband;
  extern uint8_t receiveMode;

  // add rx offset freq. if in CW mode
  // check for CW mode
  if (receiveMode == RXMODE_CW)  // add offset only if in CW receive mode
  {
      (selectedSideband == LOWER_SIDEBAND) ? (freq += RXOFFSET) : (freq -= RXOFFSET);
  }

  freq = freq << 2;  // multiply by 4 for Tayloe detector

  // set frequency for SYNTH_MS_0 - CLK0 for QSD input
  a = b = c = 1048575;
  fdiv = (double) (f_xtal * 35) / freq;
  a = (unsigned long) fdiv;
  rm = fdiv - a;  //(equiv. b/c)
  b = rm * c;
  p1  = 128 * a + (unsigned long) (128 * b / c) - 512;
  p2 = 128 * b - c * (unsigned long) (128 * b / c);
  p3 = c;

  //Write data to multisynth registers of synth n
  i2cSendRegister(SYNTH_MS_0, (p3 & 0xFF00) >> 8);      //1048757 MSB
  i2cSendRegister(SYNTH_MS_0 + 1, (p3 & 0xFF));  //1048757 LSB
  i2cSendRegister(SYNTH_MS_0 + 2, (p1 & 0x00030000) >> 16);
  i2cSendRegister(SYNTH_MS_0 + 3, (p1 & 0x0000FF00) >> 8);
  i2cSendRegister(SYNTH_MS_0 + 4, (p1 & 0x000000FF));
  i2cSendRegister(SYNTH_MS_0 + 5, 0xF0 | ((p2 & 0x000F0000) >> 16));
  i2cSendRegister(SYNTH_MS_0 + 6, (p2 & 0x0000FF00) >> 8);
  i2cSendRegister(SYNTH_MS_0 + 7, (p2 & 0x000000FF));

}

void si5351_set_TX_freq(unsigned long freq)
{

  unsigned long  a, b, c;
  unsigned long f_xtal = XTAL_FREQ;
  double fdiv;  //division factor fvco/freq (will be integer part of a+b/c)
  double rm; //remainder
  unsigned long p1, p2, p3;

  // set frequency for SYNTH_MS_1 - CLK1 for transmitter
  a = b = c = 1048575;
  fdiv = (double) (f_xtal * 35) / freq;
  a = (unsigned long) fdiv;
  rm = fdiv - a;  //(equiv. b/c)
  b = rm * c;
  p1  = 128 * a + (unsigned long) (128 * b / c) - 512;
  p2 = 128 * b - c * (unsigned long) (128 * b / c);
  p3 = c;

  //Write data to multisynth registers of synth n
  i2cSendRegister(SYNTH_MS_1, (p3 & 0xFF00) >> 8);      //1048757 MSB
  i2cSendRegister(SYNTH_MS_1 + 1, (p3 & 0xFF));  //1048757 LSB
  i2cSendRegister(SYNTH_MS_1 + 2, (p1 & 0x00030000) >> 16);
  i2cSendRegister(SYNTH_MS_1 + 3, (p1 & 0x0000FF00) >> 8);
  i2cSendRegister(SYNTH_MS_1 + 4, (p1 & 0x000000FF));
  i2cSendRegister(SYNTH_MS_1 + 5, 0xF0 | ((p2 & 0x000F0000) >> 16));
  i2cSendRegister(SYNTH_MS_1 + 6, (p2 & 0x0000FF00) >> 8);
  i2cSendRegister(SYNTH_MS_1 + 7, (p2 & 0x000000FF));

}

// This routine will enable/disable the RX and TX clocks
void si5351_RXTX_enable(void)
{
    extern uint8_t txKeyState;

    if (txKeyState == TX_KEY_DOWN)
    {
        i2cSendRegister(CLK0_CONTROL, 0x8F);       // power down rx clock
        i2cSendRegister(CLK1_CONTROL, 0x0F);       // power up tx clock
        //i2cSendRegister(CLK_ENABLE_CONTROL, 0b11111101);  // tx clock enabled, rx clock disabled
    }
    else
    {
        i2cSendRegister(CLK0_CONTROL, 0x0F);       // power up rx clock
        i2cSendRegister(CLK1_CONTROL, 0x8F);       // power down tx clock
        //i2cSendRegister(CLK_ENABLE_CONTROL, 0b11111110);  // rx clock enabled, tx clock disabled
    }
}
