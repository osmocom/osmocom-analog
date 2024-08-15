/* C-Netz SIM card emulator for ATMEL
 *
 * (C) 2020 by Andreas Eversberg <jolly@eversberg.eu>
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

extern "C"
{
  #include "sim.h"
  #include "eeprom.h"
}

/* settings for ATTINY85 */
#if defined(__AVR_ATtiny85__)
#define SERIAL_DATA  4
#define SERIAL_DELAY 124
#define SERIAL_TIMEOUT 800 /* > more than two bytes */
#else
/* settings for Arduino UNO with 16 MHz */
#define STATUS_LED   LED_BUILTIN
#define RESET_PIN    6
#define SERIAL_DATA  7
#define SERIAL_DELAY 410
#define SERIAL_TIMEOUT 1600 /* > more than two bytes */
#endif
/* to set fused for ATTINY85:
 * avrdude -c usbasp-clone -p t85 -U lfuse:w:0xc0:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
 */

/* timing test TX (010101010011) */
//#define TEST_TX
/* timing test RX (000000000001) */
//#define TEST_RX
/* timing test timeout (pause + 000000000001) */
//#define TEST_TO

sim_sim_t sim;

#include <avr/eeprom.h>
#include <util/delay.h>

uint8_t eeprom_read(enum eeprom_locations loc)
{
  return eeprom_read_byte((uint8_t *)loc);
}

void eeprom_write(enum eeprom_locations loc, uint8_t value)
{
  eeprom_write_byte((uint8_t *)loc, value);
}

size_t eeprom_length(void)
{
  return 512;
}

#ifdef RESET_PIN
volatile uint8_t *reset_in;
uint8_t reset_bit;

/* init reset pin */
void reset_init(uint8_t pin)
{
  uint8_t port;
  volatile uint8_t *mode, *out;

  reset_bit = digitalPinToBitMask(pin);
  port = digitalPinToPort(pin);

  mode = portModeRegister(port);
  out = portOutputRegister(port);
  reset_in = portInputRegister(port);

  *mode &= ~reset_bit; /* input */
  *out |= reset_bit; /* pullup */
}
#endif

volatile uint8_t *serial_mode, *serial_out, *serial_in;
uint8_t serial_bit;
uint16_t serial_delay;

/* init serial pin */
void serial_init(uint8_t pin, uint16_t delay)
{
  uint8_t port;

  serial_delay = delay;
  serial_bit = digitalPinToBitMask(pin);
  port = digitalPinToPort(pin);

  serial_mode = portModeRegister(port);
  serial_out = portOutputRegister(port);
  serial_in = portInputRegister(port);

  *serial_mode &= ~serial_bit; /* input */
  *serial_out |= serial_bit; /* pullup */
}

/* wait some time so the stop bits haven been elapsed before transmitting a block */
void serial_start_tx(void)
{
  /* wait some time, so previous stop bits have been elapsed */
  _delay_loop_2(serial_delay * 3); /* 2..3 bits of time */
}

/* transmit a byte */
void serial_tx(uint8_t b)
{
  uint8_t i, c = 0;

  /* start bit */
  *serial_mode |= serial_bit; /* output */
  *serial_out &= ~serial_bit; /* low */
  _delay_loop_2(serial_delay);
  /* 8 data bits */
  for (i = 8; i > 0; --i) {
    if (b & 1)
      *serial_out |= serial_bit; /* high */
    else
      *serial_out &= ~serial_bit; /* low */
    _delay_loop_2(serial_delay);
    c ^= b;
    b>>= 1;
  }
  /* even parity */
  if (c & 1)
    *serial_out |= serial_bit; /* high */
  else
    *serial_out &= ~serial_bit; /* low */
  _delay_loop_2(serial_delay);
  /* 2 stop bits */
  *serial_out |= serial_bit; /* high */
  _delay_loop_2(serial_delay);
  _delay_loop_2(serial_delay);
  *serial_mode &= ~serial_bit; /* input */
}

