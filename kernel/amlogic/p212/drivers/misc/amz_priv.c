/*
 * amz_priv.c
 *
 * Copyright 2017 Amazon Technologies, Inc. All Rights Reserved.
 *
 * The code contained herein is licensed under the GNU General Public
 * License Version 2. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <amz_priv.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#define AMZ_PRIVACY_OF_NODE "amz_privacy"
#define DRIVER_NAME "amz_privacy"
static int board_id;

struct priv_work_data *pw_data;

#define GET_PW_DATA(w) (container_of((w), struct priv_work_data,	\
					dwork))

struct workqueue_struct *__attribute__((weak)) amz_priv_get_workq(void)
{
	return NULL;
}

extern int idme_get_board_long_rev(void);

int ishvt(int boardid){
	return boardid==STARK_HVT_BOARDID?1:0;
}

int amz_priv_trigger(int on)
{

	if (!pw_data) {
		pr_err("%s:%u null error\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* pull/push the plug priv gpio, EVT device is inverted */
	on &= 0x1;

	if (PRIV_GPIO_USR_CNTL != pw_data->priv_gpio && ishvt(board_id)) {
		gpio_set_value(pw_data->priv_gpio, on);
	}

	pr_debug("%s:%u [%d]\n", __func__, __LINE__, on);

	/*
	 * delay setting state until hw st asserted
	 * applicable only in hw latch implementation
	 */

	pw_data->cur_priv = on;
	sysfs_notify(pw_data->kobj, NULL, "privacy_state");

	pr_debug("%s:%u done [%d]\n", __func__, __LINE__, on);
	return 0;
}
EXPORT_SYMBOL(amz_priv_trigger);

static int privacy_mode_status;
int amz_priv_timer_sysfs(int on)
{
	if (!pw_data) {
		pr_err("%s:%u null error\n", __func__, __LINE__);
		return -EINVAL;
	}
	pw_data->cur_timer_on = on;

	/*set to an invalid value so we can tell if it gets cancelled*/
	if (on)
		privacy_mode_status = -1;

	sysfs_notify(pw_data->kobj, NULL, "privacy_timer_on");
	return 0;
}
EXPORT_SYMBOL(amz_priv_timer_sysfs);

void start_privacy_timer_func(struct work_struct *work)
{
	struct priv_work_data *pw = GET_PW_DATA(to_delayed_work(work));

	if (!pw) {
		pr_err("%s: %u: pw data is null\n", __func__, __LINE__);
		return;
	}
	amz_priv_trigger(1);
	amz_priv_timer_sysfs(0);
	return;
}
EXPORT_SYMBOL(start_privacy_timer_func);

static ssize_t get_privacy_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		       pw_data->cur_priv);
	return ret;
}

static ssize_t get_privacy_timer_on(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		       pw_data->cur_timer_on);
	return ret;
}

static ssize_t get_privacy_trigger(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	ret = snprintf(buf, PAGE_SIZE, "%d\n", privacy_mode_status);
	return ret;
}

static ssize_t set_privacy_trigger(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	if (kstrtoint(buf, 10, &privacy_mode_status)) {
		pr_err("%s: kstrtoint failed\n", __func__);
		return -EPERM;
	}
	pr_debug("%s: %u: Privacy Mode set to [%d]\n", __func__,
			__LINE__, privacy_mode_status);
	if (privacy_mode_status == pw_data->cur_priv) {
		pr_debug("%s: not entering the same state\n", __func__);
		FLUSH_PRIV_WORKQ();
		return ret;
	}

	/* user space has triggered privacy:
	 * ignore exit privacy mode from software
	 */
	if ((privacy_mode_status == 1) && (!pw_data->cur_timer_on)) {
		pr_debug("%s: user space trigd priv\n", __func__);
		amz_priv_trigger(1);
		/* EVT privacy design need to pull low temporarily then pull high */
		if (!ishvt(board_id)){
			gpio_set_value(pw_data->priv_gpio, 0);
			udelay(200);
			gpio_set_value(pw_data->priv_gpio, 1);
		}
		FLUSH_PRIV_WORKQ();
		return ret;
	}

	/* provide a mechanism for software to cancel queued
	   work items to debounce */
	if (privacy_mode_status == 2) {
		FLUSH_PRIV_WORKQ();
	}

	return ret;
}

static DEVICE_ATTR(privacy_trigger, S_IWUSR | S_IWGRP | S_IRUGO,
		   get_privacy_trigger,
		   set_privacy_trigger);
static DEVICE_ATTR(privacy_state, S_IRUGO,
		   get_privacy_state,
		   NULL);
static DEVICE_ATTR(privacy_timer_on, S_IRUGO,
		   get_privacy_timer_on,
		   NULL);

static struct attribute *gpio_keys_attrs[] = {
	&dev_attr_privacy_trigger.attr,
	&dev_attr_privacy_state.attr,
	&dev_attr_privacy_timer_on.attr,
	NULL,
};

