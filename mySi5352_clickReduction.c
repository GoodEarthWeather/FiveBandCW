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

  uint16_t  omsynth[2] = { 0 };
  uint8_t   o_Rdiv[2] = { 0 };


/* In CW mode, receiver frequency is always set to RXOFFSET Hertz above transmit frequency
 * RXOFFSET should equal the sidetone frequency so that when spotting is performed the correct
 * transmit frequency is determined.
 * In SSB mode, there is no receiver offset so that transmit and receive frequency is the same
 */
#define RXOFFSET 608
//#define XTAL_FREQ 24998855  // for first bench prototype

#define XTAL_FREQ 24998694   // for field radio 1.0  23-dec-2024

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


}

void si5351_set_RX_freq(unsigned long freq)
{
  uint8_t a, R = 1, pll_stride = 0, msyn_stride = 0;
  uint32_t b, c, f, fvco, outdivider;
  uint32_t MSx_P1, MSNx_P1, MSNx_P2, MSNx_P3;

  unsigned long f_xtal = XTAL_FREQ;
  extern uint8_t selectedSideband;
  extern uint8_t receiveMode;

  // add rx offset freq. if in CW mode
  // check for CW mode
  if (receiveMode == RXMODE_CW)  // add offset only if in CW receive mode
  {
      (selectedSideband == LOWER_SIDEBAND) ? (freq += RXOFFSET) : (freq -= RXOFFSET);
  }

  freq = freq << 2;  // multiply by 4 for Tayloe detector


  // With 900 MHz beeing the maximum internal PLL-Frequency
  outdivider = 900000000 / freq;


  // use additional Output divider ("R")
  while (outdivider > 900) {
    R = R * 2;
    outdivider = outdivider / 2;
  }
  // finds the even divider which delivers the intended Frequency
  if (outdivider % 2) outdivider--;

  // Calculate the PLL-Frequency (given the even divider)
  fvco = outdivider * R * freq;

  // Convert the Output Divider to the bit-setting required in register 44
  switch (R) {
      case 1:   R = 0; break;
      case 2:   R = 16; break;
      case 4:   R = 32; break;
      case 8:   R = 48; break;
      case 16:  R = 64; break;
      case 32:  R = 80; break;
      case 64:  R = 96; break;
      case 128: R = 112; break;
  }
  // we have now the integer part of the output msynth
  // the b & c is fixed below

  MSx_P1 = 128 * outdivider - 512;

  // calc the a/b/c for the PLL Msynth
  /***************************************************************************
  * We will use integer only on the b/c relation, and will >> 5 (/32) both
  * to fit it on the 1048 k limit of C and keep the relation
  * the most accurate possible, this works fine with xtals from
  * 24 to 28 Mhz.
  *
  * This will give errors of about +/- 2 Hz maximum
  * as per my test and simulations in the worst case, well below the
  * XTAl ppm error...
  *
  * This will free more than 1K of the final eeprom
  *
  ****************************************************************************/
  a = fvco / f_xtal;
  b = (fvco % f_xtal) >> 5;     // Integer part of the fraction
                                  // scaled to match "c" limits
  c = f_xtal >> 5;              // "c" scaled to match it's limits
                                  // in the register

  // f is (128*b)/c to mimic the Floor(128*(b/c)) from the datasheet
  f = (128 * b) / c;

  // build the registers to write
  MSNx_P1 = 128 * a + f - 512;
  MSNx_P2 = 128 * b - f * c;
  MSNx_P3 = c;

  // PLLs and CLK# registers are allocated with a stride, we handle that with
  // the stride var to make code smaller

  // uncomment for tx (clk1)
  //if (clk > 0 ) pll_stride = 8;


  // set frequency for SYNTH_MS_0 - CLK0 for QSD input


  // Write the output divider msynth only if we need to, in this way we can
  // speed up the frequency changes almost by half the time most of the time
  // and the main goal is to avoid the nasty click noise on freq change
  if (omsynth[0] != outdivider || o_Rdiv[0] != R ) {

      // CLK# registers are exactly 8 * clk# bytes stride from a base register.
      //msyn_stride = clk * 8;
      msyn_stride = 0;  // for receiver clock, clk=0

      // keep track of the change
     // omsynth[clk] = (uint16_t) outdivider;
      omsynth[0] = (uint16_t) outdivider;  // for receiver clock, clk=0
      //o_Rdiv[clk] = R;    // cache it now, before we OR mask up R for special divide by 4
      o_Rdiv[0] = R;  // for receiver clock, clk=0

      // See datasheet, special trick when MSx == 4
      //    MSx_P1 is always 0 if outdivider == 4, from the above equations, so there is
      //    no need to set it to 0. ... MSx_P1 = 128 * outdivider - 512;
      //
      //        See para 4.1.3 on the datasheet.
      //

      if ( outdivider == 4 ) {
        R |= 0x0C;    // bit set OR mask for MSYNTH divide by 4, for reg 44 {3:2]
      }

      //Write data to multisynth registers of synth n
      i2cSendRegister(SYNTH_PLL_A, (MSNx_P3 & 0xFF00) >> 8);      // Bits [15:8] of MSNx_P3 in register 26
      i2cSendRegister(SYNTH_PLL_A + 1, (MSNx_P3 & 0xFF));
      i2cSendRegister(SYNTH_PLL_A + 2, (MSNx_P1 & 0x030000L) >> 16);
      i2cSendRegister(SYNTH_PLL_A + 3, (MSNx_P1 & 0xFF00) >> 8); // Bits [15:8]  of MSNx_P1 in register 29
      i2cSendRegister(SYNTH_PLL_A + 4, MSNx_P1 & 0xFF); // Bits [7:0]  of MSNx_P1 in register 30
      i2cSendRegister(SYNTH_PLL_A + 5, ((MSNx_P3 & 0x0F0000L) >> 12) | ((MSNx_P2 & 0x0F0000) >> 16)); // Parts of MSNx_P3 and MSNx_P1
      i2cSendRegister(SYNTH_PLL_A + 6, (MSNx_P2 & 0xFF00) >> 8);  // Bits [15:8]  of MSNx_P2 in register 32
      i2cSendRegister(SYNTH_PLL_A + 7, (MSNx_P2 & 0xFF)); // Bits [7:0]  of MSNx_P2 in register 33

      //Write data to multisynth registers of synth n
      i2cSendRegister(SYNTH_MS_0, 0);                         // Bits [15:8] of MS0_P3 (always 0) in register 42
      i2cSendRegister(SYNTH_MS_0 + 1, 1);                         // Bits [7:0]  of MS0_P3 (always 1) in register 43
      i2cSendRegister(SYNTH_MS_0 + 2, ((MSx_P1 & 0x030000L ) >> 16) | R);  // Bits [17:16] of MSx_P1 in bits [1:0] and R in [7:4] | [3:2]
      i2cSendRegister(SYNTH_MS_0 + 3, (MSx_P1 & 0xFF00) >> 8);    // Bits [15:8]  of MSx_P1 in register 45
      i2cSendRegister(SYNTH_MS_0 + 4, MSx_P1 & 0xFF);             // Bits [7:0]  of MSx_P1 in register 46
      i2cSendRegister(SYNTH_MS_0 + 5, 0);                         // Bits [19:16] of MS0_P2 and MS0_P3 are always 0
      i2cSendRegister(SYNTH_MS_0 + 6, 0);                         // Bits [15:8]  of MS0_P2 are always 0
      i2cSendRegister(SYNTH_MS_0 + 7, 0);                          // Bits [7:0]   of MS0_P2 are always 0

      // reset
      i2cSendRegister(PLL_RESET, 0xA0);
  }
  else {
      //Write data to multisynth registers of synth n
      i2cSendRegister(SYNTH_PLL_A, (MSNx_P3 & 0xFF00) >> 8);      // Bits [15:8] of MSNx_P3 in register 26
      i2cSendRegister(SYNTH_PLL_A + 1, (MSNx_P3 & 0xFF));
      i2cSendRegister(SYNTH_PLL_A + 2, (MSNx_P1 & 0x030000L) >> 16);
      i2cSendRegister(SYNTH_PLL_A + 3, (MSNx_P1 & 0xFF00) >> 8); // Bits [15:8]  of MSNx_P1 in register 29
      i2cSendRegister(SYNTH_PLL_A + 4, MSNx_P1 & 0xFF); // Bits [7:0]  of MSNx_P1 in register 30
      i2cSendRegister(SYNTH_PLL_A + 5, ((MSNx_P3 & 0x0F0000L) >> 12) | ((MSNx_P2 & 0x0F0000) >> 16)); // Parts of MSNx_P3 and MSNx_P1
      i2cSendRegister(SYNTH_PLL_A + 6, (MSNx_P2 & 0xFF00) >> 8);  // Bits [15:8]  of MSNx_P2 in register 32
      i2cSendRegister(SYNTH_PLL_A + 7, (MSNx_P2 & 0xFF)); // Bits [7:0]  of MSNx_P2 in register 33
  }

}

