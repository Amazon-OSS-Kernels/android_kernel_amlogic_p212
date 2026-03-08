
#include <linux/device.h>
#include "spicc.h"
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/spi/spi.h>


extern void spicc_set_clk(struct spicc *spicc, int speed);
extern void spicc_set_mode(struct spicc *spicc, u8 mode);
extern int spicc_setup(struct spi_device *spi);
extern void spicc_handle_one_msg(struct spicc *spicc, struct spi_message *m);


static ssize_t show_setting(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret = 0;
	struct spicc *spicc = container_of(class, struct spicc, cls);

	if (!strcmp(attr->attr.name, "speed"))
		ret = sprintf(buf, "speed=%d\n", spicc->speed);
	else if (!strcmp(attr->attr.name, "mode"))
		ret = sprintf(buf, "mode=%d\n", spicc->mode);
	else if (!strcmp(attr->attr.name, "bit_width"))
		ret = sprintf(buf, "bit_width=%d\n", spicc->bits_per_word);
	else if (!strcmp(attr->attr.name, "flags"))
		ret = sprintf(buf, "flags=0x%x\n", spicc->flags);
	else if (!strcmp(attr->attr.name, "test_data"))
		ret = sprintf(buf, "test_data=0x%x\n", spicc->test_data);
	else if (!strcmp(attr->attr.name, "help")) {
		pr_info("SPI device test help\n");
		pr_info("echo cs_gpio speed mode bits_per_word num");
		pr_info("[wbyte1 wbyte2...] >test\n");
	}
	return ret;
}


static ssize_t store_setting(
		struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int value;
	struct spicc *spicc = container_of(class, struct spicc, cls);

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	if (!strcmp(attr->attr.name, "speed"))
		spicc_set_clk(spicc, value);
	else if (!strcmp(attr->attr.name, "mode"))
		spicc_set_mode(spicc, value);
	else if (!strcmp(attr->attr.name, "bit_width"))
		spicc_set_bit_width(spicc, value);
	else if (!strcmp(attr->attr.name, "flags"))
		spicc->flags = value;
	else if (!strcmp(attr->attr.name, "test_data"))
		spicc->test_data = (u8)(value & 0xff);
	return count;
}

static ssize_t store_test(
		struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	struct spicc *spicc = container_of(class, struct spicc, cls);
	struct device *dev = spicc->master->dev.parent;
	unsigned int cs_gpio, speed, mode, bits_per_word, num;
	u8 *tx_buf, *rx_buf;
	unsigned long value;
	char *kstr, *str_temp, *token;
	int i;
	struct spi_transfer t;
	struct spi_message m;

	if (sscanf(buf, "%d%d%d%d%d", &cs_gpio, &speed,
			&mode, &bits_per_word, &num) != 5) {
		dev_err(dev, "error test data\n");
		return count;
	}
	kstr = kstrdup(buf, GFP_KERNEL);
	tx_buf = kzalloc(num, GFP_KERNEL | GFP_DMA);
	rx_buf = kzalloc(num, GFP_KERNEL | GFP_DMA);
	if (IS_ERR(kstr) || IS_ERR(tx_buf) || IS_ERR(rx_buf)) {
		dev_err(dev, "failed to alloc tx rx buffer\n");
		goto test_end;
	}

	str_temp = kstr;
	/* skip pass over "cs_gpio speed mode bits_per_word num" */
	for (i = 0; i < 5; i++)
		strsep(&str_temp, ", ");
	for (i = 0; i < num; i++) {
		token = strsep(&str_temp, ", ");
		if (!token || kstrtoul(token, 16, &value))
			break;
		tx_buf[i] = (u8)(value & 0xff);
	}
	for (; i < num; i++) {
		tx_buf[i] = spicc->test_data;
		if (spicc_get_flag(spicc, FLAG_TEST_DATA_AUTO_INC))
			spicc->test_data++;
	}

	spi_message_init(&m);
	m.spi = spi_alloc_device(spicc->master);
	if (cs_gpio < 1000)
		m.spi->cs_gpio = cs_gpio;
	else {
		m.spi->cs_gpio = -ENOENT;
		m.spi->chip_select = cs_gpio - 1000;
	}
	m.spi->max_speed_hz = speed;
	m.spi->mode = mode;
	m.spi->bits_per_word = bits_per_word;
	spicc_setup(m.spi);
	memset(&t, 0, sizeof(t));
	t.tx_buf = (void *)tx_buf;
	t.rx_buf = (void *)rx_buf;
	t.len = num;
	spi_message_add_tail(&t, &m);
	spicc_handle_one_msg(spicc, &m);
	spi_dev_put(m.spi);

	dev_info(dev, "read back data ok\n");
	for (i = 0; i < min_t(size_t, 32, num); i++)
		dev_info(dev, "[%d]: 0x%2x, 0x%2x\n", i, tx_buf[i], rx_buf[i]);
test_end:
	kfree(kstr);
	kfree(tx_buf);
	kfree(rx_buf);
	return count;
}

struct class_attribute spicc_class_attrs[] = {
		__ATTR(test, S_IWUSR, NULL, store_test),
		__ATTR(test_data, S_IRUGO|S_IWUSR, show_setting, store_setting),
		__ATTR(speed, S_IRUGO|S_IWUSR, show_setting, store_setting),
		__ATTR(mode, S_IRUGO|S_IWUSR, show_setting, store_setting),
		__ATTR(bit_width, S_IRUGO|S_IWUSR, show_setting, store_setting),
		__ATTR(flags, S_IRUGO|S_IWUSR, show_setting, store_setting),
		__ATTR(help, S_IRUGO, show_setting, NULL),
		__ATTR_NULL
};

