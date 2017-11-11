#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "../common/sample.h"
#include "../common/debug.h"
#include "../common/timer.h"
#include "../nmt/nmt.h"

static const uint8_t test_mo_sms_data1[] = {
	0x00, 0x00, 0x00, 0xa1, 0x41, 0x0f, 0x11,
	0x00, 0x04, 0xa1, 0x8a, 0x51,
	0x00, 0x00, 0xff, 0x05, 0xc8, 0x20, 0x93,
	0xf9, 0x7c,
};
static const uint8_t test_mo_sms_data2[] = {
	0x00, 0x02, 0x07, 0xa1, 0xa9, 0x62, 0x65,
	0xf4, 0x41, 0x10, 0x11, 0x02, 0x03, 0xa1,
	0x21, 0xf3,
	0x00, 0x30, 0xff, 0x06, 0x48, 0x61, 0x6c,
	0x6c, 0x6f, 0x21,
};

static const char *test_mo_sms_text1 = "HALLO";
static const char *test_mo_sms_text2 = "Hallo!";

static const char *test_mt_sms_text = "Moin Moin";
static const char *test_mt_sms_tel = "4948416068";
static time_t test_mt_sms_time = 851430904;

static const uint8_t test_mt_sms_data[] = {
	0x01, 0x18, 0x53, 0x4d, 0x53, 0x48, 0x18, 0x41, 0x42, 0x43, 0x02,
	0x01,
	0x01,
	0x41,
	0x1a,
	0x04,
	0x0a, 0x91, 0x94, 0x84, 0x14, 0xa6, 0x86,
	0x00,
	0x00,
	0x69, 0x21, 0x42, 0x31, 0x53, 0x4a, 0x48,
	0x09, 0xcd, 0x77, 0xda, 0x0d, 0x6a, 0xbe, 0xd3, 0x6e,
};

static void assert(int condition, char *why)
{
	printf("%s = %s\n", why, (condition) ? "TRUE" : "FALSE");

	if (!condition) {
		printf("\n******************** FAILED ********************\n\n");
		exit(-1);
	}
}

void ok(void)
{
	printf("\n OK ;->\n\n");
	sleep(1);
}

static uint8_t dms_buffer[256];
static int dms_buffer_count;
void dms_send(nmt_t *nmt, const uint8_t *data, int length, int eight_bits)
{
//	int i;

	/* skip deliver report */
	if (length == 13)
		return;

	dms_buffer_count = length;
	memcpy(dms_buffer, data, length);

	assert(length == sizeof(test_mt_sms_data), "Expecting SMS binary data length to match");
	assert(!memcmp(data, test_mt_sms_data, length), "Expecting SMS binary data to match");
//	for (i = 0; i < length; i++) {
//		printf("(0x%02x)\n", data[i]);
//	}
}

void sms_release(nmt_t *nmt)
{
	printf("(got release from SMS layer)\n");
}

int sms_submit(nmt_t *nmt, uint8_t ref, const char *orig_address, uint8_t orig_type, uint8_t orig_plan, int msg_ref, const char *dest_address, uint8_t dest_type, uint8_t dest_plan, const char *message)
{
	strcpy((char *)dms_buffer, message);
	dms_buffer_count = strlen(message);

	return 0;
}

void sms_deliver_report(nmt_t *nmt, uint8_t ref, int error, uint8_t cause)
{
	printf("(got deliver report from SMS layer)\n");
}

extern void main_mobile();

int main(void)
{
	nmt_t *nmt;
	int i;
	int rc;

	/* this is never called, it forces the linker to add mobile functions */
	if (debuglevel == -1000) main_mobile();

	debuglevel = DEBUG_DEBUG;

	nmt = calloc(sizeof(*nmt), 1);
	sms_init_sender(nmt);
	sms_reset(nmt);

	/* deliver */
	printf("(delivering SMS)\n");
	rc = sms_deliver(nmt, 1, test_mt_sms_tel, SMS_TYPE_INTERNATIONAL, SMS_PLAN_ISDN_TEL, test_mt_sms_time, test_mt_sms_text);
	assert(rc == 0, "Expecting sms_deliver() to return 0");

	sms_cleanup_sender(nmt);
	free(nmt);

	ok();

	nmt = calloc(sizeof(*nmt), 1);
	sms_init_sender(nmt);
	sms_reset(nmt);

	printf("(submitting SMS 7-bit encoded)\n");
	dms_buffer_count = 0;
	for (i = 0; i < sizeof(test_mo_sms_data1); i++)
		dms_receive(nmt, test_mo_sms_data1 + i, 1, 1);

	assert(dms_buffer_count == strlen(test_mo_sms_text1), "Expecting SMS text length to match");
	assert(!memcmp(dms_buffer, test_mo_sms_text1, dms_buffer_count), "Expecting SMS text to match");

	sms_cleanup_sender(nmt);
	free(nmt);

	ok();

	nmt = calloc(sizeof(*nmt), 1);
	sms_init_sender(nmt);
	sms_reset(nmt);

	printf("(submitting SMS 8-bit encoded)\n");
	dms_buffer_count = 0;
	for (i = 0; i < sizeof(test_mo_sms_data2); i++)
		dms_receive(nmt, test_mo_sms_data2 + i, 1, 1);

	assert(dms_buffer_count == strlen(test_mo_sms_text2), "Expecting SMS text length to match");
	assert(!memcmp(dms_buffer, test_mo_sms_text2, dms_buffer_count), "Expecting SMS text to match");

	sms_cleanup_sender(nmt);
	free(nmt);

	ok();

	return 0;
}

