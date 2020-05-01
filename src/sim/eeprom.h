
enum eeprom_locations {
	EEPROM_MAGIC		= 0x00,
	EEPROM_FUTLN_H		= 0x02,
	EEPROM_FUTLN_M		= 0x0a,
	EEPROM_FUTLN_L		= 0x12,
	EEPROM_SICH_H		= 0x1a,
	EEPROM_SICH_L		= 0x22,
	EEPROM_SONDER_H		= 0x2a,
	EEPROM_SONDER_L		= 0x32,
	EEPROM_WARTUNG_H	= 0x3a,
	EEPROM_WARTUNG_L	= 0x42,
	EEPROM_GEBZ_H		= 0x4a,
	EEPROM_GEBZ_M		= 0x4b,
	EEPROM_GEBZ_L		= 0x4c,
	EEPROM_FLAGS		= 0x4d,
	EEPROM_PIN_DATA		= 0x50,
	EEPROM_AUTH_DATA	= 0x58,
	EEPROM_RUFN		= 0x60,
};

#define EEPROM_VERSION		1	/* version eeprom layout */

#define EEPROM_FLAG_PIN_LEN	0	/* pin length */
#define EEPROM_FLAG_PIN_TRY	4	/* pin retires left */
#define EEPROM_FLAG_GEBZ	6	/* metering locked */
#define EEPROM_FLAG_APP		7	/* application locked */

uint8_t eeprom_read(enum eeprom_locations loc);
void eeprom_write(enum eeprom_locations loc, uint8_t value);
uint8_t *eeprom_memory(void);
size_t eeprom_length();

