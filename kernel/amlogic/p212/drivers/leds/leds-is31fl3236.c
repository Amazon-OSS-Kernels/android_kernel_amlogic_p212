/*
 * leds-is31fl3236.c
 *
 * Copyright (c) 2016 Amazon.com, Inc. or its affiliates. All Rights Reserved
 *
 * The code contained herein is licensed under the GNU General Public
 * License Version 2. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include "leds-is31fl3236.h"

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define REG_SW_SHUTDOWN 0x00
#define REG_PWM_BASE 0x01
#define REG_UPDATE 0x25
#define REG_CTRL_BASE 0x26
#define REG_G_CTRL 0x4A /* Global Control Register */
#define REG_RST 0x4F
#define REG_PWM_FREQ 0x4B

#define LED_SW_ON 0x01
#define LED_CTRL_UPDATE 0x0
#define LED_CHAN_DISABLED 0x0
#define LED_CHAN_ENABLED 0x01

#define LED_CURRENT_1   0x00
#define LED_CURRENT_1_2 0x01
#define LED_CURRENT_1_3 0x02
#define LED_CURRENT_1_4 0x03
#define LED_CURRENT_DEFAULT LED_CURRENT_1

#define BOOT_ANIMATION_FRAME_DELAY 88

#define PWM_FREQ_3K 0x00
#define PWM_FREQ_25K 0x01

#define FREQ_3K 3000
#define FREQ_25K 25000

struct aml_gpio {
	const char *name;
	unsigned int pin;
	unsigned int active_low;
	unsigned int state;
};

struct is31fl3236_data {
	struct mutex lock;
	bool play_boot_animation;
	bool setup_device;
	struct aml_gpio enable_gpio;
	struct aml_gpio buck_gpio;
	uint8_t ch_offset;
	struct task_struct *boot_anim_task;
	int enabled;
	uint8_t *state;
	uint8_t led_current;
	uint8_t pwm_freq;
	struct i2c_client *client;
	struct notifier_block reboot_notifier;
};


static struct is31fl3236_data issi_data;
static dev_t issi_devno;
static struct cdev *issi_cdev;

static struct device *issi_dev;


static struct class issi_class = {
	.name = "issi",
	.owner = THIS_MODULE,
};


static int is31fl3236_write_reg(struct i2c_client *client,
				uint32_t reg,
				uint8_t value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static int is31fl3236_update_led(struct i2c_client *client,
				 uint32_t led,
				 uint8_t value)
{
	return is31fl3236_write_reg(client, REG_PWM_BASE+led, value);
}

static int update_frame(struct is31fl3236_data *pdata,
						const uint8_t *buf)
{
	struct i2c_client *client = pdata->client;
	int ret;
	int i, j;
	uint8_t temp;

	mutex_lock(&pdata->lock);

	for (i = 8; i <= NUM_CHANNELS/3; i++) {
		for (j = 0; j < 3; j++) {
			/*update frame state*/
			pdata->state[i*3+j] = buf[i*3+j];
			/*reverse order of RGB leds*/
			temp = buf[(12-i)%12*3+j];
			ret = is31fl3236_update_led(client,
					(i*3+j+pdata->ch_offset)%NUM_CHANNELS,
					temp);
			if (ret != 0)
				goto fail;
		}
	}
	ret = is31fl3236_write_reg(client, REG_UPDATE, 0x0);

fail:
	mutex_unlock(&pdata->lock);
	return ret;
}

static int boot_anim_thread(void *data)
{
	struct is31fl3236_data *pdata = (struct is31fl3236_data *)data;
	int i = 0;

	while (!kthread_should_stop()) {
		update_frame(pdata, &frames[i][0]);
		msleep(BOOT_ANIMATION_FRAME_DELAY);
		i = (i + 1) % ARRAY_SIZE(frames);
	}
	update_frame(pdata, clear_frame);
	return 0;
}


static ssize_t boot_animation_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t len)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	uint8_t val;
	ssize_t ret;
	struct task_struct *stop_struct = NULL;

	ret = kstrtou8(buf, 10, &val);
	if (ret) {
		pr_err("ISSI: Invalid input to store_boot_anim\n");
		goto fail;
	}

	mutex_lock(&pdata->lock);
	if (!val) {
		if (pdata->boot_anim_task) {
			stop_struct = pdata->boot_anim_task;
			pdata->boot_anim_task = NULL;
		}
	} else {
		if (!pdata->boot_anim_task) {
			pdata->boot_anim_task = kthread_run(boot_anim_thread,
							(void *)pdata,
							"boot_animation_thread");
			if (IS_ERR(pdata->boot_anim_task))
				pr_err("ISSI: could not create boot animation thread\n");
		}
	}
	mutex_unlock(&pdata->lock);

	if (stop_struct)
		kthread_stop(stop_struct);

	ret = len;
