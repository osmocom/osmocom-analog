/*****************************************************************************\
**                                                                           **
** PBX4Linux                                                                 **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Andreas Eversberg (GPL)                                        **
**                                                                           **
** audio conversions for alaw and ulaw                                       **
**                                                                           **
\*****************************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* ulaw -> signed 16-bit */
static int16_t g711_ulaw_to_linear[256] =
{
	0x8284, 0x8684, 0x8a84, 0x8e84, 0x9284, 0x9684, 0x9a84, 0x9e84,
	0xa284, 0xa684, 0xaa84, 0xae84, 0xb284, 0xb684, 0xba84, 0xbe84,
	0xc184, 0xc384, 0xc584, 0xc784, 0xc984, 0xcb84, 0xcd84, 0xcf84,
	0xd184, 0xd384, 0xd584, 0xd784, 0xd984, 0xdb84, 0xdd84, 0xdf84,
	0xe104, 0xe204, 0xe304, 0xe404, 0xe504, 0xe604, 0xe704, 0xe804,
	0xe904, 0xea04, 0xeb04, 0xec04, 0xed04, 0xee04, 0xef04, 0xf004,
	0xf0c4, 0xf144, 0xf1c4, 0xf244, 0xf2c4, 0xf344, 0xf3c4, 0xf444,
	0xf4c4, 0xf544, 0xf5c4, 0xf644, 0xf6c4, 0xf744, 0xf7c4, 0xf844,
	0xf8a4, 0xf8e4, 0xf924, 0xf964, 0xf9a4, 0xf9e4, 0xfa24, 0xfa64,
	0xfaa4, 0xfae4, 0xfb24, 0xfb64, 0xfba4, 0xfbe4, 0xfc24, 0xfc64,
	0xfc94, 0xfcb4, 0xfcd4, 0xfcf4, 0xfd14, 0xfd34, 0xfd54, 0xfd74,
	0xfd94, 0xfdb4, 0xfdd4, 0xfdf4, 0xfe14, 0xfe34, 0xfe54, 0xfe74,
	0xfe8c, 0xfe9c, 0xfeac, 0xfebc, 0xfecc, 0xfedc, 0xfeec, 0xfefc,
	0xff0c, 0xff1c, 0xff2c, 0xff3c, 0xff4c, 0xff5c, 0xff6c, 0xff7c,
	0xff88, 0xff90, 0xff98, 0xffa0, 0xffa8, 0xffb0, 0xffb8, 0xffc0,
	0xffc8, 0xffd0, 0xffd8, 0xffe0, 0xffe8, 0xfff0, 0xfff8, 0xffff,
	0x7d7c, 0x797c, 0x757c, 0x717c, 0x6d7c, 0x697c, 0x657c, 0x617c,
	0x5d7c, 0x597c, 0x557c, 0x517c, 0x4d7c, 0x497c, 0x457c, 0x417c,
	0x3e7c, 0x3c7c, 0x3a7c, 0x387c, 0x367c, 0x347c, 0x327c, 0x307c,
	0x2e7c, 0x2c7c, 0x2a7c, 0x287c, 0x267c, 0x247c, 0x227c, 0x207c,
	0x1efc, 0x1dfc, 0x1cfc, 0x1bfc, 0x1afc, 0x19fc, 0x18fc, 0x17fc,
	0x16fc, 0x15fc, 0x14fc, 0x13fc, 0x12fc, 0x11fc, 0x10fc, 0x0ffc,
	0x0f3c, 0x0ebc, 0x0e3c, 0x0dbc, 0x0d3c, 0x0cbc, 0x0c3c, 0x0bbc,
	0x0b3c, 0x0abc, 0x0a3c, 0x09bc, 0x093c, 0x08bc, 0x083c, 0x07bc,
	0x075c, 0x071c, 0x06dc, 0x069c, 0x065c, 0x061c, 0x05dc, 0x059c,
	0x055c, 0x051c, 0x04dc, 0x049c, 0x045c, 0x041c, 0x03dc, 0x039c,
	0x036c, 0x034c, 0x032c, 0x030c, 0x02ec, 0x02cc, 0x02ac, 0x028c,
	0x026c, 0x024c, 0x022c, 0x020c, 0x01ec, 0x01cc, 0x01ac, 0x018c,
	0x0174, 0x0164, 0x0154, 0x0144, 0x0134, 0x0124, 0x0114, 0x0104,
	0x00f4, 0x00e4, 0x00d4, 0x00c4, 0x00b4, 0x00a4, 0x0094, 0x0084,
	0x0078, 0x0070, 0x0068, 0x0060, 0x0058, 0x0050, 0x0048, 0x0040,
	0x0038, 0x0030, 0x0028, 0x0020, 0x0018, 0x0010, 0x0008, 0x0000
};

