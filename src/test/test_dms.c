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

extern int dms_allow_loopback;

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

static const char testsequence[] = "This is a test for DMS protocol layer. It will test the handing of transfer window. Also it will test what happens, if frames get dropped.";
static const char *check_sequence;
int check_length;

static const uint8_t test_null[][8] = {
	{ 0x01, 0x02, 0x02, 0x04, 0x05, 0x06, 0x07, 7 },
	{ 0x01, 0x02, 0x02, 0x04, 0x05, 0x06, 0x00, 6 },
	{ 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 5 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 1 },
};

static char current_bits[1024], ack_bits[77];
int current_bit_count;

void dms_receive(nmt_t *nmt, const uint8_t *data, int length, int eight_bits)
{
	printf("(getting %d digits from DMS layer)\n", length);

	assert(!memcmp((const char *)data, check_sequence, length), "Expecting received data to macht");

	check_sequence += length;
	check_length = length;
}

void dms_all_sent(nmt_t *nmt)
{
}

/* receive bits from DMS */
int fsk_render_frame(nmt_t *nmt, const char *frame, int length, sample_t *sample)
{
	printf("(getting %d bits from DMS layer)\n", length);

	memcpy(current_bits, frame, length);
	current_bit_count = length;

	return nmt->fsk_samples_per_bit * length;
}

nmt_t *alloc_nmt(void)
{
	nmt_t *nmt;

	nmt = calloc(sizeof(*nmt), 1);
	dms_init_sender(nmt);
	nmt->fsk_samples_per_bit = 40;
	nmt->dms.frame_size = nmt->fsk_samples_per_bit * 127 + 10;
	nmt->dms.frame_spl = calloc(nmt->dms.frame_size, sizeof(nmt->dms.frame_spl[0]));

	dms_reset(nmt);

	return nmt;
}

void free_nmt(nmt_t *nmt)
{
	dms_cleanup_sender(nmt);
	free(nmt->dms.frame_spl);
	free(nmt);
}