fail:
	return ret;
}

static ssize_t boot_animation_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	int ret;

	mutex_lock(&pdata->lock);
	ret = sprintf(buf, "%d\n", pdata->boot_anim_task != NULL);
	mutex_unlock(&pdata->lock);

	return ret;
}
static DEVICE_ATTR_RW(boot_animation);


static ssize_t sw_shutdown_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t len)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	struct i2c_client *client = pdata->client;
	uint8_t val;
	ssize_t ret;

	ret = kstrtou8(buf, 10, &val);
	if (ret) {
		pr_err("ISSI: Invalid input to store_led_sw_shutdown\n");
		goto fail;
	}

	if (val < 0 || val > LED_SW_ON) {
		pr_err("ISSI: Invalid led sw_shutdown\n");
		ret = -EINVAL;
		goto fail;
	}

	mutex_lock(&pdata->lock);
	is31fl3236_write_reg(client, REG_SW_SHUTDOWN, val);
	mutex_unlock(&pdata->lock);
	ret = len;
fail:
	return ret;
}

static DEVICE_ATTR_WO(sw_shutdown);

static ssize_t pwm_freq_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t len)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	struct i2c_client *client = pdata->client;
	uint8_t val;
	ssize_t ret;

	ret = kstrtou8(buf, 10, &val);
	if (ret) {
		pr_err("ISSI: Invalid input to store_led_pwm_freq\n");
		goto fail;
	}

	if (val < PWM_FREQ_3K || val > PWM_FREQ_25K) {
		pr_err("ISSI: Invalid led pwm_freq\n");
		ret = -EINVAL;
		goto fail;
	}

	mutex_lock(&pdata->lock);
	pdata->pwm_freq = val;
	is31fl3236_write_reg(client, REG_PWM_FREQ, val);
	mutex_unlock(&pdata->lock);
	ret = len;
fail:
	return ret;
}

static ssize_t pwm_freq_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	int ret, freq;

	mutex_lock(&pdata->lock);
	if (pdata->pwm_freq)
		freq = FREQ_25K;
	else
		freq = FREQ_3K;
	ret = sprintf(buf, "%d freq is %d\n", pdata->pwm_freq, freq);
	mutex_unlock(&pdata->lock);

	return ret;
}
static DEVICE_ATTR_RW(pwm_freq);

static ssize_t led_current_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t len)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	struct i2c_client *client = pdata->client;
	uint8_t val;
	int i;
	ssize_t ret;

	ret = kstrtou8(buf, 10, &val);
	if (ret) {
		pr_err("ISSI: Invalid input to store_led_current\n");
		goto fail;
	}
	if (val < LED_CURRENT_1 || val > LED_CURRENT_1_4) {
		pr_err("ISSI: Invalid led current division\n");
		ret = -EINVAL;
		goto fail;
	}

	mutex_lock(&pdata->lock);
	pdata->led_current = val;
	val = (val << 1) | 0x1;
	for (i = 0; i < NUM_CHANNELS; i++)
		is31fl3236_write_reg(client, REG_CTRL_BASE + i, val);

	is31fl3236_write_reg(client, REG_UPDATE, 0x0);
	mutex_unlock(&pdata->lock);
	ret = len;
fail:
	return ret;
}

static ssize_t led_current_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	int ret;

	mutex_lock(&pdata->lock);
	ret = sprintf(buf, "%d\n", pdata->led_current);
	mutex_unlock(&pdata->lock);

	return ret;
}
static DEVICE_ATTR_RW(led_current);