/* alaw -> signed 16-bit */
static int16_t g711_alaw_flipped_to_linear[256] =
{
	0x13fc, 0xec04, 0x0144, 0xfebc, 0x517c, 0xae84, 0x051c, 0xfae4,
	0x0a3c, 0xf5c4, 0x0048, 0xffb8, 0x287c, 0xd784, 0x028c, 0xfd74,
	0x1bfc, 0xe404, 0x01cc, 0xfe34, 0x717c, 0x8e84, 0x071c, 0xf8e4,
	0x0e3c, 0xf1c4, 0x00c4, 0xff3c, 0x387c, 0xc784, 0x039c, 0xfc64,
	0x0ffc, 0xf004, 0x0104, 0xfefc, 0x417c, 0xbe84, 0x041c, 0xfbe4,
	0x083c, 0xf7c4, 0x0008, 0xfff8, 0x207c, 0xdf84, 0x020c, 0xfdf4,
	0x17fc, 0xe804, 0x018c, 0xfe74, 0x617c, 0x9e84, 0x061c, 0xf9e4,
	0x0c3c, 0xf3c4, 0x0084, 0xff7c, 0x307c, 0xcf84, 0x030c, 0xfcf4,
	0x15fc, 0xea04, 0x0164, 0xfe9c, 0x597c, 0xa684, 0x059c, 0xfa64,
	0x0b3c, 0xf4c4, 0x0068, 0xff98, 0x2c7c, 0xd384, 0x02cc, 0xfd34,
	0x1dfc, 0xe204, 0x01ec, 0xfe14, 0x797c, 0x8684, 0x07bc, 0xf844,
	0x0f3c, 0xf0c4, 0x00e4, 0xff1c, 0x3c7c, 0xc384, 0x03dc, 0xfc24,
	0x11fc, 0xee04, 0x0124, 0xfedc, 0x497c, 0xb684, 0x049c, 0xfb64,
	0x093c, 0xf6c4, 0x0028, 0xffd8, 0x247c, 0xdb84, 0x024c, 0xfdb4,
	0x19fc, 0xe604, 0x01ac, 0xfe54, 0x697c, 0x9684, 0x069c, 0xf964,
	0x0d3c, 0xf2c4, 0x00a4, 0xff5c, 0x347c, 0xcb84, 0x034c, 0xfcb4,
	0x12fc, 0xed04, 0x0134, 0xfecc, 0x4d7c, 0xb284, 0x04dc, 0xfb24,
	0x09bc, 0xf644, 0x0038, 0xffc8, 0x267c, 0xd984, 0x026c, 0xfd94,
	0x1afc, 0xe504, 0x01ac, 0xfe54, 0x6d7c, 0x9284, 0x06dc, 0xf924,
	0x0dbc, 0xf244, 0x00b4, 0xff4c, 0x367c, 0xc984, 0x036c, 0xfc94,
	0x0f3c, 0xf0c4, 0x00f4, 0xff0c, 0x3e7c, 0xc184, 0x03dc, 0xfc24,
	0x07bc, 0xf844, 0x0008, 0xfff8, 0x1efc, 0xe104, 0x01ec, 0xfe14,
	0x16fc, 0xe904, 0x0174, 0xfe8c, 0x5d7c, 0xa284, 0x05dc, 0xfa24,
	0x0bbc, 0xf444, 0x0078, 0xff88, 0x2e7c, 0xd184, 0x02ec, 0xfd14,
	0x14fc, 0xeb04, 0x0154, 0xfeac, 0x557c, 0xaa84, 0x055c, 0xfaa4,
	0x0abc, 0xf544, 0x0058, 0xffa8, 0x2a7c, 0xd584, 0x02ac, 0xfd54,
	0x1cfc, 0xe304, 0x01cc, 0xfe34, 0x757c, 0x8a84, 0x075c, 0xf8a4,
	0x0ebc, 0xf144, 0x00d4, 0xff2c, 0x3a7c, 0xc584, 0x039c, 0xfc64,
	0x10fc, 0xef04, 0x0114, 0xfeec, 0x457c, 0xba84, 0x045c, 0xfba4,
	0x08bc, 0xf744, 0x0018, 0xffe8, 0x227c, 0xdd84, 0x022c, 0xfdd4,
	0x18fc, 0xe704, 0x018c, 0xfe74, 0x657c, 0x9a84, 0x065c, 0xf9a4,
	0x0cbc, 0xf344, 0x0094, 0xff6c, 0x327c, 0xcd84, 0x032c, 0xfcd4
};