int main(void)
{
	nmt_t *nmt;
	dms_t *dms;
	int i, j;
	sample_t sample = 0;

	debuglevel = DEBUG_DEBUG;
	dms_allow_loopback = 1;

	nmt = alloc_nmt();
	dms = &nmt->dms;

	/* test if frame cycles until we send RAND */

	check_sequence = testsequence;
	dms_send(nmt, (uint8_t *)testsequence, strlen(testsequence) + 1, 1);
	assert(dms->frame_valid && current_bit_count == 127, "Expecting frame in queue with 127 bits");
	assert(dms->state.n_s == 1, "Expecting next frame to have sequence number 1");

	printf("Pretend that frame has been sent\n");
	dms->frame_length = 0;
	fsk_dms_frame(nmt, &sample, 1);

	assert(dms->frame_valid && current_bit_count == 127, "Expecting frame in queue with 127 bits");
	assert(dms->state.n_s == 0, "Expecting next frame to have sequence number 0 (cycles due to unacked RAND)");

	printf("Pretend that frame has been sent\n");
	dms->frame_length = 0;
	fsk_dms_frame(nmt, &sample, 1);

	assert(dms->frame_valid && current_bit_count == 127, "Expecting frame in queue with 127 bits");
	assert(dms->state.n_s == 1, "Expecting next frame to have sequence number 1");

	/* send back ID */

	printf("Sending back ID\n");
	for (i = 0; i < current_bit_count; i++)
		fsk_receive_bit_dms(nmt, current_bits[i] & 1, 1.0, 1.0);

	printf("Pretend that frame has been sent\n");
	dms->frame_length = 0;
	fsk_dms_frame(nmt, &sample, 1);

	assert(dms->frame_valid && current_bit_count == 77, "Expecting frame in queue with 77 bits");

	printf("Pretend that frame has been sent\n");
	dms->frame_length = 0;
	fsk_dms_frame(nmt, &sample, 1);

	assert(dms->frame_valid && current_bit_count == 127, "Expecting frame in queue with 127 bits");
	assert(dms->state.n_s == 0, "Expecting next frame to have sequence number 0");

	/* send back RAND */
	printf("Sending back RAND\n");
	for (i = 0; i < current_bit_count; i++)
		fsk_receive_bit_dms(nmt, current_bits[i] & 1, 1.0, 1.0);

	printf("Pretend that frame has been sent\n");
	dms->frame_length = 0;
	fsk_dms_frame(nmt, &sample, 1);

	assert(dms->frame_valid && current_bit_count == 77, "Expecting frame in queue with 77 bits");
	memcpy(ack_bits, current_bits, 77);

	/* check if DT frame will be sent now */

	printf("Pretend that frame has been sent\n");
	dms->frame_length = 0;
	fsk_dms_frame(nmt, &sample, 1);

	assert(dms->frame_valid && current_bit_count == 127, "Expecting frame in queue with 127 bits");
	assert(dms->state.n_s == 1, "Expecting next frame to have sequence number 1");

	printf("Pretend that frame has been sent\n");
	dms->frame_length = 0;
	fsk_dms_frame(nmt, &sample, 1);

	assert(dms->frame_valid && current_bit_count == 127, "Expecting frame in queue with 127 bits");
	assert(dms->state.n_s == 2, "Expecting next frame to have sequence number 2");

	printf("Pretend that frame has been sent\n");
	dms->frame_length = 0;
	fsk_dms_frame(nmt, &sample, 1);

	assert(dms->frame_valid && current_bit_count == 127, "Expecting frame in queue with 127 bits");
	assert(dms->state.n_s == 3, "Expecting next frame to have sequence number 3");

	printf("Pretend that frame has been sent\n");
	dms->frame_length = 0;
	fsk_dms_frame(nmt, &sample, 1);

	assert(dms->frame_valid && current_bit_count == 127, "Expecting frame in queue with 127 bits");
	assert(dms->state.n_s == 0, "Expecting next frame to have sequence number 0");

	/* send back ack bitss */
	printf("Sending back RR(2)\n");
	memcpy(current_bits, ack_bits, 77);
	current_bit_count = 77;
	for (i = 0; i < current_bit_count; i++)
		fsk_receive_bit_dms(nmt, current_bits[i] & 1, 1.0, 1.0);

	printf("Pretend that frame has been sent\n");
	dms->frame_length = 0;
	fsk_dms_frame(nmt, &sample, 1);

	assert(dms->frame_valid && current_bit_count == 127, "Expecting frame in queue with 127 bits");
	assert(dms->state.n_s == 3, "Expecting next frame to have sequence number 0");

	ok();

	/* loopback frames */
	printf("pipe through all data\n");
	while (check_sequence[0])  {
		printf("Sending back last received frame\n");
		for (i = 0; i < current_bit_count; i++)
			fsk_receive_bit_dms(nmt, current_bits[i] & 1, 1.0, 1.0);
		printf("Pretend that frame has been sent\n");
		dms->frame_length = 0;
		fsk_dms_frame(nmt, &sample, 1);
	}

	ok();

	debuglevel = DEBUG_INFO;

	/* test again with pseudo random packet dropps */
	srandom(0);
	free_nmt(nmt);
	nmt = alloc_nmt();
	dms = &nmt->dms;

	check_sequence = testsequence;
	dms_send(nmt, (uint8_t *)testsequence, strlen(testsequence) + 1, 1);

	/* loopback frames */
	printf("pipe through all data\n");
	while (check_sequence[0])  {
		if ((random() & 1)) {
			printf("Sending back last received frame\n");
			for (i = 0; i < current_bit_count; i++)
				fsk_receive_bit_dms(nmt, current_bits[i] & 1, 1.0, 1.0);
		}
		printf("Pretend that frame has been sent\n");
		dms->frame_length = 0;
		fsk_dms_frame(nmt, &sample, 1);
	}

	ok();

	free_nmt(nmt);
	nmt = alloc_nmt();
	dms = &nmt->dms;

	/* test zero termination */
	for (j = 0; j < 4; j++) {
		current_bit_count = 0;
		printf("zero-termination test: %d bytes in frame\n", test_null[j][7]);
		dms_send(nmt, test_null[j], test_null[j][7], 1);
		check_sequence = (char *)test_null[j];

		while (current_bit_count) {
			printf("Sending back last received frame\n");
			for (i = 0; i < current_bit_count; i++)
				fsk_receive_bit_dms(nmt, current_bits[i] & 1, 1.0, 1.0);
			current_bit_count = 0;
			printf("Pretend that frame has been sent\n");
			dms->frame_length = 0;
			fsk_dms_frame(nmt, &sample, 1);
		}
		assert(check_length == test_null[j][7], "Expecting received length to match transmitted length");
	}

	ok();

	return 0;
}

