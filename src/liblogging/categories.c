
#include <osmocom/core/utils.h>
#include <osmocom/core/logging.h>
#include "categories.h"

/* All logging categories used by this project. */

struct log_info_cat log_categories[] = {
	[DLCC] = {
		.name = "DLCC",
		.description = "libosmo-cc CC Layer",
		.color = "\033[0;37m",
	},
	[DOPTIONS] = {
		.name = "DOPTIONS",
		.description = "config options",
		.color = "\033[0;33m",
	},
	[DSENDER] = {
		.name = "DSENDER",
		.description = "transceiver instance",
		.color = "\033[1;33m",
	},
	[DSOUND] = {
		.name = "DSOUND",
		.description = "sound io",
		.color = "\033[0;35m",
	},
	[DDSP] = {
		.name = "DDSP",
		.description = "digital signal processing",
		.color = "\033[0;31m",
	},
	[DANETZ] = {
		.name = "DANETZ",
		.description = "A-Netz",
		.color = "\033[1;34m",
	},
	[DBNETZ] = {
		.name = "DBNETZ",
		.description = "B-Netz",
		.color = "\033[1;34m",
	},
	[DCNETZ] = {
		.name = "DCNETZ",
		.description = "C-Netz",
		.color = "\033[1;34m",
	},
	[DNMT] = {
		.name = "DNMT",
		.description = "Norisk Mobil Telefoni",
		.color = "\033[1;34m",
	},
	[DAMPS] = {
		.name = "DAMPS",
		.description = "Advanced Mobile Phone Service",
		.color = "\033[1;34m",
	},
	[DR2000] = {
		.name = "DR2000",
		.description = "Radiocom 2000",
		.color = "\033[1;34m",
	},
	[DIMTS] = {
		.name = "DIMTS",
		.description = "Improved Mobile Telephone Service",
		.color = "\033[1;34m",
	},
	[DMPT1327] = {
		.name = "DMPT1327",
		.description = "MPT-1327",
		.color = "\033[1;34m",
	},
	[DJOLLY] = {
		.name = "DJOLLY",
		.description = "Jolly-Com",
		.color = "\033[1;34m",
	},
	[DEURO] = {
		.name = "DEUROSIGNAL",
		.description = "Eurosignal",
		.color = "\033[1;34m",
	},
	[DPOCSAG] = {
		.name = "DPOCSAG",
		.description = "POCSAG",
		.color = "\033[1;34m",
	},
	[DGOLAY] = {
		.name = "DGOLAY",
		.description = "Golay",
		.color = "\033[1;34m",
	},
	[DFUENF] = {
		.name = "DFUENF",
		.description = "5-Ton-Folge",
		.color = "\033[1;34m",
	},
	[DFRAME] = {
		.name = "DFRAME",
		.description = "message frame",
		.color = "\033[0;36m",
	},
	[DCALL] = {
		.name = "DCALL",
		.description = "call processing",
		.color = "\033[0;37m",
	},
	[DDB] = {
		.name = "DDB",
		.description = "database access",
		.color = "\033[0;33m",
	},
	[DTRANS] = {
		.name = "DTRANS",
		.description = "transaction handing",
		.color = "\033[0;32m",
	},
	[DDMS] = {
		.name = "DDMS",
		.description = "DMS layer of NMT",
		.color = "\033[0;33m",
	},
	[DSMS] = {
		.name = "DSMS",
		.description = "SMS layer of NMT",
		.color = "\033[1;37m",
	},
	[DSDR] = {
		.name = "DSDR",
		.description = "Software Defined Radio",
		.color = "\033[1;31m",
	},
	[DUHD] = {
		.name = "DUHD",
		.description = "UHD interface",
		.color = "\033[1;35m",
	},
	[DSOAPY] = {
		.name = "DSOAPY",
		.description = "Soapy interface",
		.color = "\033[1;35m",
	},
	[DWAVE] = {
		.name = "DWAVE",
		.description = "WAVE file handling",
		.color = "\033[1;33m",
	},
	[DRADIO] = {
		.name = "DRADIO",
		.description = "Radio application",
		.color = "\033[1;34m",
	},
	[DAM791X] = {
		.name = "DAM791X",
		.description = "AM791x modem chip emulation",
		.color = "\033[0;31m",
	},
	[DUART] = {
		.name = "DUART",
		.description = "UART emulation",
		.color = "\033[0;32m",
	},
	[DDEVICE] = {
		.name = "DDEVICE",
		.description = "CUSE device emulation",
		.color = "\033[0;33m",
	},
	[DDATENKLO] = {
		.name = "DDATENKLO",
		.description = "Das Datenklo",
		.color = "\033[1;34m",
	},
	[DZEIT] = {
		.name = "DZEIT",
		.description = "Zeitansage",
		.color = "\033[1;34m",
	},
	[DSIM1] = {
		.name = "DSIM1",
		.description = "C-Netz SIM layer 1",
		.color = "\033[0;31m",
	},
	[DSIM2] = {
		.name = "DSIM2",
		.description = "C-Netz SIM layer 2",
		.color = "\033[0;33m",
	},
	[DSIMI] = {
		.name = "DSIMI",
		.description = "C-Netz SIM ICL layer",
		.color = "\033[0;36m",
	},
	[DSIM7] = {
		.name = "DSIM7",
		.description = "C-Netz SIM layer 7",
		.color = "\033[0;37m",
	},
	[DMTP2] = {
		.name = "DMTP LAYER 2",
		.description = "MTP layer 2",
		.color = "\033[1;33m",
	},
	[DMTP3] = {
		.name = "DMTP LAYER 3",
		.description = "MTP layer 3",
		.color = "\033[1;36m",
	},
	[DMUP] = {
		.name = "DMUP",
		.description = "C-Netz Mobile User Part",
		.color = "\033[1;37m",
	},
	[DDCF77] = {
		.name = "DDCF77",
		.description = "DCF77 Radio Clock",
		.color = "\033[1;34m",
	},
	[DJITTER] = {
		.name = "DJITTER",
		.description = "jitter buffer handling",
		.color = "\033[0;36m",
	},
};

size_t log_categories_size = ARRAY_SIZE(log_categories);

