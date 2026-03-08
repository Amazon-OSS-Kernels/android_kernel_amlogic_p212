/*
 * drivers/amlogic/irblaster/irblaster.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include "irblaster.h"
#include <linux/amlogic/gpio-amlogic.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>
#include <linux/of_irq.h>

#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>


#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#include <linux/vmalloc.h>
#ifndef BLASTER_METRICS_STR_LEN
#define BLASTER_METRICS_STR_LEN 128
#endif
#endif



/* #define DEBUG */

#define DEVICE_NAME 					"irblaster"
#define DEIVE_COUNT 					32
#define PS_SIZE 					10
#define REMOTE_FREQ					38000
#define REMOTE_DUTY_CYCLE 				33
#define FULL_DUTY 					100
#define LIMIT_DUTY 					25
#define MAX_DUTY 					75
#define LIMIT_FREQ 					25000
#define MAX_FREQ 					60000
#define CYCLE_DIVIDE 					1000000
#define COUNT_DELAY_MASK 				0X3ff
#define MODULE_CLK 					3
#define TIMEBASE_SHIFT 				10
#define OUTPUT_LEVEL_SHIFT 				12
#define FIFO_WRITE 					0x10000

#define REMOTE_PIN_MUX_SHIFT				31

#define FIFO_BUSY_SHIFT 				26
#define FIFO_EMPTY_SHIFT 				24
#define RESET_SHIFT 					23
#define INIT_STATE_SHIFT 				2
#define ENABLE_BLASTER_SHIFT				0
#define MODULATOR_TB_SHIFT				12

#define HIGH_PUSE_SHIFT 				16
#define LOW_PUSE_SHIFT					0

#define IR_TX_EVENT_SIZE 				4
#define IR_TX_BUFFER_SIZE 				1024
#define MAX_LOOP_CNT	 				0X10000
#define STARK_EVT_BOARDID				20020017
#define IR_DETECT_DEBOUNCE				100

#define PARSE_GPIO_NUM_PROP(node, prop_name, value, n) do { \
	value = desc_to_gpio( \
		of_get_named_gpiod_flags(node, prop_name, n, NULL)); \
} while (0)

#define PARSE_AND_INIT_GPIO_IRQ(node, prop_name, prop, gpio, level) do { \
	if (!of_property_read_u32(node, prop_name, &prop)) { \
		gpio_for_irq(gpio, AML_GPIO_IRQ(prop, \
			FILTER_NUM7, level)); \
	} \
} while (0)

static dev_t amirblaster_id;
static struct class *irblaster_class;
static struct device *irblaster_dev;
static struct cdev amirblaster_device;
static struct blaster_window *irblaster;
static DEFINE_MUTEX(irblaster_file_mutex);
static int debug_enable = 0x00;
static int board_id;

#ifdef CONFIG_AMAZON_METRICS_LOG
static char jackdetect_flag = NULL;
static char jack_fault_detect_flag = NULL;
static char handlerinit = NULL;
#endif

struct irtx_dev {
	struct device *dev;
	struct task_struct	*thread;
	int ir_ext_enable_pin;
	int irdetect_gpio;
	int irdetect_gpioirq;
	int jackremove_gpioirq;
	int irdetect_irq;
	int irremove_irq;
	int irfaultdetect_gpio;
	int irfaultdetect_gpioirq;
	int irfaultdetect_irq;
	bool plug_flag;
	struct timer_list timer;
	unsigned int timer_debounce;
	struct work_struct work_irdetect;
};


struct tx_event {
	struct list_head list;
	int size;
	int buffer[IR_TX_BUFFER_SIZE];
};

DECLARE_KFIFO(fifo, struct tx_event *, IR_TX_EVENT_SIZE);

static DEFINE_SPINLOCK(event_lock);
struct tx_event event_pool[IR_TX_EVENT_SIZE];
struct list_head event_pool_head;
static struct irtx_dev *tx_dev;

extern int idme_get_board_long_rev(void);
int ishevt(int boardid){
	return boardid<=STARK_EVT_BOARDID?1:0;
}

static int irblaster_dbg(const char* fmt, ...)
{
	va_list args;
	int r;

	if (!debug_enable)
		return 0;
	va_start(args, fmt);
	r = vprintk(fmt, args);
	va_end(args);
	return r;
}

static int event_pool_init(void)
{
	int i;
	struct tx_event *ev;
	struct tx_event *ev_head;

	INIT_LIST_HEAD(&event_pool_head);
	ev = kzalloc(sizeof(struct tx_event)*IR_TX_EVENT_SIZE,
		GFP_KERNEL);
	if (!ev)
		return -ENOMEM;
	ev_head = ev;
	for (i = 0; i < IR_TX_EVENT_SIZE; i++) {
		printk("ev=%p\n",ev);
		list_add_tail(&ev->list, &event_pool_head);
		ev++;
		
	}
	return 0;
}

