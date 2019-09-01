
enum uart_parity {
	UART_PARITY_NONE,
	UART_PARITY_EVEN,
	UART_PARITY_ODD,
	UART_PARITY_MARK,
	UART_PARITY_SPACE,
};

/* uart flags */
#define UART_PARITY_ERROR	(1 << 0)
#define UART_CODE_VIOLATION	(1 << 1)
#define UART_BREAK		(1 << 2)

typedef struct uart {
	void *inst;
	int (*tx_cb)(void *inst);
	void (*rx_cb)(void *inst, int data, uint32_t flags);
	uint8_t data_bits;
	enum uart_parity parity;
	uint8_t stop_bits;
	int last_bit;
	uint32_t tx_data;
	uint32_t rx_data;
	int tx_pos;
	int rx_pos;
	int length;
} uart_t;

int uart_init(uart_t *uart, void *inst, uint8_t data_bits, enum uart_parity parity, uint8_t stop_bits, int (*tx_cb)(void *inst), void (*rx_cb)(void *inst, int data, uint32_t flags));
int uart_tx_bit(uart_t *uart);
int uart_is_tx(uart_t *uart);
void uart_rx_bit(uart_t *uart, int bit);

