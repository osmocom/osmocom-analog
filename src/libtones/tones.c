
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "tones.h"
#include "patterns.h"
#include <osmocom/cc/g711.h>

struct tones_sets_t {
	bool listed;
	const char *name;
	const char *description;
	tones_set_t *set;
} tones_sets[] = {
	{ true, "german", "German ISDN tones used back in the 90's and 2000's", set_german },
	{ true, "oldgerman", "German analog tones used back in the 80's and 90's", set_oldgerman },
	{ true, "morsegerman", "German analog tones used until the end of the 70's", set_morsegerman },
	{ true, "american", "American tones", set_american },
	{ true, "oldamerican", "Old American tones", set_oldamerican },
	{ true, "denmark", "Danish tones", set_denmark },
	{ true, "japan", "Japanese tones", set_japan },
	{ true, "france", "French tones", set_france },
	{ false, NULL, NULL, NULL },
};

void tones_list_tonesets(void)
{
	size_t max_name_length = 0;
	size_t max_description_length = 0;
	static char spaces[256];
	static char dashes[256];
	int i;

	memset(spaces, ' ', sizeof(spaces));
	memset(dashes, '-', sizeof(dashes));

	for (i = 0; tones_sets[i].name; i++) {
		if (!tones_sets[i].listed)
			continue;
		if (strlen(tones_sets[i].name) > max_name_length)
			max_name_length = strlen(tones_sets[i].name);
		if (strlen(tones_sets[i].description) > max_description_length)
			max_description_length = strlen(tones_sets[i].description);
	}

	printf("Tones%.*s|Description\n", (int)max_name_length - 5, spaces);
	printf("%.*s\n", (int)max_name_length + 1 + (int)max_description_length, dashes);
	for (i = 0; tones_sets[i].name; i++) {
		printf("%s%.*s|%s\n", tones_sets[i].name, (int)max_name_length - (int)strlen(tones_sets[i].name), spaces, tones_sets[i].description);
	}
}

static size_t sizeof_coding(enum tones_tdata coding)
{
	switch (coding) {
	case TONES_TDATA_SLIN16HOST:
		return 2;
	case TONES_TDATA_ALAW:
	case TONES_TDATA_ULAW:
	case TONES_TDATA_ALAWFLIPPED:
	case TONES_TDATA_ULAWFLIPPED:
		return 1;
	case TONES_TDATA_EOL:
		break;
	}

	fprintf(stderr, "Illegal coding, please fix!\n");
	abort();
}