static struct tx_event *event_get(void)
{
	struct tx_event *ev = NULL;
	unsigned long flags;
	
	spin_lock_irqsave(&event_lock, flags);
	list_for_each_entry(ev, &event_pool_head, list) {
		printk("ev=%p\n",ev);
		if (ev){
			list_del((&ev->list));
			break;
		}
	}
	spin_unlock_irqrestore(&event_lock, flags);
	irblaster_dbg("event_get\n");
	return ev;
}

static void event_put(struct tx_event *ev)
{
	unsigned long flags;
	spin_lock_irqsave(&event_lock, flags);
	list_add_tail(&ev->list, &event_pool_head);
	spin_unlock_irqrestore(&event_lock, flags);
	irblaster_dbg("event_put\n");
}



static int send_bit(unsigned int hightime, unsigned int lowtime, unsigned int cycle)
{
	unsigned int count_delay;
	uint32_t val;
	int n = 0;
	int tb[3] = {
		1, 10, 100
	};
	//[11:10] = 2'b01,then set the timebase 10us.
	//[9:0] = 10'd,the timecount = N+1;
	/*
		hightime: modulator signal.
	*/
	//count_delay = (((hightime*10)*38+500)/1000-1)&0x3ff;
	/*
	MODULATOR_TB:
		00:	system clock ¡°clk¡±
		01:	mpeg_xtal3_tick
		10:	mpeg_1uS_tick
		11:	mpeg_10uS_tick
	lowtime<1024,n=0,timebase=1us
	1024<=lowtime<10240,n=1,timebase=10us
	*/
	/*
	AO_IR_BLASTER_ADDR2
	bit12: output level(or modulation enable/disable:1=enable)
	bit[11:10]: Timebase :
				00=1us
				01=10us
				10=100us
				11=Modulator clock
	bit[9:0]: Count of timebase units to delay
	*/

	count_delay = (((hightime + cycle/2) / cycle) - 1) & COUNT_DELAY_MASK;
	//count_delay = ((hightime*10+13)/26-1)*0x3ff;
	//val = (0x10000 & ~(1<<12)) | (3<<10) | (count_delay<<0);
	//printk("count_delay=%d\n", count_delay);
	val = (FIFO_WRITE | (1 << OUTPUT_LEVEL_SHIFT)) | (MODULE_CLK << TIMEBASE_SHIFT) | (count_delay << 0);
	aml_write_aobus( AO_IR_BLASTER_ADDR2, val);

	/*
	lowtime<1024,n=0,timebase=1us
	1024<=lowtime<10240,n=1,timebase=10us
	10240<=lowtime,n=2,timebase=100us
	*/
	n = lowtime >> 10; 
	if (n > 0 && n < 10)
		n = 1;
	else if (n >= 10)
		n = 2;
	lowtime = (lowtime + (tb[n] >> 1))/tb[n];
	count_delay = (lowtime-1) & COUNT_DELAY_MASK;
	val = (FIFO_WRITE | (0 << OUTPUT_LEVEL_SHIFT)) |
		(n << TIMEBASE_SHIFT) | (count_delay << 0);
	aml_write_aobus( AO_IR_BLASTER_ADDR2, val);
	//printk("lowtime=%d\n", lowtime);

	return 0;
}


#define SEND_BIT_NUM           64  /* HW FIFO Length */
#define FIFO_BEFORE_EMPTY_NS   1000000 /* spare time before fifo empty */
#define TX_SEND_ALL_TIMEOUT_MSEC   1000
#define TX_SEND_FIFO_TIMEOUT_USEC  2200

static int ir_txfifo_fill(struct blaster_window * cw)
{
	unsigned int consumerir_cycle =	CYCLE_DIVIDE / irblaster->consumerir_freqs;
	unsigned int *pData;
	unsigned int i, nLen, fifolevel;
	unsigned int totalsum = 0;

	pData = &cw->winArray[cw->tx_idx];
	if (ishevt(board_id))
		fifolevel = 0;
	else
		fifolevel = (aml_read_aobus(AO_IR_BLASTER_ADDR0) & 0x00FF0000) >> 16;
	for (i = 0; (i < SEND_BIT_NUM - fifolevel) && cw->tx_idx < cw->winNum;) {
		send_bit(*pData, *(pData+1), consumerir_cycle);
		totalsum += *pData + *(pData+1);
		pData += 2;
		cw->tx_idx += 2;
		i += 2;
	}
	irblaster_dbg("ir fifo wrote %d bytes, totalsum %d\n", i, totalsum);

	return totalsum;
}

