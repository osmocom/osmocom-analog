/* character device link to libfuse
 *
 * (C) 2019 by Andreas Eversberg <jolly@eversberg.eu>
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

#define FUSE_USE_VERSION 30

#include <cuse_lowlevel.h>
#include <fuse_opt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../libdebug/debug.h"
#define __USE_GNU
#include <pthread.h>
#include <signal.h>

#include "fioc.h"
#include "device.h"

/* enable to heavily debug poll process */
//#define DEBUG_POLL

typedef struct device {
	struct device *next;
	void *inst;
	pthread_t thread;
	int thread_started, thread_stopped;
	const char *name;
	int major, minor;
	int (*open_cb)(void *inst, int flags);
	void (*close_cb)(void *inst);
	ssize_t (*read_cb)(void *inst, char *buf, size_t size, int flags);
	ssize_t (*write_cb)(void *inst, const char *buf, size_t size, int flags);
	ssize_t (*ioctl_get_cb)(void *inst, int cmd, void *buf, size_t out_bufsz);
	ssize_t (*ioctl_set_cb)(void *inst, int cmd, const void *buf, size_t in_bufsz);
	void (*flush_tx)(void *inst);
	void (*lock_cb)(void);
	void (*unlock_cb)(void);
	short poll_revents;
	struct fuse_pollhandle *poll_handle;
	/* handle read blocking */
	fuse_req_t read_req;
	size_t read_size;
	int read_flags;
	int read_locked;
	/* handle write blocking */
	fuse_req_t write_req;
	size_t write_size;
	char *write_buf;
	int write_flags;
	int write_locked;
} device_t;

static device_t *device_list = NULL;

static device_t *get_device_by_thread(void)
{
	device_t *device = device_list;
	pthread_t thread = pthread_self();

	while (device) {
		if (device->thread == thread)
			return device;
		device = device->next;
	}

	fprintf(stderr, "Our thread is unknown, please fix!\n");
	abort();
}

static void cuse_device_open(fuse_req_t req, struct fuse_file_info *fi)
{
	(void)fi;
	int rc;
	device_t *device = get_device_by_thread();

	device->lock_cb();
	rc = device->open_cb(device->inst, fi->flags);
	device->unlock_cb();

	if (rc < 0)
		fuse_reply_err(req, -rc);
	else
		fuse_reply_open(req, fi);
}

static void cuse_device_release(fuse_req_t req, struct fuse_file_info *fi)
{
	(void)fi;
	device_t *device = get_device_by_thread();

	device->lock_cb();
	device->close_cb(device->inst);
	device->unlock_cb();

	fuse_reply_err(req, 0);
}

static void cuse_read_interrupt(fuse_req_t req, void *data)
{
	(void)req;
	device_t *device = (device_t *)data;

	if (!device->read_locked)
		device->lock_cb();

	PDEBUG(DDEVICE, DEBUG_DEBUG, "%s received interrupt from client!\n", device->name);

	if (device->read_req) {
		device->read_req = NULL;
		fuse_reply_err(req, EINTR);
	}

	if (!device->read_locked)
		device->unlock_cb();
}

void device_read_available(void *inst)
{
	device_t *device = (device_t *)inst;
	ssize_t count;

	// we are locked by caller

	/* if enough data or if buffer is full */
	if (device->read_req) {
		char buf[device->read_size];
		count = device->read_cb(device->inst, buf, device->read_size, device->read_flags);
		/* still blocking, waiting for more... */
		if (count == -EAGAIN)
			return;
		fuse_reply_buf(device->read_req, buf, count);
		device->read_req = NULL;
	}
}
	