static ssize_t frame_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	int len = 0;
	int i = 0;

	mutex_lock(&pdata->lock);
	for (i = 0; i < NUM_CHANNELS; i++) {
		len += sprintf(buf, "%s%02x",
				buf, pdata->state[i]);
	}
	mutex_unlock(&pdata->lock);
	return sprintf(buf, "%s\n", buf);
}

static ssize_t frame_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t len)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	uint8_t new_state[NUM_CHANNELS];
	char val[3];
	int count = 0;
	int ret, i;

	val[2] = '\0';
	for (i = 0; i < NUM_CHANNELS * 2; i += 2) {
		val[0] = buf[i];
		val[1] = buf[i + 1];
		ret = kstrtou8(val, 16, &new_state[count]);
		if (ret) {
			pr_err("ISSI: Invalid input for frame_store: %d\n",
			       i);
			return ret;
		}
		count++;
	}

	ret = update_frame(pdata, &new_state[0]);
	if (ret < 0) {
		pr_err("ISSI: could not update frame\n");
		return ret;
	}

	return len;
}

static DEVICE_ATTR_RW(frame);


void set_boot_animation(int boot_ani)
{
	struct is31fl3236_data *pdata = &issi_data;

	struct task_struct *stop_struct = NULL;



	mutex_lock(&pdata->lock);
	if (!boot_ani) {
		if (pdata->boot_anim_task) {
			stop_struct = pdata->boot_anim_task;
			pdata->boot_anim_task = NULL;
		}
	} else {
		if (!pdata->boot_anim_task) {
			pdata->boot_anim_task = kthread_run(boot_anim_thread,
							(void *)pdata,
							"boot_animation_thread");
			if (IS_ERR(pdata->boot_anim_task))
				pr_err("ISSI: could not create boot animation thread\n");
		}
	}
	mutex_unlock(&pdata->lock);

	if (stop_struct)
		kthread_stop(stop_struct);

}
int set_frame(char *buf)
{
	struct is31fl3236_data *pdata = &issi_data;
	uint8_t new_state[NUM_CHANNELS];
	char val[3];
	int count = 0;
	int ret, i;

	val[2] = '\0';
	for (i = 0; i < NUM_CHANNELS * 2; i += 2) {
		val[0] = buf[i];
		val[1] = buf[i + 1];
		ret = kstrtou8(val, 16, &new_state[count]);
		if (ret) {
			pr_err("ISSI: Invalid input for frame_store: %d\n",
			       i);
			return ret;
		}
		count++;
	}

	ret = update_frame(pdata, &new_state[0]);
	if (ret < 0) {
		pr_err("ISSI: could not update frame\n");
		return ret;
	}
	return 0;
}


static int issi_open(struct inode *inode, struct file *file)
{

	struct cdev *cdevp = inode->i_cdev;
	file->private_data = cdevp;
	return 0;
}

static int issi_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long issi_ioctl(struct file *filp, unsigned int cmd,
				unsigned long args)
{
	int boot_ani, i;
	char th[NUM_CHANNELS*2];
	void __user *argp = (void __user *)args;

	switch (cmd) {
	case SET_BOOT_ANIM:
		get_user(boot_ani, (int*)argp);
		set_boot_animation(boot_ani);
		break;
	case SET_FRAME:
		for(i = 0; i < NUM_CHANNELS*2; i++){
			get_user(th[i], (char*)argp++);
		}
		set_frame((char*)th);
		break;
	default:
		pr_info("issi ioctl: default\n");
		return -EINVAL;
	}
	return 0;
}


static const struct file_operations issi_fops = {
	.owner = THIS_MODULE,
	.open = issi_open,
	.compat_ioctl = issi_ioctl,
	.unlocked_ioctl = issi_ioctl,
	.release = issi_release,
};


static void is31fl3236_parse_dt(struct is31fl3236_data *pdata,
				struct device_node *node)
{
	struct gpio_desc *desc;
	enum of_gpio_flags flags;


