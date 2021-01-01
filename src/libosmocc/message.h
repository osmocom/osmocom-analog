#ifndef OSMO_CC_MSG_H
#define OSMO_CC_MSG_H

#define OSMO_CC_VERSION "OSMOCCv1"

/* call control messages types */
enum osmo_cc_msg_type {
	OSMO_CC_MSG_SETUP_REQ =		0x00,
	OSMO_CC_MSG_SETUP_IND =		0x01,
	OSMO_CC_MSG_REJ_REQ =		0x10,
	OSMO_CC_MSG_REJ_IND =		0x11,
	OSMO_CC_MSG_SETUP_ACK_REQ =	0x20,
	OSMO_CC_MSG_SETUP_ACK_IND =	0x21,
	OSMO_CC_MSG_PROC_REQ =		0x30,
	OSMO_CC_MSG_PROC_IND =		0x31,
	OSMO_CC_MSG_ALERT_REQ =		0x40,
	OSMO_CC_MSG_ALERT_IND =		0x41,
	OSMO_CC_MSG_SETUP_RSP =		0x02,
	OSMO_CC_MSG_SETUP_CNF =		0x03,
	OSMO_CC_MSG_SETUP_COMP_REQ =	0x50,
	OSMO_CC_MSG_SETUP_COMP_IND =	0x51,
	OSMO_CC_MSG_DISC_REQ =		0x60,
	OSMO_CC_MSG_DISC_IND =		0x61,
	OSMO_CC_MSG_REL_REQ =		0x70,
	OSMO_CC_MSG_REL_CNF =		0x73,
	OSMO_CC_MSG_REL_IND =		0x71,
	OSMO_CC_MSG_PROGRESS_REQ =	0x80,
	OSMO_CC_MSG_PROGRESS_IND =	0x81,
	OSMO_CC_MSG_NOTIFY_REQ =	0x84,
	OSMO_CC_MSG_NOTIFY_IND =	0x85,
	OSMO_CC_MSG_INFO_REQ =		0x88,
	OSMO_CC_MSG_INFO_IND =		0x89,
	OSMO_CC_MSG_ATTACH_REQ =	0xf8,
	OSMO_CC_MSG_ATTACH_IND =	0xf9,
	OSMO_CC_MSG_ATTACH_RSP =	0xfa,
	OSMO_CC_MSG_ATTACH_CNF =	0xfb,
	OSMO_CC_MSG_DUMMY_REQ =		0xfc,
};

#define OSMO_CC_MSG_MASK		0x03,
#define OSMO_CC_MSG_REQ			0x00,
#define OSMO_CC_MSG_IND			0x01,
#define OSMO_CC_MSG_RSP			0x02,
#define OSMO_CC_MSG_CNF			0x03,

/* information elements */
enum osmo_cc_ie_type {
	OSMO_CC_IE_CALLED =		0x11,
	OSMO_CC_IE_CALLED_SUB =		0x12,
	OSMO_CC_IE_CALLED_NAME =	0x13,
	OSMO_CC_IE_CALLED_INTERFACE =	0x14,
	OSMO_CC_IE_DTMF =		0x1d,
	OSMO_CC_IE_KEYPAD =		0x1e,
	OSMO_CC_IE_COMPLETE =		0x1f,
	OSMO_CC_IE_CALLING =		0x21,
	OSMO_CC_IE_CALLING_SUB =	0x22,
	OSMO_CC_IE_CALLING_NAME =	0x23,
	OSMO_CC_IE_CALLING_INTERFACE =	0x24,
	OSMO_CC_IE_CALLING_NETWORK =	0x2f,
	OSMO_CC_IE_REDIR =		0x31,
	OSMO_CC_IE_PROGRESS =		0x32,
	OSMO_CC_IE_NOTIFY =		0x33,
	OSMO_CC_IE_DISPLAY =		0x34,
	OSMO_CC_IE_CAUSE =		0x41,
	OSMO_CC_IE_BEARER =		0x51,
	OSMO_CC_IE_SDP =		0x52,
	OSMO_CC_IE_SOCKET_ADDRESS =	0x5e,
	OSMO_CC_IE_PRIVATE =		0x5f,
};