/* Xlaw -> signed 16-bit */
static int16_t g711_alaw_to_linear[256];
static int16_t g711_ulaw_flipped_to_linear[256];

/* signed 16-bit -> Xlaw */
static uint8_t g711_linear_to_alaw_flipped[65536];
static uint8_t g711_linear_to_ulaw_flipped[65536];
static uint8_t g711_linear_to_alaw[65536];
static uint8_t g711_linear_to_ulaw[65536];

/* transcode */
static uint8_t g711_alaw_to_ulaw[256];
static uint8_t g711_ulaw_to_alaw[256];
static uint8_t g711_alaw_flipped_to_ulaw[256];
static uint8_t g711_ulaw_flipped_to_alaw[256];
static uint8_t g711_alaw_to_ulaw_flipped[256];
static uint8_t g711_ulaw_to_alaw_flipped[256];

/* table is used to generate linear_to_alaw */
static int16_t g711_alaw_relations[] =
{
	0x8684, 0x55, 0x8a84, 0xd5, 0x8e84, 0x15, 0x9284, 0x95,
	0x9684, 0x75, 0x9a84, 0xf5, 0x9e84, 0x35, 0xa284, 0xb5,
	0xa684, 0x45, 0xaa84, 0xc5, 0xae84, 0x05, 0xb284, 0x85,
	0xb684, 0x65, 0xba84, 0xe5, 0xbe84, 0x25, 0xc184, 0xa5,
	0xc384, 0x5d, 0xc584, 0xdd, 0xc784, 0x1d, 0xc984, 0x9d,
	0xcb84, 0x7d, 0xcd84, 0xfd, 0xcf84, 0x3d, 0xd184, 0xbd,
	0xd384, 0x4d, 0xd584, 0xcd, 0xd784, 0x0d, 0xd984, 0x8d,
	0xdb84, 0x6d, 0xdd84, 0xed, 0xdf84, 0x2d, 0xe104, 0xad,
	0xe204, 0x51, 0xe304, 0xd1, 0xe404, 0x11, 0xe504, 0x91,
	0xe604, 0x71, 0xe704, 0xf1, 0xe804, 0x31, 0xe904, 0xb1,
	0xea04, 0x41, 0xeb04, 0xc1, 0xec04, 0x01, 0xed04, 0x81,
	0xee04, 0x61, 0xef04, 0xe1, 0xf004, 0x21, 0xf0c4, 0x59,
	0xf0c4, 0xa1, 0xf144, 0xd9, 0xf1c4, 0x19, 0xf244, 0x99,
	0xf2c4, 0x79, 0xf344, 0xf9, 0xf3c4, 0x39, 0xf444, 0xb9,
	0xf4c4, 0x49, 0xf544, 0xc9, 0xf5c4, 0x09, 0xf644, 0x89,
	0xf6c4, 0x69, 0xf744, 0xe9, 0xf7c4, 0x29, 0xf844, 0x57,
	0xf844, 0xa9, 0xf8a4, 0xd7, 0xf8e4, 0x17, 0xf924, 0x97,
	0xf964, 0x77, 0xf9a4, 0xf7, 0xf9e4, 0x37, 0xfa24, 0xb7,
	0xfa64, 0x47, 0xfaa4, 0xc7, 0xfae4, 0x07, 0xfb24, 0x87,
	0xfb64, 0x67, 0xfba4, 0xe7, 0xfbe4, 0x27, 0xfc24, 0x5f,
	0xfc24, 0xa7, 0xfc64, 0x1f, 0xfc64, 0xdf, 0xfc94, 0x9f,
	0xfcb4, 0x7f, 0xfcd4, 0xff, 0xfcf4, 0x3f, 0xfd14, 0xbf,
	0xfd34, 0x4f, 0xfd54, 0xcf, 0xfd74, 0x0f, 0xfd94, 0x8f,
	0xfdb4, 0x6f, 0xfdd4, 0xef, 0xfdf4, 0x2f, 0xfe14, 0x53,
	0xfe14, 0xaf, 0xfe34, 0x13, 0xfe34, 0xd3, 0xfe54, 0x73,
	0xfe54, 0x93, 0xfe74, 0x33, 0xfe74, 0xf3, 0xfe8c, 0xb3,
	0xfe9c, 0x43, 0xfeac, 0xc3, 0xfebc, 0x03, 0xfecc, 0x83,
	0xfedc, 0x63, 0xfeec, 0xe3, 0xfefc, 0x23, 0xff0c, 0xa3,
	0xff1c, 0x5b, 0xff2c, 0xdb, 0xff3c, 0x1b, 0xff4c, 0x9b,
	0xff5c, 0x7b, 0xff6c, 0xfb, 0xff7c, 0x3b, 0xff88, 0xbb,
	0xff98, 0x4b, 0xffa8, 0xcb, 0xffb8, 0x0b, 0xffc8, 0x8b,
	0xffd8, 0x6b, 0xffe8, 0xeb, 0xfff8, 0x2b, 0xfff8, 0xab,
	0x0008, 0x2a, 0x0008, 0xaa, 0x0018, 0xea, 0x0028, 0x6a,
	0x0038, 0x8a, 0x0048, 0x0a, 0x0058, 0xca, 0x0068, 0x4a,
	0x0078, 0xba, 0x0084, 0x3a, 0x0094, 0xfa, 0x00a4, 0x7a,
	0x00b4, 0x9a, 0x00c4, 0x1a, 0x00d4, 0xda, 0x00e4, 0x5a,
	0x00f4, 0xa2, 0x0104, 0x22, 0x0114, 0xe2, 0x0124, 0x62,
	0x0134, 0x82, 0x0144, 0x02, 0x0154, 0xc2, 0x0164, 0x42,
	0x0174, 0xb2, 0x018c, 0x32, 0x018c, 0xf2, 0x01ac, 0x72,
	0x01ac, 0x92, 0x01cc, 0x12, 0x01cc, 0xd2, 0x01ec, 0x52,
	0x01ec, 0xae, 0x020c, 0x2e, 0x022c, 0xee, 0x024c, 0x6e,
	0x026c, 0x8e, 0x028c, 0x0e, 0x02ac, 0xce, 0x02cc, 0x4e,
	0x02ec, 0xbe, 0x030c, 0x3e, 0x032c, 0xfe, 0x034c, 0x7e,
	0x036c, 0x9e, 0x039c, 0x1e, 0x039c, 0xde, 0x03dc, 0x5e,
	0x03dc, 0xa6, 0x041c, 0x26, 0x045c, 0xe6, 0x049c, 0x66,
	0x04dc, 0x86, 0x051c, 0x06, 0x055c, 0xc6, 0x059c, 0x46,
	0x05dc, 0xb6, 0x061c, 0x36, 0x065c, 0xf6, 0x069c, 0x76,
	0x06dc, 0x96, 0x071c, 0x16, 0x075c, 0xd6, 0x07bc, 0x56,
	0x07bc, 0xa8, 0x083c, 0x28, 0x08bc, 0xe8, 0x093c, 0x68,
	0x09bc, 0x88, 0x0a3c, 0x08, 0x0abc, 0xc8, 0x0b3c, 0x48,
	0x0bbc, 0xb8, 0x0c3c, 0x38, 0x0cbc, 0xf8, 0x0d3c, 0x78,
	0x0dbc, 0x98, 0x0e3c, 0x18, 0x0ebc, 0xd8, 0x0f3c, 0x58,
	0x0f3c, 0xa0, 0x0ffc, 0x20, 0x10fc, 0xe0, 0x11fc, 0x60,
	0x12fc, 0x80, 0x13fc, 0x00, 0x14fc, 0xc0, 0x15fc, 0x40,
	0x16fc, 0xb0, 0x17fc, 0x30, 0x18fc, 0xf0, 0x19fc, 0x70,
	0x1afc, 0x90, 0x1bfc, 0x10, 0x1cfc, 0xd0, 0x1dfc, 0x50,
	0x1efc, 0xac, 0x207c, 0x2c, 0x227c, 0xec, 0x247c, 0x6c,
	0x267c, 0x8c, 0x287c, 0x0c, 0x2a7c, 0xcc, 0x2c7c, 0x4c,
	0x2e7c, 0xbc, 0x307c, 0x3c, 0x327c, 0xfc, 0x347c, 0x7c,
	0x367c, 0x9c, 0x387c, 0x1c, 0x3a7c, 0xdc, 0x3c7c, 0x5c,
	0x3e7c, 0xa4, 0x417c, 0x24, 0x457c, 0xe4, 0x497c, 0x64,
	0x4d7c, 0x84, 0x517c, 0x04, 0x557c, 0xc4, 0x597c, 0x44,
	0x5d7c, 0xb4, 0x617c, 0x34, 0x657c, 0xf4, 0x697c, 0x74,
	0x6d7c, 0x94, 0x717c, 0x14, 0x757c, 0xd4, 0x797c, 0x54
};