static void cuse_device_read(fuse_req_t req, size_t size, off_t off, struct fuse_file_info *fi)
{
	(void)off;
	(void)fi;
	ssize_t count;
	device_t *device = get_device_by_thread();

	if (size > 65536)
		size = 65536;
	char buf[size];

	device->lock_cb();

	if (device->read_req) {
		device->unlock_cb();
		PDEBUG(DDEVICE, DEBUG_ERROR, "%s: Got another read(), while first read() has not been replied, please fix.\n", device->name);
		fuse_reply_err(req, EBUSY);
		return;
	}

#ifdef DEBUG_POLL
	puts("read: before fn");
#endif
	count = device->read_cb(device->inst, buf, size, fi->flags);
#ifdef DEBUG_POLL
	puts("read: after fn");
#endif

	/* this means that we block until modem's read() returns 0 or positive value (in nonblocking io, we return -EAGAIN) */
	if (!(fi->flags & O_NONBLOCK) && count == -EAGAIN) {
		PDEBUG(DDEVICE, DEBUG_DEBUG, "%s has no data available, waiting for data, timer or interrupt.\n", device->name);

		device->read_req = req;
		device->read_size = size;
		device->read_flags = fi->flags;
		/* to prevent race condition, tell cuse_write_interrupt that we are already locked.
		 * (interrupt may have come before and will be processed by fuse_req_interrupt_func())
		 */
		device->read_locked = 1;
		fuse_req_interrupt_func(req, cuse_read_interrupt, device);
		device->read_locked = 0;
		device->unlock_cb();
		return;
	}

	device->unlock_cb();

	if (count < 0)
		fuse_reply_err(req, -count);
	else
		fuse_reply_buf(req, buf, count);
#ifdef DEBUG_POLL
	puts("read: after reply");
#endif
}

static void cuse_write_interrupt(fuse_req_t req, void *data)
{
	(void)req;
	device_t *device = (device_t *)data;

	if (!device->write_locked)
		device->lock_cb();

	PDEBUG(DDEVICE, DEBUG_DEBUG, "%s received interrupt from client!\n", device->name);

	if (device->write_req) {
		device->write_req = NULL;
		free(device->write_buf);
		device->write_buf = NULL;
		/* flushing TX buffer */
		device->flush_tx(device->inst);
		fuse_reply_err(req, EINTR);
	}

	if (!device->write_locked)
		device->unlock_cb();
}

void device_write_available(void *inst)
{
	device_t *device = (device_t *)inst;
	ssize_t count;

	// we are locked by caller

	/* if enough space or buffer empty */
	if (device->write_req) {
		count = device->write_cb(device->inst, device->write_buf, device->write_size, device->write_flags);
		/* still blocking, waiting for more... */
		if (count == -EAGAIN)
			return;
		fuse_reply_write(device->write_req, count);
		device->write_req = NULL;
		free(device->write_buf);
		device->write_buf = NULL;
	}
}
	
static void cuse_device_write(fuse_req_t req, const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
	(void)off;
	(void)fi;
	ssize_t count;
	device_t *device = get_device_by_thread();

	device->lock_cb();

	if (device->write_req) {
		device->unlock_cb();
		PDEBUG(DDEVICE, DEBUG_ERROR, "%s: Got another write(), while first write() has not been replied, please fix.\n", device->name);
		fuse_reply_err(req, EBUSY);
		return;
	}

	count = device->write_cb(device->inst, buf, size, fi->flags);

	/* this means that we block until modem's write() returns 0 or positive value (in nonblocking io, we return -EAGAIN) */
	if (!(fi->flags & O_NONBLOCK) && count == -EAGAIN) {
		PDEBUG(DDEVICE, DEBUG_DEBUG, "%s has no buffer space available, waiting for space or interrupt.\n", device->name);

		device->write_req = req;
		device->write_size = size;
		device->write_buf = malloc(size);
		if (!buf) {
			PDEBUG(DDEVICE, DEBUG_ERROR, "No memory!\n");
			exit(0);
		}
		memcpy(device->write_buf, buf, size);
		device->write_flags = fi->flags;
		/* to prevent race condition, tell cuse_write_interrupt that we are already locked.
		 * (interrupt may have come before and will be processed by fuse_req_interrupt_func())
		 */
		device->write_locked = 1;
		fuse_req_interrupt_func(req, cuse_write_interrupt, device);
		device->write_locked = 0;
		device->unlock_cb();
		return;
	}

	device->unlock_cb();

	if (count < 0)
		fuse_reply_err(req, -count);
	else
		fuse_reply_write(req, count);
}