/* receive a byte */
uint8_t serial_rx(void)
{
  uint8_t i, b = 0;

  /* center read */
  _delay_loop_2(serial_delay >> 1);
  /* 8 data bits */
  for (i = 8; i > 0; --i) {
    _delay_loop_2(serial_delay);
    b >>= 1;
    if ((*serial_in & serial_bit))
      b |= 0x80;
  }
  /* parity */
  _delay_loop_2(serial_delay);
  /* move into (first) stop bit */
  _delay_loop_2(serial_delay);

  return b;
}

void setup() {
  uint8_t byte, ver;

#ifdef STATUS_LED
  pinMode(STATUS_LED, OUTPUT);
#endif

  /* initial eeprom init */
  byte = eeprom_read(EEPROM_MAGIC + 0);
  ver = eeprom_read(EEPROM_MAGIC + 1);
  if (byte != 'C' || ver != '0' + EEPROM_VERSION)
    sim_init_eeprom();

#ifdef RESET_PIN
  reset_init(RESET_PIN);
#endif
  serial_init(SERIAL_DATA, SERIAL_DELAY);
#ifdef TEST_TX
  while (true)
    serial_tx(0x55);
#endif
#ifdef TEST_RX
  *serial_mode |= serial_bit; /* output */
  while (true) {
    /* show low for start bit up to end of first stop bit */
    *serial_out &= ~serial_bit; /* low */
    serial_rx();
    _delay_loop_2(serial_delay >> 1);
    *serial_out |= serial_bit; /* high */
    _delay_loop_2(serial_delay);
  }
#endif
#ifdef TEST_TO
  uint16_t to;
  int rx;
  rx_again:
  rx = 1;
  /* wait until start bit is received or timeout */
  for (to = 0; to <= SERIAL_TIMEOUT;) {
    if (!(*serial_in & serial_bit)) {
      serial_tx(0x33);
      goto rx_again;
    }
#ifdef RESET_PIN
    if (!(*reset_in & reset_bit)) {
      serial_tx(0xf0);
      goto rx_again;
    }
#endif
    if (rx)
      to++;
  }
  serial_tx(0x55);
  goto rx_again;
#endif
}

void loop() {
#if !defined(TEST_TX) && !defined(TEST_RX) && !defined (TEST_TO)
  uint16_t to;
  int c, rx;

reset_again:
#ifdef RESET_PIN
  /* wait until reset is released */
  while(!(*reset_in & reset_bit));
#endif
  sim_reset(&sim, 0);

tx_again:
#ifdef STATUS_LED
  digitalWrite(STATUS_LED, LOW);
#endif
  /* send buffer until no more data to be transmitted */
  serial_start_tx();
  while ((c = sim_tx(&sim)) >= 0) {
#ifdef RESET_PIN
    /* perform reset, when low */
    if (!(*reset_in & reset_bit))
      goto reset_again;
#endif
    /* perform transmission of a byte */
    serial_tx(c);
  }
  /* wait until start bit is received or timeout */
  rx = 0;
  for (to = 0; to <= SERIAL_TIMEOUT;) {
    /* perform RX, when low (start bit) */
    if (!(*serial_in & serial_bit)) {
      c = serial_rx();
      /* if block was completely received, go to tx_again */
      if (sim_rx(&sim, c) < 0)
	      goto tx_again;
      /* start counting timeout condition */
      rx = 1;
      to = 0;
#ifdef STATUS_LED
      digitalWrite(STATUS_LED, HIGH);
#endif
    }
#ifdef RESET_PIN
    /* perform reset, when low */
    if (!(*reset_in & reset_bit))
      goto reset_again;
#endif
    /* only if we have an ongoing reception, we count for the timeout condition */
    if (rx)
      to++;
  }
  /* perform timeout */
  sim_timeout(&sim);
  goto tx_again;
#endif
}
