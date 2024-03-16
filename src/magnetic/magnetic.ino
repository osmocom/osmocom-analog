/* Magnetic card emulator for ATMEL
 *
 * This sould work with the original 'MagSpoof' out of the box!
 * In this case you should add a second switch, to allow test card and progrmming mode.
 *
 * (C) 2021 by Andreas Eversberg <jolly@eversberg.eu>
 * All Rights Reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* to set fused for ATTINY85: (This is default when shipped!)
 * avrdude -c usbasp-clone -p t85 -U lfuse:w:0xc0:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
 */

/* Use a clock speed of 8 MHz. If you change it, also change the fuses!!!!
 * The CLKPS bits are set to 0 by software, so that 8 MHz clock is not divided.
 */

/* Press switch 1 to emulate card, press switch 2 to emulate BSA44 service card.
 * The LED will do short flashs, to indicate that power is on.
 *
 * Hold a switch while powering on, to enter test mode. The LED will light up.
 * Press switch 1 to send continuous 0-bits. 
 * Press switch 2 to send continuous 1-bits. 
 * Press switch 1 and 2 to send continuos alternating 0- and 1-bits. 
 * WARNING: In test mode, H-bridge IC becomes quickly very hot. Don't fry it!
 * 
 * To enter programming mode, press both buttons simultaniously.
 * The LED will continously blink, to indicate programming mode.
 * Press switch 1 to change subscriber number or switch 2 to change security code.
 * The LED will then show the digit values by blinking. It can be aborted with any switch.
 * After short blinking, a long blink shows that the first digit can be entered.
 * Press switch 1 1-10 times to enter the digit. When done, press switch 2 and continue
 * with the next digit. When all digits are entered, press switch 2 twice to confirm.
 * A soft flash of the LED (fading in and out) will indicate that the new digits are stored.
 * A false input will abort the programming procedure and restart with continuous blinking.
 * To abort programming mode, press both buttuns simultaniously.
 */

#define F_CPU 8000000 // Oscillator frequency

extern "C"
{
  #include "iso7811.h"
}
#include <avr/eeprom.h>

#define PORT_ENABLE     3 // PIN 2 -> connect to 1-2EN of L293D and to LED with 1K resistor to ground
#define PORT_COIL1      0 // PIN 5 -> connect to 1A of L293D
#define PORT_COIL2      1 // PIN 6 -> connect to 2A of L293D
#define PORT_SWITCH1    2 // PIN 7 -> connect via switch 1 to ground
#define PORT_SWITCH2    4 // PIN 3 -> connect via switch 2 to ground (optional, leave open when unused)
#define PORT_RESET      5 // PIN 1 -> unused, leave open (Don't disable reset when programming fuses!!!) 

/* see main.c for more info */
#define CNETZ_LEAD_IN   12
#define CNETZ_LEAD_OUT  150

#define CLOCK_US        200 // Time to wait for half a bit

#define EEPROM_MAGIC    'c' // not equal to sim emulator, this has a capital 'C'
#define EEPROM_VERSION  '0'

#define SWITCH1 (digitalRead(PORT_SWITCH1) == LOW)
#define SWITCH2 (digitalRead(PORT_SWITCH2) == LOW)

static char number[9] = "1234567\0";
static char sicherung[6] = "12345";
static char string[19];
static uint8_t flux = 0;

/* enable H-bridge, but set it to neutral */
void enable_h_bridge(uint8_t enable)
{
    /* enable H-bridge and LED */
    digitalWrite(PORT_ENABLE, enable);

    /* set bridge to neutral */
    digitalWrite(PORT_COIL1, LOW);
    digitalWrite(PORT_COIL2, LOW);
}

/* send single bit with clock */
void send_bit(uint8_t bit)
{
  digitalWrite(PORT_COIL1, flux);
  flux ^= 1;
  digitalWrite(PORT_COIL2, flux);
  delayMicroseconds(CLOCK_US);

  if (bit & 1) {
    digitalWrite(PORT_COIL1, flux);
    flux ^= 1;
    digitalWrite(PORT_COIL2, flux);
  }
  delayMicroseconds(CLOCK_US);
}

/* blink exactly one second to confirm correct clock speed */
void blink_led()
{
  int i;
  
  for (i = 0; i < 3; i++) {
    delay(200);
    if (SWITCH1 || SWITCH2)
      break;
    enable_h_bridge(1);
    delay(200);
    if (SWITCH1 || SWITCH2)
      break;
    enable_h_bridge(0);
  }
}

