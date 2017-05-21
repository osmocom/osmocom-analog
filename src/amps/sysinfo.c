#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "../common/sample.h"
#include "../common/timer.h"
#include "amps.h"
#include "frame.h"

static struct sysinfo_reg_incr default_reg_incr = {
	450,
};

static struct sysinfo_loc_area default_loc_area = {
	0,
	0,
	0,
	0,
};

static struct sysinfo_new_acc default_new_acc = {
	0,
};

static struct sysinfo_overload default_overload = {
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};

static struct sysinfo_acc_type default_acc_type = {
	1,
	0,
	0,
	0,
	0,
};

static struct sysinfo_acc_attempt default_acc_attempt = {
	10,
	10,
	10,
	10,
};

void init_sysinfo(amps_si *si, int cmac, int vmac, int dcc, int sid1, int regh, int regr, int pureg, int pdreg, int locaid, int regincr, int bis)
{
	int i;

	memset(si, 0, sizeof(*si));

	/* all words */
	si->dcc = dcc;
	/* VC assginment */
	si->vmac = vmac;

	/* filler */
	si->filler.cmac = cmac;
	si->filler.sdcc1 = 0;
	si->filler.sdcc2 = 0;
	si->filler.wfom = (bis) ? 0 : 1; /* must be set to ignore B/I bit */

	/* Word 1 */
	si->word1.sid1 = sid1;
	si->word1.ep = 0; /* shall be 0 */
	si->word1.auth = 0;
	si->word1.pci = 0;

	/* Word 2 */
	si->word2.s = 1;
	si->word2.e = 1;
	si->word2.regh = regh;
	si->word2.regr = regr;
	si->word2.dtx = 0; /* DTX seems not to work with Dynatac 8000 */
	si->word2.n_1 = 20;
	si->word2.rcf = (bis) ? 0 : 1; /* must be set to ignore B/I bit */
	si->word2.cpa = 1; /* must be set for combined CC+PC */
	si->word2.cmax_1 = 20;

	/* registration increment */
	si->reg_incr.regincr = regincr;

	/* location area */
	si->loc_area.pureg = pureg;
	si->loc_area.pdreg = pdreg;
	if (locaid >= 0) {
		si->loc_area.lreg = 1;
		si->loc_area.locaid = locaid;
	}

	/* new access channel set */
	si->new_acc.newacc = 0;

	/* overload control */
	for (i = 0; i < 16; i++)
		si->overload.olc[i] = 1;

	/* Acces Tyoe */
	/* 'bis' must be 0, so the phone does not wait for busy bit.
	 * We cannot respond with B/I fast enough due to processing delay.
	 * So we don't set the B/I bit to busy on reception of message.
	 * The access type message (including this 'bis') must also be included.
	 */
	si->acc_type.bis = bis; /* must be clear to ignore B/I bit */
	si->acc_type.pci_home = 0; /* if set, bscap must allso be set */
	si->acc_type.pci_roam = 0; /* if set, bscap must allso be set */
	si->acc_type.bspc = 0;
	si->acc_type.bscap = 0;

	/* access attempt parameters */
	si->acc_attempt.maxbusy_pgr = 10;
	si->acc_attempt.maxsztr_pgr = 10;
	si->acc_attempt.maxbusy_other = 10;
	si->acc_attempt.maxsztr_other = 10;

	/* registration ID */
	si->reg_id.regid = 1000;
}

void prepare_sysinfo(amps_si *si)
{
	int i = 0;

	si->type[i++] = SYSINFO_WORD1;
	si->type[i++] = SYSINFO_WORD2;
	si->type[i++] = SYSINFO_REG_ID;
	/* include only messages that differ from default */
	if (!!memcmp(&si->reg_incr, &default_reg_incr, sizeof(si->reg_incr)))
		si->type[i++] = SYSINFO_REG_INCR;
	if (!!memcmp(&si->loc_area, &default_loc_area, sizeof(si->loc_area)))
		si->type[i++] = SYSINFO_LOC_AREA;
	if (!!memcmp(&si->new_acc, &default_new_acc, sizeof(si->new_acc)))
		si->type[i++] = SYSINFO_NEW_ACC;
	if (!!memcmp(&si->overload, &default_overload, sizeof(si->overload)))
		si->type[i++] = SYSINFO_OVERLOAD;
	if (!!memcmp(&si->acc_type, &default_acc_type, sizeof(si->acc_type)))
		si->type[i++] = SYSINFO_ACC_TYPE;
	if (!!memcmp(&si->acc_attempt, &default_acc_attempt, sizeof(si->acc_attempt)))
		si->type[i++] = SYSINFO_ACC_ATTEMPT;
	si->num = i; /* train is running */
	si->count = 0; /* first message in train */
	if (i > (int)(sizeof(si->type) / sizeof(si->type[0]))) {
		fprintf(stderr, "si type array overflow, pleas fix!\n");
		abort();
	}
}

uint64_t get_sysinfo(amps_si *si)
{
	int count, nawc, end = 0;
	time_t ti = time(NULL);

	count = si->count;

	if (++si->count == si->num) {
		end = 1;
		si->num = 0; /* train is over */
	}

	switch (si->type[count]) {
	case SYSINFO_WORD1:
		nawc = si->num - 1;
		return amps_encode_word1_system(si->dcc, si->word1.sid1, si->word1.ep, si->word1.auth, si->word1.pci, nawc);
	case SYSINFO_WORD2:
		return amps_encode_word2_system(si->dcc, si->word2.s, si->word2.e, si->word2.regh, si->word2.regr, si->word2.dtx, si->word2.n_1, si->word2.rcf, si->word2.cpa, si->word2.cmax_1, end);
	case SYSINFO_REG_ID:
		/* use time stamp to generate regid */
		si->reg_id.regid = ti & 0xfffff;
		return amps_encode_registration_id(si->dcc, si->reg_id.regid, end);
	case SYSINFO_REG_INCR:
		return amps_encode_registration_increment(si->dcc, si->reg_incr.regincr, end);
	case SYSINFO_LOC_AREA:
		return amps_encode_location_area(si->dcc, si->loc_area.pureg, si->loc_area.pdreg, si->loc_area.lreg, si->loc_area.locaid, end);
	case SYSINFO_NEW_ACC:
		return amps_encode_new_access_channel_set(si->dcc, si->new_acc.newacc, end);
	case SYSINFO_OVERLOAD:
		return amps_encode_overload_control(si->dcc, si->overload.olc, end);
	case SYSINFO_ACC_TYPE:
		return amps_encode_access_type(si->dcc, si->acc_type.bis, si->acc_type.pci_home, si->acc_type.pci_roam, si->acc_type.bspc, si->acc_type.bscap, end);
	case SYSINFO_ACC_ATTEMPT:
		return amps_encode_access_attempt(si->dcc, si->acc_attempt.maxbusy_pgr, si->acc_attempt.maxsztr_pgr, si->acc_attempt.maxbusy_other, si->acc_attempt.maxsztr_other, end);
	}

	fprintf(stderr, "get_sysinfo unknown type, please fix!\n");
	abort();
}

