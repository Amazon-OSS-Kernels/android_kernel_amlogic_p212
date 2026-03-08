/*
 * Copyright (c) 2017, Amlogic, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/kthread.h>

#include "tee_private.h"

//#define OPTEE_LOG_BUFFER_DEBUG      1

#define OPTEE_LOG_BUFFER_MAGIC      0xAA00AA00
#define OPTEE_LOG_BUFFER_SIZE       0x00100000
#define OPTEE_LOG_BUFFER_OFFSET     0x00001000
#define OPTEE_LOG_READ_MAX          PAGE_SIZE
#define OPTEE_LOG_LINE_MAX          1024
#define OPTEE_LOG_TIMER_INTERVAL    1

#undef pr_fmt
#define pr_fmt(fmt) "[TEE] " fmt

struct optee_log_ctl_s {
	unsigned int magic;
	unsigned int inited;
	unsigned int total_size;
	unsigned int fill_size;
	unsigned int mode;
	unsigned int reader;
	unsigned int writer;

	unsigned char *buffer;
};

static struct optee_log_ctl_s *optee_log_ctl = NULL;
static unsigned char *optee_log_buff;
static uint32_t optee_log_mode = 1;
static struct timer_list optee_log_timer;
static uint8_t line_buff[OPTEE_LOG_LINE_MAX];

static ssize_t log_mode_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", optee_log_mode);
}

static ssize_t log_mode_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int res = 0;
	int rc = 0;
	struct optee_log_ctl_s *ctl = optee_log_ctl;

	rc = kstrtoint(buf, 0, &res);
	pr_notice("log_mode: %d->%d\n", optee_log_mode, res);
	optee_log_mode = res;
	if (ctl && ctl->mode != optee_log_mode)
		ctl->mode = optee_log_mode;

	return count;
}

static ssize_t log_buff_get_read_buff(char **buf, int len)
{
	int writer;
	int reader;
	int read_size = 0;
	struct optee_log_ctl_s *ctl = optee_log_ctl;

	if ((!ctl) || (len <= 0))
		return 0;

	writer = ctl->writer;
	reader = ctl->reader;

	if (reader == writer) {
		read_size = 0;
	} else if (reader < writer) {
		read_size = writer - reader;
	} else {
		read_size = ctl->total_size - reader;
	}

	if (read_size > len)
		read_size = len;

	*buf = optee_log_buff + reader;
	ctl->reader += read_size;
	if (ctl->reader == ctl->total_size)
		ctl->reader = 0;

	return read_size;
}

static size_t log_print_text(char *buf, size_t size)
{
	const char *text = buf;
	size_t text_size = size;
	size_t len = 0;
	char *line = line_buff;

	do {
		const char *next = memchr(text, '\n', text_size);
		size_t line_size;

		if (next) {
			line_size = next - text;
			next++;
			text_size -= next - text;
		} else {
			line_size = text_size;
		}

		memcpy(line, text, line_size);
		len += line_size;
		if (next)
			len++;
		else if (line[line_size] == '\0')
			len++;
		line[line_size] = '\n';
		line[line_size + 1] = '\0';
		pr_notice("%s", line);
		text = next;
	} while (text && (len < size));

	return len;
}

static ssize_t log_buff_dump(char *buf, size_t size)
{
	ssize_t len;
	char *ptr = NULL;
	struct optee_log_ctl_s *ctl = optee_log_ctl;

	if (!ctl)
		return 0;

	len = ctl->reader;
	if (len == 0)
		return 0;

	ptr = optee_log_buff;
	if (len > size) {
		ptr += len - size;
		len = size;
	}
	memcpy(buf, ptr, len);

	return len;
}

static void log_buff_reset(void)
{
	struct optee_log_ctl_s *ctl = optee_log_ctl;

	if (!ctl)
		return;

	ctl->writer = 0;
	ctl->reader = 0;

	return;
}

static void log_buff_info(void)
{
#ifdef OPTEE_LOG_BUFFER_DEBUG
	struct optee_log_ctl_s *ctl = optee_log_ctl;

	if (!ctl)
		return;

	pr_notice("    total_size: %d\n", ctl->total_size);
	pr_notice("     fill_size: %d\n", ctl->fill_size);
	pr_notice("          mode: %d\n", ctl->mode);
	pr_notice("        reader: %d\n", ctl->reader);
	pr_notice("        writer: %d\n", ctl->writer);
#endif
}

static ssize_t log_buff_store(struct class *cla, struct class_attribute *attr,
		const char *buf, size_t count)
{
	int res = 0;
	int rc = 0;

	rc = kstrtoint(buf, 0, &res);
	if (res == 0) {
		/* reset log buffer */
		log_buff_reset();
	} else if ( res == 1) {
		/* show log buffer info */
		log_buff_info();
	} else {
		pr_notice("set 0 to reset tee log buffer\n");
	}

	return count;
}

static ssize_t log_buff_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return log_buff_dump(buf, OPTEE_LOG_READ_MAX);
}

static struct class_attribute log_class_attrs[] = {
	__ATTR(log_mode, S_IRUGO | S_IWUSR, log_mode_show, log_mode_store),
	__ATTR(log_buff, S_IRUGO | S_IWUSR, log_buff_show, log_buff_store),
};

static void log_buff_output(void)
{
	size_t len;
	char *read_buff = NULL;

	if (optee_log_mode == 0)
		return;

	len = log_buff_get_read_buff(&read_buff, OPTEE_LOG_READ_MAX);
	if (len > 0) {
		log_print_text(read_buff, len);
	}
}

static void log_timer_func(unsigned long arg)
{
	log_buff_output();
	mod_timer(&optee_log_timer, jiffies + OPTEE_LOG_TIMER_INTERVAL * HZ);
}

int optee_log_init(struct class *cla, phys_addr_t shm_pa)
{
	int rc = 0;
	int i = 0;
	int n = 0;
	void __iomem *shm_va;

	/* remap tee log shared memory */
	shm_va = ioremap_cache(shm_pa, OPTEE_LOG_BUFFER_SIZE);
	if (!shm_va) {
		pr_err("tee log buffer ioremap failed\n");
		rc = -1;
		goto err;
	}

	optee_log_buff = (unsigned char *)(shm_va + OPTEE_LOG_BUFFER_OFFSET);
	optee_log_ctl = (struct optee_log_ctl_s *)shm_va;
	if ((optee_log_ctl->magic != OPTEE_LOG_BUFFER_MAGIC)
		|| (optee_log_ctl->inited != 1)) {
		pr_err("tee log buffer inited\n");
		optee_log_ctl = NULL;
		rc = -1;
		goto err;
	}
	optee_log_ctl->mode = optee_log_mode;

	/* create attr files */
	n = sizeof(log_class_attrs) / sizeof(struct class_attribute);
	for (i = 0; i < n; i++) {
		rc = class_create_file(cla, &log_class_attrs[i]);
		if (rc)
			goto err;
	}

	/* init timer */
	init_timer(&optee_log_timer);
	optee_log_timer.data = 0L;
	optee_log_timer.function = log_timer_func;
	optee_log_timer.expires = jiffies + HZ;
	add_timer(&optee_log_timer);

err:
	return rc;
}

void optee_log_exit(struct class *cla)
{
	int i = 0;
	int n = 0;

	del_timer_sync(&optee_log_timer);

	n = sizeof(log_class_attrs) / sizeof(struct class_attribute);
	for (i = 0; i < n; i++)
		class_remove_file(cla, &log_class_attrs[i]);
}
