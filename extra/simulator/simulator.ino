/* SIM card for ATMEL
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

/* NOTE: Reset must be an input pin, not the reset of the controller.
 * The clock pulse must be detected during reset.
 */
#if defined(__AVR_ATtiny85__)
#define RST_PIN  2
#define CLK_PIN  3
#define DATA_PIN 4
#else
#define CLK_PIN  5
#define RST_PIN  6
#define DATA_PIN 7
#endif

uint8_t card_data[] = {
/* Example: Service card for AEG OLYMPIA. */
0xff, 0xf7, 0x5c, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x47, 0x38,
0x78, 0x28, 0x07, 0x8c, 0xc7, 0x03, 0xfe, 0xfd,
0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
};

volatile uint8_t *rst_in, *clk_in, *data_in, *data_out, *data_mode;
uint8_t rst_bit, clk_bit, data_bit;

void setup()
{
  volatile uint8_t *out, *mode;
  uint8_t port;

  rst_bit = digitalPinToBitMask(RST_PIN);
  port = digitalPinToPort(RST_PIN);
  mode = portModeRegister(port);
  out = portOutputRegister(port);
  rst_in = portInputRegister(port);
  *mode &= ~rst_bit; /* input */
  *out |= rst_bit; /* pullup */

  clk_bit = digitalPinToBitMask(CLK_PIN);
  port = digitalPinToPort(CLK_PIN);
  mode = portModeRegister(port);
  out = portOutputRegister(port);
  clk_in = portInputRegister(port);
  *mode &= ~clk_bit; /* input */
  *out |= clk_bit; /* pullup */

  data_bit = digitalPinToBitMask(DATA_PIN);
  port = digitalPinToPort(DATA_PIN);
  data_mode = portModeRegister(port);
  data_out = portOutputRegister(port);
  data_in = portInputRegister(port);
  *data_mode |= data_bit; /* output */
  /* wait for reset */
  while (!(*rst_in & rst_bit));
}

uint8_t byte_count;
uint8_t bit_count;

void loop()
{
reset:
  /* initial reset state */
  byte_count = 0;
  bit_count = 0;
  *data_out |= data_bit; /* high */
  /* now we have reset, so we wait for the first clock pulse */
  while (!(*clk_in & clk_bit));
  while ((*clk_in & clk_bit));
  /* wait for reset to become low, if not already before first clock pulse (AEG phone) */
  while ((*rst_in & rst_bit));

next_bit:
  /* present bit */
  if ((card_data[byte_count] >> bit_count) & 1)
    *data_out |= data_bit; /* high */
  else
    *data_out &= ~data_bit; /* low */
  if (++bit_count == 8) {
    bit_count = 0;
    if (++byte_count == sizeof(card_data)) {
      goto reset;
    }
  }

  /* wait for clock pulse */
  while (!(*clk_in & clk_bit)) {
    /* reset counter if reset was detected */
    if ((*rst_in & rst_bit)) {
      goto reset;
    }
  }
  while ((*clk_in & clk_bit)) {
    /* reset counter if reset was detected */
    if ((*rst_in & rst_bit)) {
      goto reset;
    }
  }
  goto next_bit;
}