/* transfer the totalsum to how long we need to wait, in order to set timer interrupt
   leave some room for interrupt response
 */
static long totalsum2ns(int totalsum)
{
	return (totalsum*1000 > FIFO_BEFORE_EMPTY_NS ? 
			totalsum*1000 - FIFO_BEFORE_EMPTY_NS : 
			totalsum*1000);
}

static enum hrtimer_restart ir_txfifo_timer_func(struct hrtimer *timer)
{
	unsigned int totalsum = 0;
	unsigned long cnt;
	ktime_t now;
	struct blaster_window *cw = container_of(timer,
						     struct blaster_window,
						     ir_txfifo_timer);

	irblaster_dbg("ir timer enter\n");
	/* wait for fifo empty */
	if(ishevt(board_id)) {
		cnt = 0;
		while (!(aml_read_aobus(AO_IR_BLASTER_ADDR0) & (1<<FIFO_EMPTY_SHIFT))){
			udelay(2);
			cnt += 2;
			if (cnt >= TX_SEND_FIFO_TIMEOUT_USEC) {
				pr_info("%s waited empty for more than %ldus\n", __func__, TX_SEND_FIFO_TIMEOUT_USEC);
				return HRTIMER_NORESTART;
			}
		}
		/* wait for fifo not busy */
		cnt = 0;
		while (aml_read_aobus(AO_IR_BLASTER_ADDR0) & (1<<FIFO_BUSY_SHIFT)){
			udelay(2);
			cnt += 2;
			if (cnt >= TX_SEND_FIFO_TIMEOUT_USEC) {
				pr_info("%s waited busy for more than %ldus\n", __func__, TX_SEND_FIFO_TIMEOUT_USEC);
				return HRTIMER_NORESTART;
			}
		}
	}
	/* fill next segment if there's till data */
	if (cw->winNum > cw->tx_idx){
		/* reset first */
		if(ishevt(board_id)) {
			aml_write_aobus(AO_RTI_GEN_CTNL_REG0 ,
				aml_read_aobus(AO_RTI_GEN_CTNL_REG0) | (1 << RESET_SHIFT)); /*reset*/
			udelay(2);
			aml_write_aobus(AO_RTI_GEN_CTNL_REG0 ,
				aml_read_aobus(AO_RTI_GEN_CTNL_REG0) & ~(1 << RESET_SHIFT)); /*reset*/
		}

		totalsum = ir_txfifo_fill(cw);

		now = hrtimer_cb_get_time(timer);
		hrtimer_forward(timer, now, ns_to_ktime(totalsum2ns(totalsum)));

		return HRTIMER_RESTART;
	}
	else {
		/* all tx is done, now wake up waiting thread */
		complete(&cw->tx_completion);
	}

	return HRTIMER_NORESTART;
}