static void cuse_device_ioctl(fuse_req_t req, int cmd, void *arg, struct fuse_file_info *fi, unsigned flags, const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	(void)fi;
	ssize_t rc;
	char out_buf[out_bufsz];
	device_t *device = get_device_by_thread();

	if (flags & FUSE_IOCTL_COMPAT) {
		fuse_reply_err(req, ENOSYS);
		return;
	}

	switch (cmd) {
	case TCGETS:
	case TIOCMGET:
	case TIOCGWINSZ:
	case FIONREAD:
	case TIOCOUTQ:
		device->lock_cb();
		rc = device->ioctl_get_cb(device->inst, cmd, out_buf, out_bufsz);
		device->unlock_cb();
		if (rc < 0) {
			fuse_reply_err(req, -rc);
			break;
		}
		if (rc == 0) {
			// do we need this ?
			fuse_reply_ioctl(req, 0, NULL, 0);
			break;
		}
		if (!out_bufsz) {
			struct iovec iov = { arg, rc };

			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		} else {
			fuse_reply_ioctl(req, 0, out_buf, rc);
		}
		break;
	case TCSETS:
	case TCSETSW:
	case TCSETSF:
	case TIOCMBIS:
	case TIOCMBIC:
	case TIOCMSET:
	case TCFLSH:
	case TCSBRK:
	case TCSBRKP:
	case TIOCSBRK:
	case TIOCCBRK:
	case TIOCGSID:
	case TIOCGPGRP:
	case TIOCSCTTY:
	case TIOCSPGRP:
	case TIOCSWINSZ:
	case TCXONC:
		device->lock_cb();
		rc = device->ioctl_set_cb(device->inst, cmd, in_buf, in_bufsz);
		device->unlock_cb();
		if (rc < 0) {
			fuse_reply_err(req, -rc);
			break;
		}
		if (rc == 0) {
			/* empty control is replied */
			fuse_reply_ioctl(req, 0, NULL, 0);
			break;
		}
		if (!in_bufsz) {
			struct iovec iov = { arg, rc };

			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			fuse_reply_ioctl(req, 0, NULL, 0);
		}
		break;
	default:
		PDEBUG(DDEVICE, DEBUG_NOTICE, "%s: receives unknown ioctl: 0x%x\n", device->name, cmd);
		fuse_reply_err(req, EINVAL);
	}
}

void device_set_poll_events(void *inst, short revents)
{
	device_t *device = (device_t *)inst;

	// we are locked by caller

	if (revents == device->poll_revents)
		return;

#ifdef DEBUG_POLL
	printf("new revents 0x%x\n", revents);
#endif
	device->poll_revents = revents;
	if (device->poll_handle) {
#ifdef DEBUG_POLL
		printf("notify with handle %p\n", device->poll_handle);
#endif
		fuse_lowlevel_notify_poll(device->poll_handle);
	}
}

static void cuse_device_poll(fuse_req_t req, struct fuse_file_info *fi, struct fuse_pollhandle *ph)
{
	(void)fi;
	device_t *device = get_device_by_thread();

#ifdef DEBUG_POLL
	printf("poll %p %p\n", ph, device->poll_handle);
#endif
	if (ph) {
		device->lock_cb();
		if (device->poll_handle)
			fuse_pollhandle_destroy(device->poll_handle);
		device->poll_handle = ph;
#ifdef DEBUG_POLL
		printf("storing %p\n", device->poll_handle);
#endif
		device->unlock_cb();
	}

#ifdef DEBUG_POLL
	printf("sending revents 0x%x\n", device->poll_revents);
#endif
	fuse_reply_poll(req, device->poll_revents);
}

static void cuse_device_flush(fuse_req_t req, struct fuse_file_info *fi)
{
	(void)req;
	(void)fi;
	device_t *device = get_device_by_thread();
	PDEBUG(DDEVICE, DEBUG_NOTICE, "%s: unhandled flush\n", device->name);
}