static struct attribute_group gpio_keys_attr_group = {
	.attrs = gpio_keys_attrs,
};

int amz_set_public_hw_pin(int gpio)
{
	int ret;

	if (gpio <= 0 || !pw_data) {
		pr_err("%s:%u: error %d\n", __func__, __LINE__, gpio);
		return -EINVAL;
	}

	pw_data->public_hw_st_gpio = gpio;
	ret = gpio_request(pw_data->public_hw_st_gpio, "amz_public_hw_st");
	if (ret) {
		pr_err("%s:%u gpio req failed ret=%d\n",
				__func__, __LINE__, ret);
		pw_data->public_hw_st_gpio = -EINVAL;
	} else {
		pw_data->cur_priv = !gpio_get_value(pw_data->public_hw_st_gpio);
	}
	return 0;
}
EXPORT_SYMBOL(amz_set_public_hw_pin);

#ifdef CONFIG_OF
int amz_priv_kickoff(struct device *dev)
{
	int ret;
	struct device_node *node = NULL;

	pr_err("dbg000 %s: begin\n", __func__);
	if (!pw_data) {
		pr_err("%s: pw_data is null\n", __func__);
		return -EINVAL;
	}

	node = of_find_node_by_name(NULL, AMZ_PRIVACY_OF_NODE);
	if (NULL == node) {
		pr_err("dbg000%s: could not find privacy device tree node\n",
		       __func__);
		pw_data->priv_gpio = PRIV_GPIO_USR_CNTL;
	} else {
		board_id =idme_get_board_long_rev();
		pr_info("in priv driver, board id is %d", board_id);
		if (ishvt(board_id))
			pw_data->priv_gpio = of_get_named_gpio_flags(node, "gpioshvt",  0 ,  NULL);
		else
			pw_data->priv_gpio = of_get_named_gpio_flags(node, "gpios",  0 ,  NULL);
		if (!gpio_is_valid(pw_data->priv_gpio)) {
			pr_err("dbg000%s: could not find privacy gpio: %d\n",
			       __func__,
			       pw_data->priv_gpio);
			pw_data->priv_gpio = PRIV_GPIO_USR_CNTL;
		}
	}
	pw_data->kobj = &dev->kobj;

	if (ishvt(board_id))
		ret = gpio_request_one(pw_data->priv_gpio,
					GPIOF_DIR_OUT | GPIOF_INIT_LOW, "amz_priv_trig");
	else
		ret = gpio_request_one(pw_data->priv_gpio,
					GPIOF_DIR_OUT | GPIOF_INIT_HIGH, "amz_priv_trig");
	if (ret)
		pr_err("dbg000%s:%u gpio req failed ret=%d\n",
				__func__, __LINE__, ret);

	if (ishvt(board_id))
		gpio_set_value(pw_data->priv_gpio, 0);
	else
		gpio_set_value(pw_data->priv_gpio, 1);

	//gpio_direction_output(pw_data->priv_gpio, GPIOF_OUT_INIT_LOW);

	ret = sysfs_create_group(&dev->kobj, &gpio_keys_attr_group);
	if (ret) {
		pr_err("dbg000%s:%u:Unable to export priv node, err: %d\n",
					__func__, __LINE__, ret);
		return -EINVAL;
	}

	pr_err("dbg000%s:%u:success!\n", __func__, __LINE__);
	return 0;
}
#else
int amz_priv_kickoff(int gpio, struct device *dev)
{
	int ret;

	if (gpio <= 0 && PRIV_GPIO_USR_CNTL != gpio) {
		pr_err("%s: invalid gpio %d\n", __func__, gpio);
		return -EINVAL;
	}

	if (!pw_data) {
		pr_err("%s: pw_data is null\n", __func__);
		return -EINVAL;
	}
	pw_data->priv_gpio = gpio;
	pw_data->kobj = &dev->kobj;

	if (PRIV_GPIO_USR_CNTL != gpio) {
		ret = gpio_request(pw_data->priv_gpio, "amz_priv_trig");
		if (ret)
			pr_err("%s:%u gpio req failed ret=%d\n",
					__func__, __LINE__, ret);
	}

	ret = sysfs_create_group(&dev->kobj, &gpio_keys_attr_group);
	if (ret) {
		pr_err("%s:%u:Unable to export priv node, err: %d\n",
					__func__, __LINE__, ret);
		return -EINVAL;
	}

	pr_debug("%s:%u:success!\n", __func__, __LINE__);
	return 0;
}
#endif
EXPORT_SYMBOL(amz_priv_kickoff);

static int __init amz_priv_init(void)
{
	pw_data = kzalloc(sizeof(struct priv_work_data), GFP_KERNEL);
	if (!pw_data) {
		pr_err("%s: out of mem\n", __func__);
		return -EINVAL;
	}

	return 0;
}
early_initcall(amz_priv_init);