/* type of number, see ITU-T Rec. Q.931 */
#define OSMO_CC_TYPE_UNKNOWN			0
#define OSMO_CC_TYPE_INTERNATIONAL		1
#define OSMO_CC_TYPE_NATIONAL			2
#define OSMO_CC_TYPE_NETWORK			3
#define OSMO_CC_TYPE_SUBSCRIBER			4
#define OSMO_CC_TYPE_ABBREVIATED		5
#define OSMO_CC_TYPE_RESERVED			7

/* numbering plan, see ITU-T Rec. Q.931 */
#define OSMO_CC_PLAN_UNKNOWN			0
#define OSMO_CC_PLAN_TELEPHONY			1
#define OSMO_CC_PLAN_DATA			3
#define OSMO_CC_PLAN_TTY			4
#define OSMO_CC_PLAN_NATIONAL_STANDARD		8
#define OSMO_CC_PLAN_PRIVATE			9
#define OSMO_CC_PLAN_RESERVED			15

/* presentation indicator, see ITU-T Rec. Q.931 */
#define OSMO_CC_PRESENT_ALLOWED			0
#define OSMO_CC_PRESENT_RESTRICTED		1
#define OSMO_CC_PRESENT_NOT_AVAIL		2
#define OSMO_CC_PRESENT_RESERVED		3

/* screening indicator, see ITU-T Rec. Q.931 */
#define OSMO_CC_SCREEN_USER_UNSCREENED		0
#define OSMO_CC_SCREEN_USER_VERIFIED_PASSED	1
#define OSMO_CC_SCREEN_USER_VERIFIED_FAILED	2
#define OSMO_CC_SCREEN_NETWORK			3

/* screening indicator, see ITU-T Rec. Q.931 */
#define OSMO_CC_REDIR_REASON_UNKNOWN		0
#define OSMO_CC_REDIR_REASON_CFB		1
#define OSMO_CC_REDIR_REASON_CFNR		2
#define OSMO_CC_REDIR_REASON_CD			4
#define OSMO_CC_REDIR_REASON_CF_OUTOFORDER	9
#define OSMO_CC_REDIR_REASON_CF_BY_DTE		10
#define OSMO_CC_REDIR_REASON_CFU		15

/* notification indicator, see ITU-T Rec. Q.931 ff. */
#define OSMO_CC_NOTIFY_USER_SUSPENDED		0x00    
#define OSMO_CC_NOTIFY_USER_RESUMED		0x01
#define OSMO_CC_NOTIFY_BEARER_SERVICE_CHANGE	0x02
#define OSMO_CC_NOTIFY_CALL_COMPLETION_DELAY	0x03
#define OSMO_CC_NOTIFY_CONFERENCE_ESTABLISHED	0x42
#define OSMO_CC_NOTIFY_CONFERENCE_DISCONNECTED	0x43
#define OSMO_CC_NOTIFY_OTHER_PARTY_ADDED	0x44
#define OSMO_CC_NOTIFY_ISOLATED			0x45
#define OSMO_CC_NOTIFY_REATTACHED		0x46
#define OSMO_CC_NOTIFY_OTHER_PARTY_ISOLATED	0x47
#define OSMO_CC_NOTIFY_OTHER_PARTY_REATTACHED	0x48
#define OSMO_CC_NOTIFY_OTHER_PARTY_SPLIT	0x49
#define OSMO_CC_NOTIFY_OTHER_PARTY_DISCONNECTED	0x4a
#define OSMO_CC_NOTIFY_CONFERENCE_FLOATING	0x4b
#define OSMO_CC_NOTIFY_CONFERENCE_DISC_PREEMPT	0x4c	/* disconnect preemted */
#define OSMO_CC_NOTIFY_CONFERENCE_FLOATING_SUP	0x4f	/* served user preemted */
#define OSMO_CC_NOTIFY_CALL_IS_A_WAITING_CALL	0x60
#define OSMO_CC_NOTIFY_DIVERSION_ACTIVATED	0x68
#define OSMO_CC_NOTIFY_RESERVED_CT_1		0x69
#define OSMO_CC_NOTIFY_RESERVED_CT_2		0x6a
#define OSMO_CC_NOTIFY_REVERSE_CHARGING		0x6e
#define OSMO_CC_NOTIFY_REMOTE_HOLD		0x79
#define OSMO_CC_NOTIFY_REMOTE_RETRIEVAL		0x7a
#define OSMO_CC_NOTIFY_CALL_IS_DIVERTING	0x7b

