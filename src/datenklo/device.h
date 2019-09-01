
void *device_init(void *inst, const char *name, int (*open)(void *inst, int flags), void (*close)(void *inst), ssize_t (*read)(void *inst, char *buf, size_t size, int flags), ssize_t (*write)(void *inst, const char *buf, size_t size, int flags), ssize_t ioctl_get(void *inst, int cmd, void *buf, size_t out_bufsz), ssize_t ioctl_set(void *inst, int cmd, const void *buf, size_t in_bufsz), void (*flush_tx)(void *inst), void (*lock)(void), void (*unlock)(void));
void device_exit(void *inst);
void device_set_poll_events(void *inst, short revents);
void device_read_available(void *inst);
void device_write_available(void *inst);