static int render_chunk(void *out_spl, enum tones_tdata out_coding, void *in_spl, enum tones_tdata in_coding, int duration, double db)
{
	uint8_t *temp = NULL, *temp2 = NULL;
	uint16_t *new_in_spl = NULL;
	int temp_size, temp_size2;
	double level, value;
	int s;

	/* Change level for linear samples */
	if (in_coding == TONES_TDATA_SLIN16HOST && db) {
		level = pow(10.0, db / 20.0);
		new_in_spl = malloc(duration * 2);
		for (s = 0; s < duration; s++) {
			value = (double)(((int16_t *)in_spl)[s]) * level;
			if (value > 32767)
				value = 32767;
			if (value < -32767)
				value = -32767;
			new_in_spl[s] = (int16_t)value;
		}
		in_spl = new_in_spl;
	}

	switch (out_coding) {
	case TONES_TDATA_SLIN16HOST:
		switch (in_coding) {
		case TONES_TDATA_SLIN16HOST:
			memcpy(out_spl, in_spl, duration * 2);
			break;
		case TONES_TDATA_ALAW:
			g711_decode_alaw(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration * 2);
			break;
		case TONES_TDATA_ULAW:
			g711_decode_ulaw(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration * 2);
			break;
		case TONES_TDATA_ALAWFLIPPED:
			g711_decode_alaw_flipped(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration * 2);
			break;
		case TONES_TDATA_ULAWFLIPPED:
			g711_decode_ulaw_flipped(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration * 2);
			break;
		case TONES_TDATA_EOL:
			return -EINVAL;
		}
		break;
	case TONES_TDATA_ALAW:
		switch (in_coding) {
		case TONES_TDATA_SLIN16HOST:
			g711_encode_alaw(in_spl, duration * 2, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_ALAW:
			memcpy(out_spl, in_spl, duration);
			break;
		case TONES_TDATA_ULAW:
			g711_transcode_ulaw_to_alaw(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_ALAWFLIPPED:
			g711_transcode_flipped(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_ULAWFLIPPED:
			g711_transcode_ulaw_flipped_to_alaw(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_EOL:
			return -EINVAL;
		}
		break;
	case TONES_TDATA_ULAW:
		switch (in_coding) {
		case TONES_TDATA_SLIN16HOST:
			g711_encode_ulaw(in_spl, duration * 2, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_ALAW:
			g711_transcode_alaw_to_ulaw(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_ULAW:
			memcpy(out_spl, in_spl, duration);
			break;
		case TONES_TDATA_ALAWFLIPPED:
			g711_transcode_alaw_flipped_to_ulaw(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_ULAWFLIPPED:
			g711_transcode_flipped(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_EOL:
			return -EINVAL;
		}
		break;
	case TONES_TDATA_ALAWFLIPPED:
		switch (in_coding) {
		case TONES_TDATA_SLIN16HOST:
			g711_encode_alaw_flipped(in_spl, duration * 2, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_ALAW:
			g711_transcode_flipped(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_ULAW:
			g711_transcode_ulaw_to_alaw_flipped(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_ALAWFLIPPED:
			memcpy(out_spl, in_spl, duration);
			break;
		case TONES_TDATA_ULAWFLIPPED:
			g711_transcode_ulaw_flipped_to_alaw(in_spl, duration, &temp, &temp_size, NULL);
			g711_transcode_flipped(temp, temp_size, &temp2, &temp_size2, NULL);
			memcpy(out_spl, temp2, duration);
			break;
		case TONES_TDATA_EOL:
			return -EINVAL;
		}
		break;
	case TONES_TDATA_ULAWFLIPPED:
		switch (in_coding) {
		case TONES_TDATA_SLIN16HOST:
			g711_encode_ulaw_flipped(in_spl, duration * 2, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_ALAW:
			g711_transcode_alaw_to_ulaw_flipped(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_ULAW:
			g711_transcode_flipped(in_spl, duration, &temp, &temp_size, NULL);
			memcpy(out_spl, temp, duration);
			break;
		case TONES_TDATA_ALAWFLIPPED:
			g711_transcode_alaw_flipped_to_ulaw(in_spl, duration, &temp, &temp_size, NULL);
			g711_transcode_flipped(temp, temp_size, &temp2, &temp_size2, NULL);
			memcpy(out_spl, temp2, duration);
			break;
		case TONES_TDATA_ULAWFLIPPED:
			memcpy(out_spl, in_spl, duration);
			break;
		case TONES_TDATA_EOL:
			return -EINVAL;
		}
		break;
	case TONES_TDATA_EOL:
		return -EINVAL;
	}

	free(temp);
	free(temp2);
	free(new_in_spl);

	return 0;
}

static void render_tone(tones_data_t *data, uint8_t tone, tones_seq_t *seq, enum tones_tdata coding)
{
	int duration, i, s, torender;
	size_t in_spl_size, out_spl_size;
	int rc;

	out_spl_size = sizeof_coding(coding);

	/* Get total duration (without tone off duration) to allocate memory. */
	duration = 0;
	for (i = 0; seq[i].tdata; i++) {
		in_spl_size = sizeof_coding(seq[i].tdata);
		if (seq[i].duration == TONES_DURATION_AUTO)
			duration += seq[i].spl_data_size / in_spl_size;
		else
			duration += seq[i].duration;
	}
	data->spl_data[tone] = malloc(duration * data->spl_size);

	/* Render each tone chunk. */
	duration = 0;
	for (i = 0; seq[i].tdata; i++) {
		in_spl_size = sizeof_coding(seq[i].tdata);
		if (seq[i].duration == TONES_DURATION_AUTO) {
			torender = seq[i].spl_data_size / in_spl_size;
			rc = render_chunk(data->spl_data[tone] + duration * out_spl_size, coding, seq[i].spl_data, seq[i].tdata, torender, seq[i].db);
			if (rc)
				abort();
			duration += torender;
		} else for (s = 0; s < seq[i].duration; s += torender) {
			torender = seq[i].spl_data_size / in_spl_size;
			if (seq[i].duration - s < torender)
				torender = seq[i].duration - s;
			rc = render_chunk(data->spl_data[tone] + duration * out_spl_size, coding, seq[i].spl_data, seq[i].tdata, torender, seq[i].db);
			if (rc)
				abort();
			duration += torender;
		}
	}
	/* Store total duration of tone. */
	data->spl_duration[tone] = duration;
	/* Store silence interval until repeat. Use duration of TONES_TDATA_EOL. */
	duration += seq[i].duration;
	data->spl_repeat[tone] = duration;
}

int tones_init(tones_data_t *data, const char *toneset, enum tones_tdata coding)
{
	tones_set_t *set;
	int i;

	memset(data, 0, sizeof(*data));

	if (!toneset)
		return 0;

	for (i = 0; tones_sets[i].name; i++) {
		if (!strcasecmp(tones_sets[i].name, toneset))
			break;
	}
	if (!tones_sets[i].name)
		return -EINVAL;
	set = tones_sets[i].set;

	data->spl_size = sizeof_coding(coding);

	for (i = 0; set[i].tone; i++) {
		render_tone(data, set[i].tone, set[i].seq, coding);
		int peak = 0;
		for (int s = 0; s < data->spl_duration[set[i].tone]; s++) {
			if (((int16_t *)data->spl_data[set[i].tone])[s] > peak)
				peak = ((int16_t *)data->spl_data[set[i].tone])[s];
		}
	}
	render_tone(data, TONES_TONE_SILENCE, seq_silence, coding);

	return 0;
}

void tones_exit(tones_data_t *data)
{
	int i;

	for (i = 0; i < 256; i++)
		free(data->spl_data[i]);
	memset(data, 0, sizeof(*data));
}

/* This function is used to substitute missing tones/anouncements. */
static enum tones_tone substitute_tone(tones_data_t *data, enum tones_tone tone)
{
	/* If no tone is given, keep this decision. */
	if (!tone)
		return tone;

	/* If no 'recall', use 'ringback'. If none of them, turn off tone. */
	if (tone == TONES_TONE_RECALL && !data->spl_data[tone])
		tone = TONES_TONE_RINGBACK;
	if (tone == TONES_TONE_RINGBACK && !data->spl_data[tone])
		tone = TONES_TONE_RECALL;
	if (tone == TONES_TONE_RECALL && !data->spl_data[tone]) {
		tone = TONES_TONE_SILENCE;
		return tone;
	}

	/* Turn 'dialtonespecial' into 'dialtone'. If no dialtone exists, turn off tone. */
	if (tone == TONES_TONE_DIALTONE_SPECIAL && !data->spl_data[tone])
		tone = TONES_TONE_DIALTONE;
	if (tone == TONES_TONE_DIALTONE && !data->spl_data[tone]) {
		tone = TONES_TONE_SILENCE;
		return tone;
	}

	/* If no CW exists, turn off tone. */
	if (tone == TONES_TONE_CW && !data->spl_data[tone]) {
		tone = TONES_TONE_OFF;
		return tone;
	}

	/* If no interception exists, turn off tone. */
	if (tone == TONES_TONE_AUFSCHALTTON && !data->spl_data[tone]) {
		tone = TONES_TONE_OFF;
		return tone;
	}

	/* Turn 'congestion' into 'busy'. */
	if (tone == TONES_TONE_CONGESTION && !data->spl_data[tone])
		tone = TONES_TONE_BUSY;

	/* Turn 'noanswer' into 'busy'. */
	if (tone == TONES_TONE_NOANSWER && !data->spl_data[tone])
		tone = TONES_TONE_BUSY;

	/* Turn 'hangup' into 'busy' and vice versa. If none of them, use 'SIT'. */
	if (tone == TONES_TONE_HANGUP && !data->spl_data[tone])
		tone = TONES_TONE_BUSY;
	if (tone == TONES_TONE_BUSY && !data->spl_data[tone])
		tone = TONES_TONE_HANGUP;
	if (tone == TONES_TONE_HANGUP && !data->spl_data[tone])
		tone = TONES_TONE_SIT;

	/* Turn any other non-existing tone into 'SIT' */
	if (!data->spl_data[tone])
		tone = TONES_TONE_SIT;

	/* If no 'SIT', try to use congestion, busy or hangup. */
	if (tone == TONES_TONE_SIT && !data->spl_data[tone])
		tone = TONES_TONE_CONGESTION;
	if (tone == TONES_TONE_CONGESTION && !data->spl_data[tone])
		tone = TONES_TONE_BUSY;
	if (tone == TONES_TONE_BUSY && !data->spl_data[tone])
		tone = TONES_TONE_HANGUP;

	/* If no tone at all, turn off tone.*/
	if (!data->spl_data[tone])
		tone = TONES_TONE_SILENCE;

	return tone;
}

void tones_set_tone(tones_data_t *data, tones_t *t, enum tones_tone tone)
{
	t->data = data;
	t->tone = substitute_tone(data, tone);
	t->spl_pos = 0;
}

void tones_read_tone(tones_t *t, void *out_data, int out_duration)
{
	void *in_data;
	int in_duration, in_repeat, in_pos, tosend;
	size_t spl_size;

	/* No tone was set, so nothing to play. */
	if (!t->data)
		return;

	in_data = t->data->spl_data[t->tone];
	in_duration = t->data->spl_duration[t->tone];
	in_repeat = t->data->spl_repeat[t->tone];
	spl_size = t->data->spl_size;
	in_pos = t->spl_pos;

	/* No sample to play. */
	if (!in_data || !in_duration || !in_repeat || !spl_size)
		return;

	/* Copy sample to output stream. */
	while(out_duration) {
		/* Wrap around if repeat count is reached. */
		if (in_pos == in_repeat)
			in_pos = 0;
		/* Skip tone until repeat count is reached. */
		if (in_pos >= in_duration) {
			/* Do not overwrite output stream. */
			tosend = in_repeat - in_pos;
			if (out_duration < tosend)
				tosend = out_duration;
		} else {
			/* Overwrite output stream. */
			tosend = in_duration - in_pos;
			if (out_duration < tosend)
				tosend = out_duration;
			memcpy(out_data, in_data + in_pos * spl_size, tosend * spl_size);
		}
		out_data += tosend * spl_size;
		out_duration -= tosend;
		in_pos += tosend;
	}

	t->spl_pos = in_pos;
}