	pdata->play_boot_animation = of_property_read_bool(node, "play-boot-animation");
	pdata->setup_device = of_property_read_bool(node, "setup-device");

	desc = of_get_named_gpiod_flags(node, "enable-gpio", 0, &flags);
	pdata->enable_gpio.pin = desc_to_gpio(desc);

	desc = of_get_named_gpiod_flags(node, "buck-gpio", 0, &flags);
	pdata->buck_gpio.pin = desc_to_gpio(desc);
	/*Read a channel offset, if one is available*/
	if (of_property_read_u8(node, "channel-offset", &pdata->ch_offset))
		pdata->ch_offset = 0;
	/*Offset should not exceed the number of channels available*/
	if (pdata->ch_offset >= NUM_CHANNELS) {
		pr_err("ISSI: Invalid channel offset=%d\n", pdata->ch_offset);
		pdata->ch_offset = 0;
	}
}

static int is31fl3236_power_on_device(struct i2c_client *client)
{
	struct is31fl3236_data *pdata = dev_get_platdata(&client->dev);
	int i;
	int ret = 0;

	if (pdata->setup_device) {
		gpio_request(pdata->enable_gpio.pin, "ISSI_ENABLE_GPIO");
		gpio_direction_output(pdata->enable_gpio.pin, 1);

		ret = is31fl3236_write_reg(client, REG_RST, 0x0);
		if (ret < 0) {
			pr_err("ISSI: Could not reset registers\n");
			goto fail;
		}

		ret = is31fl3236_write_reg(client, REG_SW_SHUTDOWN, LED_SW_ON);
		if (ret < 0) {
			pr_err("ISSI: Could not start device\n");
			goto fail;
		}

		ret = is31fl3236_write_reg(client, REG_PWM_FREQ, PWM_FREQ_25K);
		if (ret < 0) {
			pr_err("ISSI: Could not set PWM Freq to 25K\n");
			goto fail;
		}
	}
	pdata->enabled = LED_CHAN_ENABLED;
	pdata->pwm_freq = PWM_FREQ_25K;

	for (i = 0; i < NUM_CHANNELS; i++) {
		ret = is31fl3236_write_reg(client, REG_CTRL_BASE + i, 0x01);
		if (ret < 0) {
			pr_err("ISSI: Could not enabled led: %d\n", i);
			goto fail;
		}
	}
	ret = is31fl3236_write_reg(client, REG_UPDATE, 0x0);

	return ret;
fail:
	gpio_direction_output(pdata->enable_gpio.pin, 0);
	return ret;
}

static int is31fl3236_reboot_callback(struct notifier_block *self,
				      unsigned long val,
				      void *data)
{
	int ret;


	struct is31fl3236_data *pdata =
		container_of(self, struct is31fl3236_data, reboot_notifier);
	pr_err("ISSI: resetting device before reboot!\n");
	ret = is31fl3236_write_reg(pdata->client, REG_RST, 0x0);
	if (ret < 0) {
		pr_err("ISSI: Could not reset registers\n");
		return notifier_from_errno(-EIO);
	}
	return NOTIFY_DONE;
}