uint8_t g711_flip[256];

static int g711_initialized = 0;

/* generate tables
 */
void g711_init(void)
{
	int i, j;

	/* flip tables */
	for (i = 0; i < 256; i++) {
		g711_flip[i]
			= ((i & 1) << 7)
			+ ((i & 2) << 5)
			+ ((i & 4) << 3)
			+ ((i & 8) << 1)
			+ ((i & 16) >> 1)
			+ ((i & 32) >> 3)
			+ ((i & 64) >> 5)
			+ ((i & 128) >> 7);
		g711_alaw_to_linear[i] = g711_alaw_flipped_to_linear[g711_flip[i]];
		g711_ulaw_flipped_to_linear[i] = g711_ulaw_to_linear[g711_flip[i]];
	}

	/* linear to alaw tables */
	i = j = 0;
	while(i < 65536) {
		if (i - 32768 > g711_alaw_relations[j << 1])
			j++;
		if (j > 255)
			j = 255;
		g711_linear_to_alaw_flipped[(i - 32768) & 0xffff] = g711_alaw_relations[(j << 1) | 1];
		g711_linear_to_alaw[(i - 32768) & 0xffff] = g711_flip[g711_alaw_relations[(j << 1) | 1]];
		i++;
	}

	/* linear to ulaw tables */
	i = j = 0;
	while(i < 32768) {
		if (i - 32768 > g711_ulaw_to_linear[j])
			j++;
		g711_linear_to_ulaw[(i - 32768) & 0xffff] = j;
		g711_linear_to_ulaw_flipped[(i - 32768) & 0xffff] = g711_flip[j];
		i++;
	}
	j = 255;
	while(i < 65536) {
		if (i - 32768 > g711_ulaw_to_linear[j])
			j--;
		g711_linear_to_ulaw[(i - 32768) & 0xffff] = j;
		g711_linear_to_ulaw_flipped[(i - 32768) & 0xffff] = g711_flip[j];
		i++;
	}

	/* transcode */
	for (i = 0; i < 256; i++) {
		g711_alaw_to_ulaw[i] = g711_linear_to_ulaw[(uint16_t)g711_alaw_to_linear[i]];
		g711_ulaw_to_alaw[i] = g711_linear_to_alaw[(uint16_t)g711_ulaw_to_linear[i]];
		g711_alaw_flipped_to_ulaw[i] = g711_linear_to_ulaw[(uint16_t)g711_alaw_to_linear[g711_flip[i]]];
		g711_ulaw_flipped_to_alaw[i] = g711_linear_to_alaw[(uint16_t)g711_ulaw_to_linear[g711_flip[i]]];
		g711_alaw_to_ulaw_flipped[i] = g711_flip[g711_linear_to_ulaw[(uint16_t)g711_alaw_to_linear[i]]];
		g711_ulaw_to_alaw_flipped[i] = g711_flip[g711_linear_to_alaw[(uint16_t)g711_ulaw_to_linear[i]]];
	}

	g711_initialized = 1;
}