/* coding standard, see ITU-T Rec. Q.931 */
#define OSMO_CC_CODING_ITU_T			0
#define OSMO_CC_CODING_ISO_IEC			1
#define OSMO_CC_CODING_NATIONAL			2
#define OSMO_CC_CODING_STANDARD_SPECIFIC	3

/* cause, see ITU-T Rec. Q.850 */
#define	OSMO_CC_ISDN_CAUSE_UNASSIGNED_NR	1
#define	OSMO_CC_ISDN_CAUSE_NO_ROUTE		3
#define	OSMO_CC_ISDN_CAUSE_CHAN_UNACCEPT	6
#define	OSMO_CC_ISDN_CAUSE_OP_DET_BARRING	8
#define	OSMO_CC_ISDN_CAUSE_NORM_CALL_CLEAR	16
#define	OSMO_CC_ISDN_CAUSE_USER_BUSY		17
#define	OSMO_CC_ISDN_CAUSE_USER_NOTRESPOND	18
#define	OSMO_CC_ISDN_CAUSE_USER_ALERTING_NA	19
#define	OSMO_CC_ISDN_CAUSE_CALL_REJECTED	21
#define	OSMO_CC_ISDN_CAUSE_NUMBER_CHANGED	22
#define	OSMO_CC_ISDN_CAUSE_PRE_EMPTION		25
#define	OSMO_CC_ISDN_CAUSE_NONSE_USER_CLR	26
#define	OSMO_CC_ISDN_CAUSE_DEST_OOO		27
#define	OSMO_CC_ISDN_CAUSE_INV_NR_FORMAT	28
#define	OSMO_CC_ISDN_CAUSE_FACILITY_REJ		29
#define	OSMO_CC_ISDN_CAUSE_RESP_STATUS_INQ	30
#define	OSMO_CC_ISDN_CAUSE_NORMAL_UNSPEC	31
#define	OSMO_CC_ISDN_CAUSE_NO_CIRCUIT_CHAN	34
#define	OSMO_CC_ISDN_CAUSE_NETWORK_OOO		38
#define	OSMO_CC_ISDN_CAUSE_TEMP_FAILURE		41
#define	OSMO_CC_ISDN_CAUSE_SWITCH_CONG		42
#define	OSMO_CC_ISDN_CAUSE_ACC_INF_DISCARD	43
#define	OSMO_CC_ISDN_CAUSE_REQ_CHAN_UNAVAIL	44
#define	OSMO_CC_ISDN_CAUSE_RESOURCE_UNAVAIL	47
#define	OSMO_CC_ISDN_CAUSE_QOS_UNAVAIL		49
#define	OSMO_CC_ISDN_CAUSE_REQ_FAC_NOT_SUBSC	50
#define	OSMO_CC_ISDN_CAUSE_INC_BARRED_CUG	55
#define	OSMO_CC_ISDN_CAUSE_BEARER_CAP_UNAUTH	57
#define	OSMO_CC_ISDN_CAUSE_BEARER_CA_UNAVAIL	58
#define	OSMO_CC_ISDN_CAUSE_SERV_OPT_UNAVAIL	63
#define	OSMO_CC_ISDN_CAUSE_BEARERSERV_UNIMPL	65
#define	OSMO_CC_ISDN_CAUSE_ACM_GE_ACM_MAX	68
#define	OSMO_CC_ISDN_CAUSE_REQ_FAC_NOTIMPL	69
#define	OSMO_CC_ISDN_CAUSE_RESTR_BCAP_AVAIL	70
#define	OSMO_CC_ISDN_CAUSE_SERV_OPT_UNIMPL	79
#define	OSMO_CC_ISDN_CAUSE_INVAL_CALLREF	81
#define	OSMO_CC_ISDN_CAUSE_USER_NOT_IN_CUG	87
#define	OSMO_CC_ISDN_CAUSE_INCOMPAT_DEST	88
#define	OSMO_CC_ISDN_CAUSE_INVAL_TRANS_NET	91
#define	OSMO_CC_ISDN_CAUSE_SEMANTIC_INCORR	95
#define	OSMO_CC_ISDN_CAUSE_INVAL_MAND_INF	96
#define	OSMO_CC_ISDN_CAUSE_MSGTYPE_NOTEXIST	97
#define	OSMO_CC_ISDN_CAUSE_MSGTYPE_INCOMPAT	98
#define	OSMO_CC_ISDN_CAUSE_IE_NOTEXIST		99
#define	OSMO_CC_ISDN_CAUSE_COND_IE_ERR		100
#define	OSMO_CC_ISDN_CAUSE_MSG_INCOMP_STATE	101
#define	OSMO_CC_ISDN_CAUSE_RECOVERY_TIMER	102
#define	OSMO_CC_ISDN_CAUSE_PROTO_ERR		111
#define	OSMO_CC_ISDN_CAUSE_INTERWORKING		127