/* wait until release of single key, but abort when pressing both keys.
 * if 2 is given, wait for release of all keys, don't abort when pressing both keys.
 * compensate contact shattering (German: Tastenprellen)
 */
void wait_release(int keys)
{
  int i = 0;
  while (i < 50) {
    delay(1);
    if (keys < 2) {
      if (SWITCH1 && SWITCH2)
        break;
    }
    if (SWITCH1 || SWITCH2)
      i = 0;
    else
      i++;
  }
}

/* test mode */
void test_pattern(void)
{
  /* test pattern */
  while (42) {
    if (!SWITCH1 && !SWITCH2)
      enable_h_bridge(1);
    if (SWITCH1)
      send_bit(0);
    if (SWITCH2)
      send_bit(1);
  }
}

/* read eeprom, if version is correct */
void read_eeprom(void)
{
  int i;

  if (eeprom_read_byte(0) == EEPROM_MAGIC
   && eeprom_read_byte(1) == EEPROM_VERSION) {
    for (i = 0; i < 8; i++)
      number[i] = eeprom_read_byte(i + 2);
    number[i] = '\0';
    for (i = 0; i < 5; i++)
      sicherung[i] = eeprom_read_byte(i + 10);
    sicherung[i] = '\0';
  }
}

/* write eeprom, */
void write_eeprom(void)
{
  int i;

  eeprom_write_byte(0, EEPROM_MAGIC);
  eeprom_write_byte(1, EEPROM_VERSION);
  for (i = 0; i < 8; i++)
    eeprom_write_byte(i + 2, number[i]);
  for (i = 0; i < 5; i++)
    eeprom_write_byte(i + 10, sicherung[i]);

  /* show soft flash */
  for (i = 0; i < 55; i++) {
    enable_h_bridge(1);
    delay(i/5);
    enable_h_bridge(0);
    delay(10 - i/5);
  }
  for (i = 54; i >= 0; i--) {
    enable_h_bridge(1);
    delay(i/5);
    enable_h_bridge(0);
    delay(10 - i/5);
  }
}

void setup() {
  /* setup clock speed to 8 MHz */
  CLKPR = _BV(CLKPCE);
  CLKPR = 0;

  /* setup ports */
  pinMode(PORT_ENABLE, OUTPUT);
  digitalWrite(PORT_ENABLE, LOW);
  pinMode(PORT_COIL1, OUTPUT);
  digitalWrite(PORT_COIL1, LOW);
  pinMode(PORT_COIL2, OUTPUT);
  digitalWrite(PORT_COIL2, LOW);
  pinMode(PORT_SWITCH1, INPUT_PULLUP);
  pinMode(PORT_SWITCH2, INPUT_PULLUP);

  /* blink with LED */
  blink_led();

  /* transmit test pattern */
  if (SWITCH1 || SWITCH2) {
    wait_release(1);
    test_pattern();
  }

  /* read subscriber data from eeprom */
  read_eeprom();
}

/* send card data */
void send_string(const char *string)
{
  uint8_t data[CNETZ_LEAD_IN + 21 + CNETZ_LEAD_OUT];
  int length, i;
  uint8_t digit;

  length = encode_track(data, string, CNETZ_LEAD_IN, CNETZ_LEAD_OUT);

  /* enable H-bridge and LED */
  enable_h_bridge(1);

  /* send bits */
  for (i = 0; i < length; i++) {
    digit = data[i];
    send_bit((digit >> 0) & 1);
    send_bit((digit >> 1) & 1);
    send_bit((digit >> 2) & 1);
    send_bit((digit >> 3) & 1);
    send_bit((digit >> 4) & 1);
    /* abort when pressing both switches */
    if (SWITCH1 && SWITCH2)
      break;
  }

  /* disable H-bridge */
  enable_h_bridge(0);
}