void g711_encode_alaw_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
	int16_t *src = (int16_t *)src_data;
	uint8_t *dst;
	int len = src_len / 2, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

	dst = malloc(len);
	if (!dst)
		return;
	for (i = 0; i < len; i++)
		dst[i] = g711_linear_to_alaw_flipped[(uint16_t)src[i]];
	*dst_data = dst;
	*dst_len = len;
}

void g711_encode_ulaw_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
	int16_t *src = (int16_t *)src_data;
	uint8_t *dst;
	int len = src_len / 2, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

	dst = malloc(len);
	if (!dst)
		return;
	for (i = 0; i < len; i++)
		dst[i] = g711_linear_to_ulaw_flipped[(uint16_t)src[i]];
	*dst_data = dst;
	*dst_len = len;
}

void g711_decode_alaw_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
        uint8_t *src = src_data;
	int16_t *dst;
        int len = src_len, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

        dst = malloc(len * 2);
        if (!dst)
                return;
        for (i = 0; i < len; i++)
                dst[i] = g711_alaw_flipped_to_linear[src[i]];
        *dst_data = (uint8_t *)dst;
        *dst_len = len * 2;
}

void g711_decode_ulaw_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
        uint8_t *src = src_data;
	int16_t *dst;
        int len = src_len, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

        dst = malloc(len * 2);
        if (!dst)
                return;
        for (i = 0; i < len; i++)
                dst[i] = g711_ulaw_flipped_to_linear[src[i]];
        *dst_data = (uint8_t *)dst;
        *dst_len = len * 2;
}

