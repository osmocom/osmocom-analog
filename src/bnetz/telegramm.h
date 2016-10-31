
struct impulstelegramm {
	int digit;
	const char *sequence;
	uint16_t telegramm;
	const char *description;
};

void bnetz_init_telegramm(void);
struct impulstelegramm *bnetz_digit2telegramm(int digit);
struct impulstelegramm *bnetz_telegramm2digit(uint16_t telegramm);

