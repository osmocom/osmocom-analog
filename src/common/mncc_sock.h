#include "mncc.h"

int mncc_write(uint8_t *buf, int length);
void mncc_handle(void);
void mncc_sock_close(void);
int mncc_init(const char *sock_name);
void mncc_exit(void);