/* location, see ITU-T Rec. Q.931 */
#define OSMO_CC_LOCATION_USER			0
#define OSMO_CC_LOCATION_PRIV_SERV_LOC_USER	1
#define OSMO_CC_LOCATION_PUB_SERV_LOC_USER	2
#define OSMO_CC_LOCATION_TRANSIT		3
#define OSMO_CC_LOCATION_PUB_SERV_REM_USER	4
#define OSMO_CC_LOCATION_PRIV_SERV_REM_USER	5
#define OSMO_CC_LOCATION_BEYOND_INTERWORKING	10

/* progress description, see ITU-T Rec. Q.931 */
#define OSMO_CC_PROGRESS_NOT_END_TO_END_ISDN	1
#define OSMO_CC_PROGRESS_DEST_NOT_ISDN		2
#define OSMO_CC_PROGRESS_ORIG_NOT_ISDN		3
#define OSMO_CC_PROGRESS_RETURN_TO_ISDN		4
#define OSMO_CC_PROGRESS_INTERWORKING		5
#define OSMO_CC_PROGRESS_INBAND_INFO_AVAILABLE	8

/* information transfer capability, see ITU-T Rec. Q.931 */
#define OSMO_CC_CAPABILITY_SPEECH		0
#define OSMO_CC_CAPABILITY_DATA			8
#define OSMO_CC_CAPABILITY_DATA_RESTRICTED	9
#define OSMO_CC_CAPABILITY_AUDIO		16
#define OSMO_CC_CAPABILITY_DATA_WITH_TONES	17
#define OSMO_CC_CAPABILITY_VIDEO		24

/* transfer mode, see ITU-T Rec. Q.931 */
#define OSMO_CC_MODE_CIRCUIT			0
#define OSMO_CC_MODE_PACKET			2

#define OSMO_CC_DTMF_MODE_OFF			0	/* stop tone */
#define OSMO_CC_DTMF_MODE_ON			1	/* start tone */
#define OSMO_CC_DTMF_MODE_DIGITS		2	/* play tone(s) with duration and pauses */

#define OSMO_CC_SOCKET_CAUSE_VERSION_MISMATCH	1	/* version mismatch */
#define OSMO_CC_SOCKET_CAUSE_FAILED		2	/* connection failed */
#define OSMO_CC_SOCKET_CAUSE_BROKEN_PIPE	3	/* connected socket failed */
#define OSMO_CC_SOCKET_CAUSE_TIMEOUT		4	/* keepalive packets timeout */
// if you add causes here, add them in process_cause.c also!