void si5351_set_TX_freq(unsigned long freq)
{
  uint8_t a, R = 1, pll_stride = 0, msyn_stride = 0;
  uint32_t b, c, f, fvco, outdivider;
  uint32_t MSx_P1, MSNx_P1, MSNx_P2, MSNx_P3;

  unsigned long f_xtal = XTAL_FREQ;
  // With 900 MHz beeing the maximum internal PLL-Frequency
  outdivider = 900000000 / freq;


  // use additional Output divider ("R")
  while (outdivider > 900) {
    R = R * 2;
    outdivider = outdivider / 2;
  }
  // finds the even divider which delivers the intended Frequency
  if (outdivider % 2) outdivider--;

  // Calculate the PLL-Frequency (given the even divider)
  fvco = outdivider * R * freq;

  // Convert the Output Divider to the bit-setting required in register 44
  switch (R) {
      case 1:   R = 0; break;
      case 2:   R = 16; break;
      case 4:   R = 32; break;
      case 8:   R = 48; break;
      case 16:  R = 64; break;
      case 32:  R = 80; break;
      case 64:  R = 96; break;
      case 128: R = 112; break;
  }
  // we have now the integer part of the output msynth
  // the b & c is fixed below

  MSx_P1 = 128 * outdivider - 512;

  // calc the a/b/c for the PLL Msynth
  /***************************************************************************
  * We will use integer only on the b/c relation, and will >> 5 (/32) both
  * to fit it on the 1048 k limit of C and keep the relation
  * the most accurate possible, this works fine with xtals from
  * 24 to 28 Mhz.
  *
  * This will give errors of about +/- 2 Hz maximum
  * as per my test and simulations in the worst case, well below the
  * XTAl ppm error...
  *
  * This will free more than 1K of the final eeprom
  *
  ****************************************************************************/
  a = fvco / f_xtal;
  b = (fvco % f_xtal) >> 5;     // Integer part of the fraction
                                  // scaled to match "c" limits
  c = f_xtal >> 5;              // "c" scaled to match it's limits
                                  // in the register

  // f is (128*b)/c to mimic the Floor(128*(b/c)) from the datasheet
  f = (128 * b) / c;

  // build the registers to write
  MSNx_P1 = 128 * a + f - 512;
  MSNx_P2 = 128 * b - f * c;
  MSNx_P3 = c;

  // PLLs and CLK# registers are allocated with a stride, we handle that with
  // the stride var to make code smaller

  // uncomment for tx (clk1)
  pll_stride = 8;


  // set frequency for SYNTH_MS_0 - CLK0 for QSD input


  // Write the output divider msynth only if we need to, in this way we can
  // speed up the frequency changes almost by half the time most of the time
  // and the main goal is to avoid the nasty click noise on freq change
  if (omsynth[1] != outdivider || o_Rdiv[1] != R ) {

      // CLK# registers are exactly 8 * clk# bytes stride from a base register.
      //msyn_stride = clk * 8;
      msyn_stride = 8;  // for receiver clock, clk=0

      // keep track of the change
     // omsynth[clk] = (uint16_t) outdivider;
      omsynth[1] = (uint16_t) outdivider;  // for receiver clock, clk=0
      //o_Rdiv[clk] = R;    // cache it now, before we OR mask up R for special divide by 4
      o_Rdiv[1] = R;  // for receiver clock, clk=0

      // See datasheet, special trick when MSx == 4
      //    MSx_P1 is always 0 if outdivider == 4, from the above equations, so there is
      //    no need to set it to 0. ... MSx_P1 = 128 * outdivider - 512;
      //
      //        See para 4.1.3 on the datasheet.
      //

      if ( outdivider == 4 ) {
        R |= 0x0C;    // bit set OR mask for MSYNTH divide by 4, for reg 44 {3:2]
      }

      //Write data to multisynth registers of synth n
      i2cSendRegister(SYNTH_PLL_B, (MSNx_P3 & 0xFF00) >> 8);      // Bits [15:8] of MSNx_P3 in register 26
      i2cSendRegister(SYNTH_PLL_B + 1, (MSNx_P3 & 0xFF));
      i2cSendRegister(SYNTH_PLL_B + 2, (MSNx_P1 & 0x030000L) >> 16);
      i2cSendRegister(SYNTH_PLL_B + 3, (MSNx_P1 & 0xFF00) >> 8); // Bits [15:8]  of MSNx_P1 in register 29
      i2cSendRegister(SYNTH_PLL_B + 4, MSNx_P1 & 0xFF); // Bits [7:0]  of MSNx_P1 in register 30
      i2cSendRegister(SYNTH_PLL_B + 5, ((MSNx_P3 & 0x0F0000L) >> 12) | ((MSNx_P2 & 0x0F0000) >> 16)); // Parts of MSNx_P3 and MSNx_P1
      i2cSendRegister(SYNTH_PLL_B + 6, (MSNx_P2 & 0xFF00) >> 8);  // Bits [15:8]  of MSNx_P2 in register 32
      i2cSendRegister(SYNTH_PLL_B + 7, (MSNx_P2 & 0xFF)); // Bits [7:0]  of MSNx_P2 in register 33

      //Write data to multisynth registers of synth n
      i2cSendRegister(SYNTH_MS_1, 0);                         // Bits [15:8] of MS0_P3 (always 0) in register 42
      i2cSendRegister(SYNTH_MS_1 + 1, 1);                         // Bits [7:0]  of MS0_P3 (always 1) in register 43
      i2cSendRegister(SYNTH_MS_1 + 2, ((MSx_P1 & 0x030000L ) >> 16) | R);  // Bits [17:16] of MSx_P1 in bits [1:0] and R in [7:4] | [3:2]
      i2cSendRegister(SYNTH_MS_1 + 3, (MSx_P1 & 0xFF00) >> 8);    // Bits [15:8]  of MSx_P1 in register 45
      i2cSendRegister(SYNTH_MS_1 + 4, MSx_P1 & 0xFF);             // Bits [7:0]  of MSx_P1 in register 46
      i2cSendRegister(SYNTH_MS_1 + 5, 0);                         // Bits [19:16] of MS0_P2 and MS0_P3 are always 0
      i2cSendRegister(SYNTH_MS_1 + 6, 0);                         // Bits [15:8]  of MS0_P2 are always 0
      i2cSendRegister(SYNTH_MS_1 + 7, 0);                          // Bits [7:0]   of MS0_P2 are always 0

      // reset
      i2cSendRegister(PLL_RESET, 0xA0);
  }
  else {
      //Write data to multisynth registers of synth n
      i2cSendRegister(SYNTH_PLL_B, (MSNx_P3 & 0xFF00) >> 8);      // Bits [15:8] of MSNx_P3 in register 26
      i2cSendRegister(SYNTH_PLL_B + 1, (MSNx_P3 & 0xFF));
      i2cSendRegister(SYNTH_PLL_B + 2, (MSNx_P1 & 0x030000L) >> 16);
      i2cSendRegister(SYNTH_PLL_B + 3, (MSNx_P1 & 0xFF00) >> 8); // Bits [15:8]  of MSNx_P1 in register 29
      i2cSendRegister(SYNTH_PLL_B + 4, MSNx_P1 & 0xFF); // Bits [7:0]  of MSNx_P1 in register 30
      i2cSendRegister(SYNTH_PLL_B + 5, ((MSNx_P3 & 0x0F0000L) >> 12) | ((MSNx_P2 & 0x0F0000) >> 16)); // Parts of MSNx_P3 and MSNx_P1
      i2cSendRegister(SYNTH_PLL_B + 6, (MSNx_P2 & 0xFF00) >> 8);  // Bits [15:8]  of MSNx_P2 in register 32
      i2cSendRegister(SYNTH_PLL_B + 7, (MSNx_P2 & 0xFF)); // Bits [7:0]  of MSNx_P2 in register 33
  }


}

// This routine will enable/disable the RX and TX clocks
void si5351_RXTX_enable(void)
{
    extern uint8_t txKeyState;

    if (txKeyState == TX_KEY_DOWN)
    {
        i2cSendRegister(CLK0_CONTROL, 0x8F);       // power down rx clock
        i2cSendRegister(CLK1_CONTROL, 0x0F);       // power up tx clock
        i2cSendRegister(CLK_ENABLE_CONTROL, 0b11111101);  // tx clock enabled, rx clock disabled
    }
    else
    {
        i2cSendRegister(CLK0_CONTROL, 0x0F);       // power up rx clock
        i2cSendRegister(CLK1_CONTROL, 0x8F);       // power down tx clock
        i2cSendRegister(CLK_ENABLE_CONTROL, 0b11111110);  // rx clock enabled, tx clock disabled
    }
}