static void send_all_frame(struct blaster_window * cw)
{
	int i, ret;
	unsigned int consumerir_cycle =	CYCLE_DIVIDE / irblaster->consumerir_freqs;
	unsigned int high_ct, low_ct;
	unsigned int totalsum = 0;
	unsigned long flags;

	pr_info("start sending frame\n");
	irblaster_dbg("cw->winNum = %d\n", cw->winNum);
	irblaster_dbg("cw->winArray = ");
	for ( i = 0; i < cw->winNum; i++ ) {
		irblaster_dbg("%d,", cw->winArray[i]);
		if ( i % 10 == 9 )
			irblaster_dbg("\n");
	}
	irblaster_dbg("\n");

	/*reset*/
	aml_write_aobus(AO_RTI_GEN_CTNL_REG0 ,
		aml_read_aobus(AO_RTI_GEN_CTNL_REG0) | (1 << RESET_SHIFT));
	udelay(2);
	aml_write_aobus(AO_RTI_GEN_CTNL_REG0 ,
		aml_read_aobus(AO_RTI_GEN_CTNL_REG0) & ~(1 << RESET_SHIFT));

	/*
	1. disable ir blaster
	2. set the modulator_tb = 2'10; mpeg_1uS_tick 1us

	*/
	aml_write_aobus(AO_IR_BLASTER_ADDR0, ((1 << INIT_STATE_SHIFT) | (2 << MODULATOR_TB_SHIFT)) & ~(1 << ENABLE_BLASTER_SHIFT));
	/*
	1. set mod_high_count = 13
    2. set mod_low_count = 13
 	3. 60khz 8, 38k-13us, 12
	*/
	high_ct = consumerir_cycle * irblaster->consumerir_dutycycle / FULL_DUTY;
	low_ct = consumerir_cycle - high_ct;
	aml_write_aobus(AO_IR_BLASTER_ADDR1,
		((high_ct - 1) << HIGH_PUSE_SHIFT) | ((low_ct - 1) << LOW_PUSE_SHIFT));

	/* Setting this bit to 1 initializes the output to be high.*/
	aml_write_aobus(AO_IR_BLASTER_ADDR0,
		aml_read_aobus(AO_IR_BLASTER_ADDR0) & ~(1 << INIT_STATE_SHIFT));

	/*enable irblaster*/
	aml_write_aobus(AO_IR_BLASTER_ADDR0,
		aml_read_aobus(AO_IR_BLASTER_ADDR0) | (1 << ENABLE_BLASTER_SHIFT));

	/* init hrtimer */
	if (hrtimer_active(&cw->ir_txfifo_timer)) {
		hrtimer_try_to_cancel(&cw->ir_txfifo_timer);
	}
	hrtimer_init(&cw->ir_txfifo_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cw->ir_txfifo_timer.function = ir_txfifo_timer_func;

	spin_lock_irqsave(&event_lock, flags);
	/* fill fifo, start timer */
	cw->tx_idx = 0;
	totalsum = ir_txfifo_fill(cw);
	hrtimer_start(&cw->ir_txfifo_timer, ns_to_ktime(totalsum2ns(totalsum)), HRTIMER_MODE_REL);
	spin_unlock_irqrestore(&event_lock, flags);
	/* wait till finish, should finish within 2sec */
	ret = wait_for_completion_interruptible_timeout(&cw->tx_completion, msecs_to_jiffies(totalsum/1000 + TX_SEND_ALL_TIMEOUT_MSEC));
	if (!ret) 
		pr_err("%s timed out", __func__);

	pr_info("The all frame finished !!\n");
}

static int ir_tx_thread(void *data)
{
	struct irtx_dev *dev = (struct irtx_dev *)data;
	struct tx_event *ev = NULL;
	int retval, i;

	while (!kthread_should_stop()) {
		pr_info("wakeup ir_tx_thread\n");
		retval = kfifo_len(&fifo);
		if (retval <= 0) {
			set_current_state(TASK_INTERRUPTIBLE);
			if (kthread_should_stop())
				set_current_state(TASK_RUNNING);
			schedule();
			continue;
		}
		retval = kfifo_get(&fifo, &ev);
		if (retval) {
			irblaster->winNum = ev->size;
			for ( i = 0; i < irblaster->winNum; i++)
				irblaster->winArray[i] = ev->buffer[i];
			pr_info("send_all_frame.size=%d\n",ev->size);
			send_all_frame(irblaster);
			event_put(ev);
		} else
			pr_err("kfifo_get fail, retval=%d,ev=%p\n", retval, ev);
	}

	return 0;
}

/**
  * Function to set the irblaster Carrier Frequency,
  * The modulator is typically run between 32khz and 56khz.
  *
  * @param[in] pointer to irblaster structure.
  * @param[in] carrirer freqs value.
  * \return Reuturns 0 on success else return the error value.
 */

int set_consumerir_freqs(struct blaster_window *irblaster, int consumerir_freqs)
{
	if (consumerir_freqs > MAX_FREQ || consumerir_freqs < LIMIT_FREQ)
		return -1;
	else
		irblaster->consumerir_freqs = consumerir_freqs;
	return 0;
}

/**
  * Function to get the irblaster cur Carrier Frequency.
  *
  * @param[in] pointer to irblaster structure.
  * \return Reuturns freqs.
 */

static int  get_consumerir_freqs(struct blaster_window *irblaster)
{
	return irblaster->consumerir_freqs;
}

static int  set_duty_cycle(int duty_cycle)
{
	if (duty_cycle > MAX_DUTY || duty_cycle < LIMIT_DUTY)
		return -1;
	else
		irblaster->consumerir_dutycycle = duty_cycle;

	return 0;
}



static int aml_irblaster_open(struct inode *inode, struct file *file)
{
	irblaster_dbg("aml_irblaster_open()\n");
	return 0;
}

static int send(const char * buf, int len)
{
	int i=0, j=0, m=0, ret=0;
	int val;
	char tone[PS_SIZE];
#ifdef CONFIG_AMAZON_METRICS_LOG
	char *blaster_metric_prefix = "blaster:def:monitor=1;CT;1";
	char mbuf[BLASTER_METRICS_STR_LEN + 1];
	static char jack_print = NULL;
	static char jack_fault_print = NULL;
#endif
	for(i = 0; i < len; i++){
		if(buf[i] == '\0') {
			break;
		} else if (buf[i] == 's'){
			tone[j] = '\0';
			ret = kstrtoint(tone, 10, &val);
			irblaster->winArray[m] = val*10;
			j = 0;
			m++;
			if(m >= IR_TX_BUFFER_SIZE)
				break;
			continue;
		}
		if(j < PS_SIZE){
			tone[j] = buf[i];
			j++;
		} else {
			pr_err("send timing value is out of range\n");
			return -ENOMEM;
		}
	}

	irblaster->winNum = m;
	pr_info("send_all_frame.size=%d\n",irblaster->winNum);
	send_all_frame(irblaster);
	memset(irblaster->winArray, 0, sizeof(irblaster->winArray));

#ifdef CONFIG_AMAZON_METRICS_LOG
	if(jackdetect_flag && !jack_print){
		jack_print = 1;
		jackdetect_flag = 0;
		snprintf(mbuf, BLASTER_METRICS_STR_LEN,
			"%s,irjack_dtected_%d;CT;",
			blaster_metric_prefix, jack_print);
		log_to_metrics(ANDROID_LOG_INFO, "BlasterEvent", mbuf);
	}

	if(jack_fault_detect_flag && !jack_fault_print){
		jack_fault_print = 1;
		jack_fault_detect_flag = 0;
		snprintf(mbuf, BLASTER_METRICS_STR_LEN,
			"%s,irjack_fault_dtected_%d;CT;",
			blaster_metric_prefix, jack_fault_print);
		log_to_metrics(ANDROID_LOG_INFO, "BlasterEvent", mbuf);
    }
#endif

	return 0;
}

static long aml_irblaster_ioctl(struct file *filp, unsigned int cmd,
				unsigned long args)
{

	int consumerir_freqs = 0, duty_cycle = 0;
	static int psize;
	s32 r = 0;
	void __user *argp = (void __user *)args;
	char *sendcode = NULL;
	irblaster_dbg("aml_irblaster_ioctl()  0x%4x\n ", cmd);
	switch (cmd) {
	case SET_KSIZE:
		pr_info("in set psize\n");
		if(get_user(psize, (int*)argp) < 0)
			return -EFAULT;
		break;
	case CONSUMERIR_TRANSMIT:
		psize = psize ? psize:IR_TX_BUFFER_SIZE;
		sendcode = kcalloc(psize, sizeof(char), GFP_KERNEL);

		if (sendcode == NULL) {
			pr_err("irblaster: can't get sendcode memory; aborting\n");
			return -ENOMEM;
		}
		if (copy_from_user(sendcode, (char*)argp,
					psize))
			return -EFAULT;
		pr_info("send code is %s\n", sendcode);
		r = send(sendcode, psize);
		kfree(sendcode);
		break;
	case GET_CARRIER:
		pr_info("in get freq\n");
		consumerir_freqs = get_consumerir_freqs(irblaster);
		put_user(consumerir_freqs, (int*)argp);
		return consumerir_freqs;
	case SET_CARRIER:
		pr_info("in set freq\n");
		if(get_user(consumerir_freqs, (int*)argp) < 0)
			return -EFAULT;
		r = set_consumerir_freqs(irblaster, consumerir_freqs);
		break;
	case SET_DUTYCYCLE:
		pr_info("in set duty_cycle\n");
		if (copy_from_user(&duty_cycle, argp, sizeof(int)))
			return -EFAULT;
		get_user(duty_cycle, (int*)argp);
		r = set_duty_cycle(duty_cycle);
		break;

	default:
		r = -ENOIOCTLCMD;
		break;
	}

	return r;
}
static int aml_irblaster_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t show_plug(struct device* dev,
	struct device_attribute* attr, char* buf)
{
	struct irtx_dev *irdev = dev_get_drvdata(dev);
	sprintf(buf, "%d\n", irdev->plug_flag);
	return strlen(buf);
}

static ssize_t show_debug(struct device* dev,
	struct device_attribute* attr, char* buf)
{
	if (debug_enable)
		sprintf(buf, "debug=enable\n");
	else
		sprintf(buf, "debug=disable\n");
	return strlen(buf);
}

static ssize_t store_debug(struct device * dev,
	struct device_attribute *attr, const char * buf, size_t count)
{
	if (!strncmp(buf, "enable", 1)) {
		debug_enable = 1;
		pr_info("enable debug\n");
	}
	else if(!strncmp(buf, "disable", 1)) {
		debug_enable = 0;
		pr_info("disable debug\n");
	}
	return strlen(buf);;
}

static ssize_t store_carrier_freq(struct device* dev,
	struct device_attribute* attr, const char* buf, size_t count)

{
	int ret;
	mutex_lock(&irblaster->lock);
	ret = kstrtoint(buf, 10, &irblaster->consumerir_freqs);
		if (ret) {
			pr_err("IR_OUT: Invalid input for carrier_freq\n");
			return ret;
		}
	pr_info("carrier_freq is %d\n", irblaster->consumerir_freqs);
	mutex_unlock(&irblaster->lock);
	return strlen(buf);
}

static ssize_t store_duty_cycle(struct device* dev,
	struct device_attribute* attr, const char* buf, size_t count)

{
	int ret, dc;
	mutex_lock(&irblaster->lock);
	ret = kstrtoint(buf, 10, &dc);
	if (ret) {
		pr_err("IR_OUT: Invalid input for duty_cycle\n");
		return ret;
	}
	ret = set_duty_cycle(dc);
	if (ret) {
		pr_err("IR_OUT: Invalid input for duty_cycle\n");
		return ret;
	}
	pr_info("duty_cycle is %d\n", irblaster->consumerir_dutycycle);
	mutex_unlock(&irblaster->lock);
	return strlen(buf);
}


static ssize_t show_log(struct device * dev,
	struct device_attribute *attr, char * buf)
{
	memset(buf, 0, PAGE_SIZE);
	return strlen(buf);
}

static ssize_t show_send_value(struct device * dev,
	struct device_attribute *attr, char * buf)
{
	memset(buf, 0, PAGE_SIZE);
	return strlen(buf);
}

static ssize_t store_send(struct device * dev,
	struct device_attribute *attr, const char * buf, size_t count)
{
	/*alloc memory*/
	struct tx_event *ev;
	int i=0, j=0, m=0, ret=0;
	int val;
	char tone[PS_SIZE];
	struct irtx_dev *irdev = dev_get_drvdata(dev);
	ev = event_get();
	if (!ev) {
		pr_err("please wait for send\n");
		return -ENOMEM;
	}

	while(buf[i] != '\0'){
		if(buf[i] == 's'){
			tone[j] = '\0';
			ret = kstrtoint(tone, 10, &val);
			irblaster->winArray[m] = val*10;
			j = 0;
			i++;
			m++;
			if(m >= IR_TX_BUFFER_SIZE)
				break;
			continue;
		}

		tone[j] = buf[i];
		i++;
		j++;

		if(j >= PS_SIZE){
			pr_err("send timing value is out of range\n");
			return -ENOMEM;
		}
	}

	ev->size = m;
	irblaster->winNum = m;
	pr_info("send_all_frame.size=%d\n",irblaster->winNum);
	send_all_frame(irblaster);
	memset(irblaster->winArray, 0, sizeof(irblaster->winArray));
	/*to send cycle to,*/
//	kfifo_put(&fifo, (const struct tx_event *)ev);
	
	/*to wake up ir_tx_thread*/
//	wake_up_process(irdev->thread);
	return count;
}

static DEVICE_ATTR(debug, S_IWUSR | S_IRUGO, show_debug, store_debug);
static DEVICE_ATTR(log, S_IRUGO, show_log, NULL);
static DEVICE_ATTR(sendvalue, S_IRUGO, show_send_value, NULL);
static DEVICE_ATTR(send, S_IWUSR, NULL, store_send);
static DEVICE_ATTR(carrier_freq, S_IWUSR, NULL, store_carrier_freq);
static DEVICE_ATTR(duty_cycle, S_IWUSR, NULL, store_duty_cycle);
static DEVICE_ATTR(plug, S_IRUGO, show_plug, NULL);

static void gpio_tyrion_gpio_timer(void *data)
{
	struct irtx_dev *dev = (struct irtx_dev *)data;
	schedule_work(&(dev->work_irdetect));
}

static irqreturn_t ir_detect_handler(int irq, void *data)
{
	struct irtx_dev *dev = (struct irtx_dev *)data;

	if(handlerinit){
		jackdetect_flag = 1;
		dev->plug_flag = 1;
		if (dev->timer_debounce)
			mod_timer(&dev->timer,jiffies + msecs_to_jiffies(dev->timer_debounce));
	}

	return IRQ_HANDLED;
}

static irqreturn_t ir_remove_handler(int irq, void *data)
{
	struct irtx_dev *dev = (struct irtx_dev *)data;

	if(handlerinit){
		jackdetect_flag = 1;
		dev->plug_flag = 0;
		if (dev->timer_debounce)
			mod_timer(&dev->timer,jiffies + msecs_to_jiffies(dev->timer_debounce));
	}
	return IRQ_HANDLED;
}

static irqreturn_t ir_fault_detect_handler(int irq, void *data)
{
	struct irtx_dev *dev = (struct irtx_dev *)data;
#ifdef CONFIG_AMAZON_METRICS_LOG
	if(handlerinit)
		jack_fault_detect_flag = 1;
#endif
	return IRQ_HANDLED;
}

static void work_irdetecting(struct work_struct *work)
{
	struct irtx_dev *dev = container_of(work, struct irtx_dev, work_irdetect);
	char data[2][32];
	char *envp[3] = {
		[0] = data[0],
		[1] = data[1],
		[2] = NULL,
	};
	if(jackdetect_flag) {
		snprintf(data[0], sizeof(data[0]), "plug=%d", dev->plug_flag);
		pr_info("generate IR detect uevent %s\n", envp[0]);
		kobject_uevent_env(&dev->dev->kobj, KOBJ_CHANGE, envp);
	}
}

static const struct file_operations aml_irblaster_fops = {
	.owner		= THIS_MODULE,
	.open		= aml_irblaster_open,
	.compat_ioctl = aml_irblaster_ioctl,
	.unlocked_ioctl = aml_irblaster_ioctl,
	.release	= aml_irblaster_release,
};

static int  aml_irblaster_probe(struct platform_device *pdev)
{
	int r, val, ret;
	struct irtx_dev *dev;
	char data[32];
	char *envp[3] = {
		[0] = data,
		[1] = NULL,
		[2] = NULL,
	};
	pr_info("irblaster probe\n");
	dev = kzalloc(sizeof(struct irtx_dev), GFP_KERNEL);
	if (!dev) {
		pr_info("");
		return -ENOMEM;
	}
	irblaster = kzalloc(sizeof(struct blaster_window), GFP_KERNEL);
	if (irblaster == NULL)
		return -1;
	memset(irblaster, 0, sizeof(struct blaster_window));

	irblaster->consumerir_freqs = REMOTE_FREQ;
	irblaster->consumerir_dutycycle = REMOTE_DUTY_CYCLE;

	if (!pdev->dev.of_node) {
		pr_err("aml_irblaster: pdev->dev.of_node == NULL!\n");
		return -1;
	}
	r = alloc_chrdev_region(&amirblaster_id, 0, DEIVE_COUNT, DEVICE_NAME);
	if (r < 0) {
		pr_err("Can't register major for ir irblaster device\n");
		return r;
	}
	cdev_init(&amirblaster_device, &aml_irblaster_fops);
	amirblaster_device.owner = THIS_MODULE;
	cdev_add(&(amirblaster_device), amirblaster_id, DEIVE_COUNT);
	irblaster_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(irblaster_class)) {
		unregister_chrdev_region(amirblaster_id, DEIVE_COUNT);
		pr_err("Can't create class for ir irblaster device\n");
		return -1;
	}
	irblaster_dev = device_create(irblaster_class, NULL,
					amirblaster_id, dev,
				      "irblaster%d", 1);
	if (irblaster_dev == NULL) {
		pr_err("irblaster_dev create error\n");
		class_destroy(irblaster_class);
		return -EEXIST;
	}

	mutex_init(&irblaster->lock);
	init_completion(&irblaster->tx_completion);
	device_create_file(irblaster_dev, &dev_attr_debug);
	device_create_file(irblaster_dev, &dev_attr_log);
	device_create_file(irblaster_dev, &dev_attr_sendvalue);
	device_create_file(irblaster_dev, &dev_attr_send);
	device_create_file(irblaster_dev, &dev_attr_duty_cycle);
	device_create_file(irblaster_dev, &dev_attr_carrier_freq);
	device_create_file(irblaster_dev, &dev_attr_plug);

	event_pool_init();
	INIT_KFIFO(fifo);

	dev->thread = kthread_run(ir_tx_thread, dev,
		 "ir-blaster-thread");

	val = aml_read_aobus(AO_RTI_PIN_MUX_REG) & ~0x01; /*clear ao_reg0*/

	/*clear ao_reg21*/
	aml_write_aobus(AO_RTI_PIN_MUX_REG , val | (1 << REMOTE_PIN_MUX_SHIFT));

	dev->ir_ext_enable_pin = of_get_named_gpio(pdev->dev.of_node, "enable_pin", 0);
	ret = gpio_request(dev->ir_ext_enable_pin, "enable_pin");
	if(ret)
		goto err;
	ret = gpio_direction_output(dev->ir_ext_enable_pin, 1);
	if(ret)
		goto err2;
	dev->irdetect_gpio = of_get_named_gpio(pdev->dev.of_node, "irdetect", 0);
	ret = gpio_request(dev->irdetect_gpio, "irdetect");
	if (ret < 0)
		goto err2;
	dev->plug_flag = gpio_get_value(dev->irdetect_gpio);
	dev->timer_debounce = IR_DETECT_DEBOUNCE;
	dev->dev = &pdev->dev;
	INIT_WORK(&(dev->work_irdetect), work_irdetecting);
	setup_timer(&dev->timer, gpio_tyrion_gpio_timer, (void *)dev);

	PARSE_GPIO_NUM_PROP(pdev->dev.of_node, "irdetect", dev->irdetect_gpio, 0);
	PARSE_AND_INIT_GPIO_IRQ(pdev->dev.of_node, "gpioirq", dev->irdetect_gpioirq, dev->irdetect_gpio, GPIO_IRQ_HIGH);
	PARSE_AND_INIT_GPIO_IRQ(pdev->dev.of_node, "removedetectirq", dev->jackremove_gpioirq, dev->irdetect_gpio, GPIO_IRQ_LOW);
	/*request gpio irq to kernel*/
	dev->irdetect_irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	ret = request_irq(dev->irdetect_irq, ir_detect_handler, IRQF_DISABLED, "irdetect", dev);
	if (ret < 0)
		goto err2;
	PARSE_GPIO_NUM_PROP(pdev->dev.of_node, "irfaultdetect", dev->irdetect_gpio, 0);
	PARSE_AND_INIT_GPIO_IRQ(pdev->dev.of_node, "gpiofaultdetectirq", dev->irdetect_gpioirq, dev->irdetect_gpio, GPIO_IRQ_LOW);
	dev->irfaultdetect_irq = irq_of_parse_and_map(pdev->dev.of_node, 1);
	ret = request_irq(dev->irfaultdetect_irq, ir_fault_detect_handler, IRQF_DISABLED, "irfaultdetect", dev);
	if (ret < 0){
		free_irq(dev->irdetect_irq, dev);
		goto err2;
	}
	dev->irremove_irq = irq_of_parse_and_map(pdev->dev.of_node, 2);
	ret = request_irq(dev->irremove_irq, ir_remove_handler, IRQF_DISABLED, "irremove", dev);
	if (ret < 0){
		free_irq(dev->irdetect_irq, dev);
		free_irq(dev->irfaultdetect_irq, dev);
		goto err2;
	}

	pr_info("irdetect is %d, plug value is %d\n", dev->irdetect_gpio, dev->plug_flag);
	snprintf(data, sizeof(data), "plug=%d", dev->plug_flag);
	pr_info("generate IR detect uevent %s\n", envp[0]);
	kobject_uevent_env(&dev->dev->kobj, KOBJ_CHANGE, envp);

	tx_dev = dev;
	handlerinit = 1;
	board_id =idme_get_board_long_rev();
	return 0;
err2:
	gpio_free(dev->ir_ext_enable_pin);
	gpio_free(dev->irdetect_gpio);

err:
	pr_info("blaster:blaster_probe error %d\n", ret);

	return ret;
}