/* network type (network IE) and meaning of 'id' */
#define OSMO_CC_NETWORK_UNDEFINED		0x00
#define OSMO_CC_NETWORK_ALSA_NONE		0x01
#define OSMO_CC_NETWORK_POTS_NONE		0x02
#define OSMO_CC_NETWORK_ISDN_NONE		0x03
#define OSMO_CC_NETWORK_SIP_NONE		0x04
#define OSMO_CC_NETWORK_GSM_IMSI		0x05	/* id has decimal IMSI */
#define OSMO_CC_NETWORK_GSM_IMEI		0x06	/* id has decimal IMEI */
#define OSMO_CC_NETWORK_WEB_NONE		0x07
#define OSMO_CC_NETWORK_DECT_NONE		0x08
#define OSMO_CC_NETWORK_BLUETOOTH_NONE		0x09
#define OSMO_CC_NETWORK_SS5_NONE		0x0a
#define OSMO_CC_NETWORK_ANETZ_NONE		0x80
#define OSMO_CC_NETWORK_BNETZ_MUENZ		0x81	/* id starts with 'M' */
#define OSMO_CC_NETWORK_CNETZ_NONE		0x82
#define OSMO_CC_NETWORK_NMT_NONE		0x83	/* id has decimal password */
#define OSMO_CC_NETWORK_R2000_NONE		0x84
#define OSMO_CC_NETWORK_AMPS_ESN		0x85	/* if has decimal ESN (TACS also) */
#define OSMO_CC_NETWORK_MTS_NONE		0x86
#define OSMO_CC_NETWORK_IMTS_NONE		0x87
#define OSMO_CC_NETWORK_EUROSIGNAL_NONE		0x88
#define OSMO_CC_NETWORK_JOLLYCOM_NONE		0x89	/* call from JollyCom... */

typedef struct osmo_cc_msg {
	uint8_t type;
	uint16_t length_networkorder;
	uint8_t data[0];
} __attribute__((packed)) osmo_cc_msg_t;

typedef struct osmo_cc_msg_list {
	struct osmo_cc_msg_list *next;
	struct osmo_cc_msg *msg;
	uint32_t callref;
	char host[128];
	uint16_t port;
} osmo_cc_msg_list_t;

typedef struct osmo_cc_ie {
	uint8_t type;
	uint16_t length_networkorder;
	uint8_t data[0];
} __attribute__((packed)) osmo_cc_ie_t;

struct osmo_cc_ie_called {
	uint8_t type;
	uint8_t	plan;
	char digits[0];
} __attribute__((packed));

struct osmo_cc_ie_called_sub {
	uint8_t type;
	char digits[0];
} __attribute__((packed));

struct osmo_cc_ie_called_name {
	char name[0];
} __attribute__((packed));

struct osmo_cc_ie_called_interface {
	char name[0];
} __attribute__((packed));

struct osmo_cc_ie_calling {
	uint8_t type;
	uint8_t	plan;
	uint8_t present;
	uint8_t	screen;
	char digits[0];
} __attribute__((packed));

struct osmo_cc_ie_calling_sub {
	uint8_t type;
	char digits[0];
} __attribute__((packed));

struct osmo_cc_ie_calling_name {
	char name[0];
} __attribute__((packed));

struct osmo_cc_ie_calling_interface {
	char name[0];
} __attribute__((packed));

struct osmo_cc_ie_network {
	uint8_t type;
	char id[0];
} __attribute__((packed));

struct osmo_cc_ie_bearer {
	uint8_t coding;
	uint8_t capability;
	uint8_t mode;
} __attribute__((packed));

struct osmo_cc_ie_redir {
	uint8_t type;
	uint8_t	plan;
	uint8_t present;
	uint8_t	screen;
	uint8_t redir_reason;
	char digits[0];
} __attribute__((packed));

struct osmo_cc_ie_dtmf {
	uint8_t duration_ms;
	uint8_t pause_ms;
	uint8_t dtmf_mode;
	char digits[0];
} __attribute__((packed));

struct osmo_cc_ie_keypad {
	char digits[0];
} __attribute__((packed));

struct osmo_cc_ie_progress {
	uint8_t coding;
	uint8_t location;
	uint8_t progress;
} __attribute__((packed));

struct osmo_cc_ie_notify {
	uint8_t notify;
} __attribute__((packed));