static void cuse_device_fsync(fuse_req_t req, int datasync, struct fuse_file_info *fi)
{
	(void)req;
	(void)datasync;
	(void)fi;
	device_t *device = get_device_by_thread();
	PDEBUG(DDEVICE, DEBUG_NOTICE, "%s: unhandled fsync\n", device->name);
}


static const struct cuse_lowlevel_ops cuse_device_clop = {
	.open		= cuse_device_open,
	.release	= cuse_device_release,
	.read		= cuse_device_read,
	.write		= cuse_device_write,
	.ioctl		= cuse_device_ioctl,
	.poll		= cuse_device_poll,
	.fsync		= cuse_device_fsync,
	.flush		= cuse_device_flush,
};

static void *device_child(void *arg)
{
	device_t *device = (device_t *)arg;

	int argc = 3;
	/* use -f to run without debug, but -d to debug */
	char *argv[3] = { "datenklo", "-f", "-s" };
	char dev_name[128] = "DEVNAME=";
	const char *dev_info_argv[] = { dev_name };
	struct cuse_info ci;

	strncat(dev_name, device->name, sizeof(dev_name) - strlen(dev_name) - 1);

	memset(&ci, 0, sizeof(ci));
	ci.dev_major = device->major;
	ci.dev_minor = device->minor;
	ci.dev_info_argc = 1;
	ci.dev_info_argv = dev_info_argv;
	ci.flags = CUSE_UNRESTRICTED_IOCTL;

	device->thread_started = 1;

	PDEBUG(DDEVICE, DEBUG_INFO, "Device '%s' started.\n", device->name);
	cuse_lowlevel_main(argc, argv, &ci, &cuse_device_clop, NULL);
	PDEBUG(DDEVICE, DEBUG_INFO, "Device '%s' terminated.\n", device->name);

	device->thread_stopped = 1;

	return NULL;
}

void *device_init(void *inst, const char *name, int (*open)(void *inst, int flags), void (*close)(void *inst), ssize_t (*read)(void *inst, char *buf, size_t size, int flags), ssize_t (*write)(void *inst, const char *buf, size_t size, int flags), ssize_t ioctl_get(void *inst, int cmd, void *buf, size_t out_bufsz), ssize_t ioctl_set(void *inst, int cmd, const void *buf, size_t in_bufsz), void (*flush_tx)(void *inst), void (*lock)(void), void (*unlock)(void))
{
	int rc = -EINVAL;
	char tname[64];
	device_t *device = NULL;
	device_t **devicep;

	device = calloc(1, sizeof(*device));
	if (!device) {
		PDEBUG(DDEVICE, DEBUG_ERROR, "No memory!\n");
		errno = ENOMEM;
		goto error;
	}
	device->inst = inst;
	device->name = name;
	device->open_cb = open;
	device->close_cb = close;
	device->read_cb = read;
	device->write_cb = write;
	device->ioctl_get_cb = ioctl_get;
	device->ioctl_set_cb = ioctl_set;
	device->flush_tx = flush_tx;
	device->lock_cb = lock;
	device->unlock_cb = unlock;

	rc = pthread_create(&device->thread, NULL, device_child, device);
	if (rc < 0) {
		PDEBUG(DDEVICE, DEBUG_ERROR, "Failed to create device thread!\n");
		errno = -rc;
		goto error;
	}

	pthread_getname_np(device->thread, tname, sizeof(tname));
	strncat(tname, "-device", sizeof(tname) - strlen(tname) - 1);
	tname[sizeof(tname) - 1] = '\0';
	pthread_setname_np(device->thread, tname);

	while (!device->thread_started)
		usleep(100);

	/* attach to list */
	devicep = &device_list;
	while (*devicep)
		devicep = &((*devicep)->next);
	*devicep = device;

	return device;

error:
	device_exit(device);
	return NULL;
}

void device_exit(void *inst)
{
	device_t *device = (device_t *)inst;
	device_t **devicep;

	/* detach from list */
	devicep = &device_list;
	while (*devicep && *devicep != device)
		devicep = &((*devicep)->next);
	if (*devicep)
		*devicep = device->next;

	/* the device-thread is terminated when the program terminates, so no kill required (REALLY????) */

	free(device);
}