void g711_encode_alaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
	int16_t *src = (int16_t *)src_data;
	uint8_t *dst;
	int len = src_len / 2, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

	dst = malloc(len);
	if (!dst)
		return;
	for (i = 0; i < len; i++)
		dst[i] = g711_linear_to_alaw[(uint16_t)src[i]];
	*dst_data = dst;
	*dst_len = len;
}

void g711_encode_ulaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
	int16_t *src = (int16_t *)src_data;
	uint8_t *dst;
	int len = src_len / 2, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

	dst = malloc(len);
	if (!dst)
		return;
	for (i = 0; i < len; i++)
		dst[i] = g711_linear_to_ulaw[(uint16_t)src[i]];
	*dst_data = dst;
	*dst_len = len;
}

void g711_decode_alaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
        uint8_t *src = src_data;
	int16_t *dst;
        int len = src_len, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

        dst = malloc(len * 2);
        if (!dst)
                return;
        for (i = 0; i < len; i++)
                dst[i] = g711_alaw_to_linear[src[i]];
        *dst_data = (uint8_t *)dst;
        *dst_len = len * 2;
}

void g711_decode_ulaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
        uint8_t *src = src_data;
	int16_t *dst;
        int len = src_len, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

        dst = malloc(len * 2);
        if (!dst)
                return;
        for (i = 0; i < len; i++)
                dst[i] = g711_ulaw_to_linear[src[i]];
        *dst_data = (uint8_t *)dst;
        *dst_len = len * 2;
}

void g711_transcode_alaw_to_ulaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
	uint8_t *src = src_data, *dst;
	int len = src_len, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

        dst = malloc(len);
        if (!dst)
                return;
        for (i = 0; i < len; i++)
                dst[i] = g711_alaw_to_ulaw[src[i]];
        *dst_data = dst;
        *dst_len = len;
}

void g711_transcode_alaw_flipped_to_ulaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
	uint8_t *src = src_data, *dst;
	int len = src_len, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

        dst = malloc(len);
        if (!dst)
                return;
        for (i = 0; i < len; i++)
                dst[i] = g711_alaw_flipped_to_ulaw[src[i]];
        *dst_data = dst;
        *dst_len = len;
}

void g711_transcode_alaw_to_ulaw_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
	uint8_t *src = src_data, *dst;
	int len = src_len, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

        dst = malloc(len);
        if (!dst)
                return;
        for (i = 0; i < len; i++)
                dst[i] = g711_alaw_to_ulaw_flipped[src[i]];
        *dst_data = dst;
        *dst_len = len;
}

void g711_transcode_ulaw_to_alaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
	uint8_t *src = src_data, *dst;
	int len = src_len, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

        dst = malloc(len);
        if (!dst)
                return;
        for (i = 0; i < len; i++)
                dst[i] = g711_ulaw_to_alaw[src[i]];
        *dst_data = dst;
        *dst_len = len;
}

void g711_transcode_ulaw_flipped_to_alaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
	uint8_t *src = src_data, *dst;
	int len = src_len, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

        dst = malloc(len);
        if (!dst)
                return;
        for (i = 0; i < len; i++)
                dst[i] = g711_ulaw_flipped_to_alaw[src[i]];
        *dst_data = dst;
        *dst_len = len;
}

void g711_transcode_ulaw_to_alaw_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
	uint8_t *src = src_data, *dst;
	int len = src_len, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

        dst = malloc(len);
        if (!dst)
                return;
        for (i = 0; i < len; i++)
                dst[i] = g711_ulaw_to_alaw_flipped[src[i]];
        *dst_data = dst;
        *dst_len = len;
}

void g711_transcode_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *priv)
{
	uint8_t *src = src_data, *dst;
	int len = src_len, i;

	if (!g711_initialized) {
		fprintf(stderr, "G711 codec not initialized! Please fix!\n");
		abort();
	}

        dst = malloc(len);
        if (!dst)
                return;
        for (i = 0; i < len; i++)
                dst[i] = g711_flip[src[i]];
        *dst_data = dst;
        *dst_len = len;
}