static int is31fl3236_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{

	int ret = 0;
	pr_info("in issi probe\n");
	
	ret = alloc_chrdev_region(&issi_devno, 0, 1, "issi");

	ret = class_register(&issi_class);
	issi_cdev = cdev_alloc();
	cdev_init(issi_cdev, &issi_fops);
	ret = cdev_add(issi_cdev, issi_devno, 1);

	issi_dev = device_create(&issi_class, NULL, issi_devno, NULL,"issi");

	struct is31fl3236_data *pdata = devm_kzalloc(&client->dev,
						sizeof(struct is31fl3236_data),
						GFP_KERNEL);
	if (!pdata) {
		pr_err("ISSI: Could not allocate memory for platform data\n");
		ret = -ENOMEM;
		goto fail;
	}

	pdata->client = client;
	pdata->state = devm_kzalloc(&client->dev,
				    sizeof(uint8_t) * NUM_CHANNELS,
								GFP_KERNEL);
	if (!pdata->state) {
		pr_err("ISSI: Could not allocate memory for state data\n");
		ret = -ENOMEM;
		devm_kfree(&client->dev, pdata);
		goto fail;
	}
	pdata->led_current = LED_CURRENT_DEFAULT;
	pdata->enabled = LED_CHAN_DISABLED;
	mutex_init(&pdata->lock);
	pdata->boot_anim_task = NULL;
	pdata->pwm_freq = 0;
	pdata->reboot_notifier.notifier_call = is31fl3236_reboot_callback;
	is31fl3236_parse_dt(pdata, client->dev.of_node);

	gpio_request(pdata->buck_gpio.pin, "ISSI_buck_gpio");
	gpio_direction_output(pdata->buck_gpio.pin, 1);

	client->dev.platform_data = pdata;
	ret = is31fl3236_power_on_device(client);

	if (ret < 0) {
		pr_err("ISSI: Could not power on device: %d\n", ret);
		goto fail;
	}

	ret = device_create_file(&client->dev, &dev_attr_frame);
	if (ret) {
		pr_err("ISSI: Could not create frame sysfs entry\n");
		goto fail;
	}
	ret = device_create_file(&client->dev, &dev_attr_led_current);
	if (ret) {
		pr_err("ISSI: Could not create brightness sysfs entry\n");
		goto fail;
	}

	ret = device_create_file(&client->dev, &dev_attr_boot_animation);
	if (ret) {
		pr_err("ISSI: Could not create boot animation entry\n");
		goto fail;
	}

	ret = device_create_file(&client->dev, &dev_attr_sw_shutdown);
	if (ret) {
		pr_err("ISSI: Could not create sw shutdown entry\n");
		goto fail;
	}

	ret = device_create_file(&client->dev, &dev_attr_pwm_freq);
	if (ret) {
		pr_err("ISSI: Could not create sw shutdown entry\n");
		goto fail;
	}

	ret = register_reboot_notifier(&pdata->reboot_notifier);
	if (ret) {
		pr_err("ISSI: Could not register reboot callback\n");
		goto fail;
	}

	if (pdata->play_boot_animation) {
		mutex_lock(&pdata->lock);
		pdata->boot_anim_task = kthread_run(boot_anim_thread,
						    (void *)pdata,
						    "boot_animation_thread");
		if (IS_ERR(pdata->boot_anim_task))
			pr_err("ISSI: could not start boot animation thread");
		mutex_unlock(&pdata->lock);
	}
	issi_data = *pdata;
fail:
	return ret;
}

static int is31fl3236_remove(struct i2c_client *client)
{
	struct is31fl3236_data *pdata = dev_get_platdata(&client->dev);
	int ret = 0;

	mutex_lock(&pdata->lock);
	if (pdata->boot_anim_task) {
		kthread_stop(pdata->boot_anim_task);
		pdata->boot_anim_task = NULL;
	}

	ret = is31fl3236_write_reg(client, REG_RST, 0x0);
	if (ret < 0) {
		pr_err("ISSI: Could not reset registers\n");
		goto fail;
	}

fail:
	gpio_direction_output(pdata->buck_gpio.pin, 0);
	gpio_direction_output(pdata->enable_gpio.pin, 0);
	mutex_unlock(&pdata->lock);
	return ret;
}

static struct i2c_device_id is31fl3236_i2c_match[] = {
	{"issi,is31fl3236", 0},
};
MODULE_DEVICE_TABLE(i2c, is31fl3236_i2c_match);

static struct of_device_id is31fl3236_of_match[] = {
	{
		.compatible	= "issi,is31fl3236",
	},
	{},
};
MODULE_DEVICE_TABLE(of, is31fl3236_of_match);

static struct i2c_driver is31fl3236_driver = {
	.driver = {
		.name = "is31fl3236",
		.of_match_table = is31fl3236_of_match,
	},
	.probe = is31fl3236_probe,
	.remove = is31fl3236_remove,
	.id_table = is31fl3236_i2c_match,
};

module_i2c_driver(is31fl3236_driver);

MODULE_AUTHOR("Amazon.com");
MODULE_DESCRIPTION("ISSI IS31FL3236 LED Driver");
MODULE_LICENSE("GPL");
