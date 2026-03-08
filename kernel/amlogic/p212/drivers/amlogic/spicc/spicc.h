#ifndef __SPICC_H__
#define __SPICC_H__

#include <linux/amlogic/saradc.h>

#define SPICC_FIFO_SIZE 16
#define SPICC_DEFAULT_BIT_WIDTH 8
#define SPICC_DEFAULT_SPEED_HZ 3000000

#define SPICC_REG_RXDATA (0<<2)
#define SPICC_REG_TXDATA (1<<2)
#define SPICC_REG_CON    (2<<2)
#define SPICC_REG_INT    (3<<2)
#define SPICC_REG_DMA    (4<<2)
#define SPICC_REG_STA    (5<<2)
#define SPICC_REG_PERIOD (6<<2)
#define SPICC_REG_TEST   (7<<2)
#define SPICC_REG_DRADDR (8<<2)
#define SPICC_REG_DWADDR (9<<2)

#define CON_ENABLE bits_desc(SPICC_REG_CON, 0, 1)
#define CON_MODE bits_desc(SPICC_REG_CON, 1, 1)
#define CON_XCH bits_desc(SPICC_REG_CON, 2, 1)
#define CON_SMC bits_desc(SPICC_REG_CON, 3, 1)
#define CON_CLK_POL bits_desc(SPICC_REG_CON, 4, 1)
#define CON_CLK_PHA bits_desc(SPICC_REG_CON, 5, 1)
#define CON_SS_CTL bits_desc(SPICC_REG_CON, 6, 1)
#define CON_SS_POL bits_desc(SPICC_REG_CON, 7, 1)
#define CON_DRCTL bits_desc(SPICC_REG_CON, 8, 2)
#define CON_CHIP_SELECT bits_desc(SPICC_REG_CON, 12, 2)
#define CON_DATA_RATE_DIV bits_desc(SPICC_REG_CON, 16, 3)
#define CON_BITS_PER_WORD bits_desc(SPICC_REG_CON, 19, 6)
#define CON_BURST_LEN bits_desc(SPICC_REG_CON, 25, 7)
#define BURST_LEN_MAX 128

#define INT_TX_EMPTY_EN bits_desc(SPICC_REG_INT, 0, 1)
#define INT_TX_HALF_EN bits_desc(SPICC_REG_INT, 1, 1)
#define INT_TX_FULL_EN bits_desc(SPICC_REG_INT, 2, 1)
#define INT_RX_READY_EN bits_desc(SPICC_REG_INT, 3, 1)
#define INT_RX_HALF_EN bits_desc(SPICC_REG_INT, 4, 1)
#define INT_RX_FULL_EN bits_desc(SPICC_REG_INT, 5, 1)
#define INT_RX_OF_EN bits_desc(SPICC_REG_INT, 6, 1)
#define INT_XFER_COM_EN bits_desc(SPICC_REG_INT, 7, 1)

#define DMA_EN bits_desc(SPICC_REG_DMA, 0, 1)
#define DMA_TX_FIFO_TH bits_desc(SPICC_REG_DMA, 1, 5)
#define DMA_RX_FIFO_TH bits_desc(SPICC_REG_DMA, 6, 5)
#define DMA_NUM_RD_BURST bits_desc(SPICC_REG_DMA, 11, 4)
#define DMA_NUM_WR_BURST bits_desc(SPICC_REG_DMA, 15, 4)
#define DMA_URGENT bits_desc(SPICC_REG_DMA, 19, 1)
#define DMA_THREAD_ID bits_desc(SPICC_REG_DMA, 20, 6)
#define DMA_BURST_NUM bits_desc(SPICC_REG_DMA, 26, 6)

#define STA_TX_EMPTY bits_desc(SPICC_REG_STA, 0, 1)
#define STA_TX_HALF bits_desc(SPICC_REG_STA, 1, 1)
#define STA_TX_FULL bits_desc(SPICC_REG_STA, 2, 1)
#define STA_RX_READY bits_desc(SPICC_REG_STA, 3, 1)
#define STA_RX_HALF bits_desc(SPICC_REG_STA, 4, 1)
#define STA_RX_FULL bits_desc(SPICC_REG_STA, 5, 1)
#define STA_RX_OF bits_desc(SPICC_REG_STA, 6, 1)
#define STA_XFER_COM bits_desc(SPICC_REG_STA, 7, 1)

#define TX_COUNT bits_desc(SPICC_REG_TEST, 0, 5)
#define RX_COUNT bits_desc(SPICC_REG_TEST, 5, 5)
#define DELAY_CONTROL bits_desc(SPICC_REG_TEST, 16, 6)
#define RX_FIFO_RESET bits_desc(SPICC_REG_TEST, 22, 1)
#define TX_FIFO_RESET bits_desc(SPICC_REG_TEST, 23, 1)
#define CLK_FREE_EN bits_desc(SPICC_REG_TEST, 24, 1)

struct spicc_platform_data {
	int device_id;
	struct spicc_regs __iomem *regs;
	struct pinctrl *pinctrl;
	struct clk *clk;
	int num_chipselect;
	int *cs_gpios;
};

//#define SPICC_QUEUE_WORK

/**
 * struct spicc
 * @lock: spinlock for SPICC controller.
 * @msg_queue: link with the spi message list.
 * @wq: work queque
 * @work: work
 * @master: spi master alloc for this driver.
 * @spi: spi device on working.
 * @regs: the start register address of this SPICC controller.
 */
struct spicc {
	spinlock_t lock;
#ifdef SPICC_QUEUE_WORK
	struct list_head msg_queue;
	struct workqueue_struct *wq;
	struct work_struct work;
#endif
	struct spi_master *master;
	struct spi_device *spi;
	struct class cls;

	int device_id;
	struct reset_control *rst;
	struct clk *clk;
	void __iomem *regs;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pullup;
	struct pinctrl_state *pulldown;
	int bits_per_word;
	int mode;
	int speed;
	unsigned int dma_tx_threshold;
	unsigned int dma_rx_threshold;
	unsigned int dma_num_per_read_burst;
	unsigned int dma_num_per_write_burst;
	int irq;
	struct completion completion;
#define FLAG_DMA_EN 0
#define FLAG_TEST_DATA_AUTO_INC 1
#define FLAG_SSCTL 2
#define FLAG_ENHANCE 3
#define FLAG_TIME_DEBUG 4
	unsigned int flags;
	u8 test_data;
	unsigned int delay_control;
	unsigned int cs_delay;
	int remain;
	u8 *txp;
	u8 *rxp;
	int burst_len;
};

static inline bool spicc_get_flag(struct spicc *spicc, unsigned int flag)
{
	bool ret;
	ret = (spicc->flags >> flag) & 1;
	return ret;
}

static inline void spicc_set_flag(struct spicc *spicc, unsigned int flag, bool value)
{
	if (value)
		spicc->flags |= 1<<flag;
	else
		spicc->flags &= ~(1<<flag);
}

static inline void spicc_set_bit_width(struct spicc *spicc, u8 bw)
{
	setb(spicc->regs, CON_BITS_PER_WORD, bw-1);
	spicc->bits_per_word = bw;
}

#endif