/* send zeros or ones depending on the key pressed, stop by pressing both keys */
void program_mode(void)
{
  uint8_t blink;
  uint8_t edit;
  char io[9];
  int i, b, d;

error:
  blink = 0;
  edit = 0;
  /* flash LED, wait for key press */
  while (!edit) {
    blink ^= 1;
    enable_h_bridge(blink);
    for (d = 0; d < 50; d++) {
      delay(1);
      if (SWITCH1) {
        edit = 1;
        break;
      }
      if (SWITCH2) {
        edit = 2;
        break;
      }
    }
  }
  enable_h_bridge(0);
  wait_release(1);
  if (SWITCH1 && SWITCH2)
     goto done;

  /* copy subscriber data to io-buffer */
  switch (edit) {
  case 1:
    for (i = 0; i < 8; i++)
      io[i] = number[i];
    io[i] = '\0';
    break;
  case 2:
    for (i = 0; i < 5; i++)
      io[i] = sicherung[i];
    io[i] = '\0';
    break;
  }

  /* blink the io-buffer data */
  for (i = 0; io[i]; i++) {
    for (d = 0; d < 1000; d++) {
      delay(1);
      if (SWITCH1 || SWITCH2)
        goto stop_blink;
    }
    if (io[i] > '0')
      blink = io[i] - '0';
    else
      blink = 10;
    for (b = 0; b < blink; b++) {
      enable_h_bridge(1);
      for (d = 0; d < 100; d++) {
        delay(1);
        if (SWITCH1 || SWITCH2) {
          enable_h_bridge(0);
          goto stop_blink;
        }
      }
      enable_h_bridge(0);
      for (d = 0; d < 400; d++) {
        delay(1);
        if (SWITCH1 || SWITCH2)
          goto stop_blink;
      }
    }
  }
  for (d = 0; d < 1000; d++) {
    delay(1);
    if (SWITCH1 && SWITCH2)
      goto stop_blink;
  }
  stop_blink:
  wait_release(1);
  if (SWITCH1 && SWITCH2)
    goto done;
  enable_h_bridge(1);
  for (d = 0; d < 500; d++) {
    delay(1);
    if (SWITCH1 && SWITCH2)
      goto done;
  }
  enable_h_bridge(0);

  /* key in the data to io-buffer */
  i = 0;
  b = 0;
  while (42) {
    if (SWITCH1) {
      enable_h_bridge(1);
      for (d = 0; d < 100; d++) {
        delay(1);
        if (SWITCH1 && SWITCH2)
          goto done;
      }
      enable_h_bridge(0);
      wait_release(1);
      if (SWITCH1 && SWITCH2)
        goto done;
      b++;
      if (b > 10)
        goto error;
    }
    if (SWITCH2) {
      wait_release(1);
      if (SWITCH1 && SWITCH2)
        goto done;
      if (b == 0) {
        while (i < 9)
          io[i++] = '\0';
        break;
      }
      if (b == 10)
        b = 0;
      io[i++] = '0' + b;
      b = 0;
      if (i > 8)
        goto error;
      enable_h_bridge(1);
      for (d = 0; d < 500; d++) {
        delay(1);
        if (SWITCH1 && SWITCH2)
          goto done;
      }
      enable_h_bridge(0);
    }
  }

  /* verify input */
  switch (edit) {
  case 1:
    if (strlen(io) < 7)
      goto error;
    if (io[0] > '7')
      goto error;
    if (strlen(io) == 8) {
      if ((io[1] - '0') * 10 + io[2] - '0' > 31)
        goto error;
      if (atoi(io + 3) > 65535)
        goto error;
    } else {
      if (atoi(io + 2) > 65535)
        goto error;
    }
    break;
  case 2:
    if (strlen(io) > 5)
      goto error;
    if (!io[0])
      goto error;
    if (io[0] == '0' && io[1] != '\0')
      goto error;
    if (atoi(io) > 65535)
      goto error;
    break;
  }

  /* copy io-buffer data to subscriber data */
  switch (edit) {
  case 1:
    for (i = 0; i < 8; i++)
      number[i] = io[i];
    number[i] = '\0';
    break;
  case 2:
    for (i = 0; i < 5; i++)
      sicherung[i] = io[i];
    sicherung[i] = '\0';
    break;
  }

  /* write subscriber data to eeprom */
  write_eeprom();

done:
  enable_h_bridge(0);
  return;
}

static uint16_t flash = 0;

void loop() {
  /* go programming */
  if (SWITCH1 && SWITCH2) {
    flash = 0;
    wait_release(2);
    program_mode();
    wait_release(2);
    return;
  }

  /* send card */
  if (SWITCH1) {
    flash = 0;
    cnetz_card(string, number, sicherung);
    send_string(string);
    wait_release(1);
    return;
  }

  /* send service card */
  if (SWITCH2) {
    flash = 0;
    bsa44_service(string);
    send_string(string);
    wait_release(1);
    return;
  }

  /* slow blink to show that the device is powered on */
  delay(1);
  flash++;
  if (flash == 1980)
    enable_h_bridge(1);
  if (flash == 2000) {
    enable_h_bridge(0);
    flash = 0;
  }
}
