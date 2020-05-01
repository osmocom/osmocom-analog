
typedef struct sim_sniffer {
	enum l1_state		l1_state;
	int 			inverse_order;
	int			atr_count;
	int			atr_if_count;
	uint8_t			atr_if_mask;
	uint8_t			atr_t0;
	uint8_t			atr_ta;
	uint8_t			atr_tb;
	uint8_t			atr_tc;
	uint8_t			atr_td;
	uint8_t			atr_tck;
	enum block_state	block_state;
	uint8_t			block_address;
	uint8_t			block_control;
	uint8_t			block_length;
	uint8_t			block_count;
	uint8_t			block_checksum;
	uint8_t			block_data[256];
} sim_sniffer_t;

void sniffer_reset(sim_sniffer_t *sim);
void sniffer_rx(sim_sniffer_t *sim, uint8_t c);
void sniffer_timeout(sim_sniffer_t *sim);