struct osmo_cc_ie_cause {
	uint8_t location;
	uint8_t isdn_cause;
	uint16_t sip_cause_networkorder;
	uint8_t socket_cause;
} __attribute__((packed));

struct osmo_cc_ie_display {
	char text[0];
} __attribute__((packed));

struct osmo_cc_ie_sdp {
	char sdp[0];
} __attribute__((packed));

struct osmo_cc_ie_socket_address {
	char address[0];
} __attribute__((packed));

struct osmo_cc_ie_private {
	uint32_t unique_networkorder;
	uint8_t data[0];
} __attribute__((packed));

uint32_t osmo_cc_new_callref(void);
const char *osmo_cc_msg_name(uint8_t msg_type);
osmo_cc_msg_t *osmo_cc_new_msg(uint8_t msg_type);
osmo_cc_msg_t *osmo_cc_clone_msg(osmo_cc_msg_t *msg);
osmo_cc_msg_t *osmo_cc_msg_list_dequeue(osmo_cc_msg_list_t **mlp, uint32_t *callref_p);
osmo_cc_msg_list_t *osmo_cc_msg_list_enqueue(osmo_cc_msg_list_t **mlp, osmo_cc_msg_t *msg, uint32_t callref);
void osmo_cc_free_msg(osmo_cc_msg_t *msg);
int osmo_cc_get_ie_struct(osmo_cc_msg_t *msg, uint8_t ie_type, int ie_repeat, int ie_len, const osmo_cc_ie_t **ie_struct);
int osmo_cc_get_ie_data(osmo_cc_msg_t *msg, uint8_t ie_type, int ie_repeat, int ie_len, const void **ie_data);
int osmo_cc_has_ie(osmo_cc_msg_t *msg, uint8_t ie_type, int ie_repeat);
int osmo_cc_remove_ie(osmo_cc_msg_t *msg, uint8_t ie_type, int ie_repeat);
void *osmo_cc_add_ie(osmo_cc_msg_t *msg, uint8_t ie_type, int ie_len);
void *osmo_cc_msg_sep_ie(osmo_cc_msg_t *msg, void **iep, uint8_t *ie_type, uint16_t *ie_length);