static int aml_irblaster_remove(struct platform_device *pdev)
{
	struct irtx_dev *dev = platform_get_drvdata(pdev);
	if (dev->timer_debounce)
		del_timer_sync(&dev->timer);
	device_remove_file(irblaster_dev, &dev_attr_debug);
	device_remove_file(irblaster_dev, &dev_attr_log);
	device_remove_file(irblaster_dev, &dev_attr_sendvalue);
	device_remove_file(irblaster_dev, &dev_attr_send);
	device_remove_file(irblaster_dev, &dev_attr_carrier_freq);
	device_remove_file(irblaster_dev, &dev_attr_duty_cycle);
	device_remove_file(irblaster_dev, &dev_attr_plug);
	kfree(irblaster);
	free_irq(dev->irdetect_irq, dev);
	free_irq(dev->irfaultdetect_irq, dev);
	free_irq(dev->irremove_irq, dev);
	cdev_del(&amirblaster_device);
	device_destroy(irblaster_class, amirblaster_id);
	class_destroy(irblaster_class);
	unregister_chrdev_region(amirblaster_id, DEIVE_COUNT);
	return 0;
}
static const struct of_device_id irblaster_dt_match[] = {
	{
		.compatible	= "amlogic,am_irblaster",
	},
	{},
};
static struct platform_driver aml_irblaster_driver = {
	.probe		= aml_irblaster_probe,
	.remove		= aml_irblaster_remove,
	.suspend	= NULL,
	.resume		= NULL,
	.driver = {
		.name = "meson-irblaster",
		.owner  = THIS_MODULE,
		.of_match_table = irblaster_dt_match,
	},
};



static int __init aml_irblaster_init(void)
{
	pr_info("BLASTER Driver Init\n");
	if (platform_driver_register(&aml_irblaster_driver)) {
		irblaster_dbg("failed to register aml_ir_irblaster_driver module\n");
		return -ENODEV;
	}
	return 0;
}

static void __exit aml_irblaster_exit(void)
{
	pr_info("IRBLASTER Driver exit\n");
	platform_driver_unregister(&aml_irblaster_driver);
}
module_init(aml_irblaster_init);
module_exit(aml_irblaster_exit);

MODULE_AUTHOR("platform-beijing");
MODULE_DESCRIPTION("Irblaster Driver");
MODULE_LICENSE("GPL");
