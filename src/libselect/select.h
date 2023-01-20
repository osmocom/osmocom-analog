
#define OSMO_FD_READ    0x0001
#define OSMO_FD_WRITE   0x0002
#define OSMO_FD_EXCEPT  0x0004
#define BSC_FD_READ    0x0001
#define BSC_FD_WRITE   0x0002
#define BSC_FD_EXCEPT  0x0004

struct osmo_fd {
	struct osmo_fd *next;
	int fd;
	unsigned int when;
	int (*cb)(struct osmo_fd *fd, unsigned int what);
	void *data;
};

int osmo_fd_register(struct osmo_fd *ofd);
void osmo_fd_unregister(struct osmo_fd *ofd);
int osmo_fd_select(double timeout);