void osmo_cc_add_ie_called(osmo_cc_msg_t *msg, uint8_t type, uint8_t plan, const char *dialing);
int osmo_cc_get_ie_called(osmo_cc_msg_t *msg, int ie_repeat, uint8_t *type, uint8_t *plan, char *dialing, size_t dialing_size);
void osmo_cc_add_ie_called_sub(osmo_cc_msg_t *msg, uint8_t type, const char *dialing);
int osmo_cc_get_ie_called_sub(osmo_cc_msg_t *msg, int ie_repeat, uint8_t *type, char *dialing, size_t dialing_size);
void osmo_cc_add_ie_called_name(osmo_cc_msg_t *msg, const char *name);
int osmo_cc_get_ie_called_name(osmo_cc_msg_t *msg, int ie_repeat, char *name, size_t name_size);
void osmo_cc_add_ie_called_interface(osmo_cc_msg_t *msg, const char *interface);
int osmo_cc_get_ie_called_interface(osmo_cc_msg_t *msg, int ie_repeat, char *interface, size_t interface_size);
void osmo_cc_add_ie_complete(osmo_cc_msg_t *msg);
int osmo_cc_get_ie_complete(osmo_cc_msg_t *msg, int ie_repeat);
void osmo_cc_add_ie_calling(osmo_cc_msg_t *msg, uint8_t type, uint8_t plan, uint8_t present, uint8_t screen, const char *callerid);
int osmo_cc_get_ie_calling(osmo_cc_msg_t *msg, int ie_repeat, uint8_t *type, uint8_t *plan, uint8_t *present, uint8_t *screen, char *callerid, size_t callerid_size);
void osmo_cc_add_ie_calling_sub(osmo_cc_msg_t *msg, uint8_t type, const char *callerid);
int osmo_cc_get_ie_calling_sub(osmo_cc_msg_t *msg, int ie_repeat, uint8_t *type, char *callerid, size_t callerid_size);
void osmo_cc_add_ie_calling_name(osmo_cc_msg_t *msg, const char *name);
int osmo_cc_get_ie_calling_name(osmo_cc_msg_t *msg, int ie_repeat, char *name, size_t name_size);
void osmo_cc_add_ie_calling_interface(osmo_cc_msg_t *msg, const char *interface);
int osmo_cc_get_ie_calling_interface(osmo_cc_msg_t *msg, int ie_repeat, char *interface, size_t interface_size);
void osmo_cc_add_ie_calling_network(osmo_cc_msg_t *msg, uint8_t type, const char *networkid);
int osmo_cc_get_ie_calling_network(osmo_cc_msg_t *msg, int ie_repeat, uint8_t *type, char *networkid, size_t networkid_size);
void osmo_cc_add_ie_bearer(osmo_cc_msg_t *msg, uint8_t coding, uint8_t capability, uint8_t mode);
int osmo_cc_get_ie_bearer(osmo_cc_msg_t *msg, int ie_repeat, uint8_t *coding, uint8_t *capability, uint8_t *mode);
void osmo_cc_add_ie_redir(osmo_cc_msg_t *msg, uint8_t type, uint8_t plan, uint8_t present, uint8_t screen, uint8_t redir_reason, const char *callerid);
int osmo_cc_get_ie_redir(osmo_cc_msg_t *msg, int ie_repeat, uint8_t *type, uint8_t *plan, uint8_t *present, uint8_t *screen, uint8_t *reason, char *callerid, size_t callerid_size);
void osmo_cc_add_ie_dtmf(osmo_cc_msg_t *msg, uint8_t duration_ms, uint8_t pause_ms, uint8_t dtmf_mode, const char *digits);
int osmo_cc_get_ie_dtmf(osmo_cc_msg_t *msg, int ie_repeat, uint8_t *duration_ms, uint8_t *pause_ms, uint8_t *dtmf_mode, char *digits, size_t digits_size);
void osmo_cc_add_ie_keypad(osmo_cc_msg_t *msg, const char *digits);
int osmo_cc_get_ie_keypad(osmo_cc_msg_t *msg, int ie_repeat, char *digits, size_t digits_size);
void osmo_cc_add_ie_progress(osmo_cc_msg_t *msg, uint8_t coding, uint8_t location, uint8_t progress);
int osmo_cc_get_ie_progress(osmo_cc_msg_t *msg, int ie_repeat, uint8_t *coding, uint8_t *location, uint8_t *progress);
void osmo_cc_add_ie_notify(osmo_cc_msg_t *msg, uint8_t notify);
int osmo_cc_get_ie_notify(osmo_cc_msg_t *msg, int ie_repeat, uint8_t *notify);
void osmo_cc_add_ie_cause(osmo_cc_msg_t *msg, uint8_t location, uint8_t isdn_cause, uint16_t sip_cause, uint8_t socket_cause);
int osmo_cc_get_ie_cause(osmo_cc_msg_t *msg, int ie_repeat, uint8_t *location, uint8_t *isdn_cause, uint16_t *sip_cause, uint8_t *socket_cause);
void osmo_cc_add_ie_display(osmo_cc_msg_t *msg, const char *text);
int osmo_cc_get_ie_display(osmo_cc_msg_t *msg, int ie_repeat, char *text, size_t text_size);
void osmo_cc_add_ie_sdp(osmo_cc_msg_t *msg, const char *sdp);
int osmo_cc_get_ie_sdp(osmo_cc_msg_t *msg, int ie_repeat, char *sdp, size_t sdp_size);
void osmo_cc_add_ie_socket_address(osmo_cc_msg_t *msg, const char *address);
int osmo_cc_get_ie_socket_address(osmo_cc_msg_t *msg, int ie_repeat, char *address, size_t address_size);
void osmo_cc_add_ie_private(osmo_cc_msg_t *msg, uint32_t unique, const uint8_t *data, size_t data_size);
int osmo_cc_get_ie_private(osmo_cc_msg_t *msg, int ie_repeat, uint32_t *unique, uint8_t *data, size_t data_size);

#endif /* OSMO_CC_MSG_H */
