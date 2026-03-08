#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <asm/smp_plat.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/machine.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/pm_opp.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/cpu.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/workqueue.h>
#include <linux/earlysuspend.h>
#include <linux/cpufreq-boost.h>


extern int meson_cpufreq_enable_boost(bool enable);

struct boost_state {
	int request;
	struct delayed_work work;
};

struct cpufreq_boost {
	spinlock_t boost_lock;
	int last_req_mode;
	wait_queue_head_t wq;
	struct task_struct *thread;
	struct boost_state state[PRIO_DEFAULT];
	atomic_t event;
};

static struct cpufreq_boost cpuboost;

#define MAX_CORES_NUMBER nr_cpu_ids
#define MAX_FREQUENCY 1
#define MAX_DURATION 10000

static ssize_t cpufreq_boost_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t cpufreq_boost_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t n);
static struct device_attribute cpufreq_boost_attr = __ATTR(cpufreq_boost, (S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP),
	cpufreq_boost_show, cpufreq_boost_store);

static void cpufreq_boost_disable_work(struct work_struct *work)
{
	unsigned long flags;
	struct boost_state *state = container_of(work, struct boost_state, work.work);

	spin_lock_irqsave(&cpuboost.boost_lock, flags);
	if (state->request > 0)
		state->request--;
	spin_unlock_irqrestore(&cpuboost.boost_lock, flags);

	atomic_inc(&cpuboost.event);
	wake_up(&cpuboost.wq);
}

/*
 * Set up performance boost mode with requested duration
 * @duration: How long the user want this mode to keep. Specify with ms.
 * @mode: Request mode.
*/
int set_cpufreq_boost(int duration, int prio_mode)
{
	unsigned long flags;
	struct boost_state *state;

	if (duration > MAX_DURATION ||
	    prio_mode < 0 || prio_mode > PRIO_DEFAULT)
		return -EINVAL;

	if (prio_mode == PRIO_DEFAULT || !duration)
		return 0;

	spin_lock_irqsave(&cpuboost.boost_lock, flags);

	state = &cpuboost.state[prio_mode];
	if (duration == ON)
		state->request++;
	else if (duration == OFF)
		state->request--;
	else if (!mod_delayed_work(system_wq, &state->work, msecs_to_jiffies(duration)))
		state->request++;
	spin_unlock_irqrestore(&cpuboost.boost_lock, flags);

	atomic_inc(&cpuboost.event);
	wake_up(&cpuboost.wq);
	return 0;
}
EXPORT_SYMBOL(set_cpufreq_boost);

static int cpufreq_boost_dvfs_hotplug_thread(void *ptr)
{
#if 0
#ifndef CONFIG_ARCH_MT8173  /* for 8135 */
	char cmd[128] = "";
	int l = 0;
#endif
#endif
	int max_freq, cores_to_set_b, cores_to_set_l, cores_to_set_sum;
	unsigned long flags;

	set_user_nice(current, -10);

	while (!kthread_should_stop()) {
		int i, set_mode = PRIO_DEFAULT;

		spin_lock_irqsave(&cpuboost.boost_lock, flags);
		for (i = PRIO_DEFAULT - 1; i >= 0; i--) {
			if (cpuboost.state[i].request) {
				set_mode = i;
				break;
			}
		}
		spin_unlock_irqrestore(&cpuboost.boost_lock, flags);

		/* there is no big core in mt8127 */
		cores_to_set_b = 0;
		switch (set_mode) {
		case PRIO_MAX_CORES_MAX_FREQ:
			cores_to_set_l = num_possible_cpus();
			max_freq = MAX_FREQUENCY;
			break;
		case PRIO_MAX_CORES:
			cores_to_set_l = num_possible_cpus();
			max_freq = 0;
			break;
		case PRIO_RESET:
			for (i = PRIO_DEFAULT - 1; i >= 0; i--)
				cpuboost.state[i].request = 0;
		default:
			cores_to_set_l = 1;
			max_freq = 0;
			break;
		}

		if (max_freq)
			meson_cpufreq_enable_boost(true);
		else
			meson_cpufreq_enable_boost(false);

		printk("cpufreq_boost: Mode=%d cpu_min_num_sum=%d cpu_min_num_little=%d\n",
					set_mode, cores_to_set_sum, cores_to_set_l);

		cpuboost.last_req_mode = set_mode;

		while (!atomic_read(&cpuboost.event))
			wait_event(cpuboost.wq, atomic_read(&cpuboost.event));

		atomic_dec(&cpuboost.event);
	}
	return 0;
}

static ssize_t cpufreq_boost_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	i = snprintf(buf, PAGE_SIZE, "Mode: %d\n", cpuboost.last_req_mode);
	return i;
}

static ssize_t cpufreq_boost_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	int now_req_duration = 0;
	int now_req_mode = 0;

	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d %d", &now_req_duration, &now_req_mode) != 2)
		return -EINVAL;
	if (now_req_mode < 0)
		return -EINVAL;

	set_cpufreq_boost(now_req_duration, now_req_mode);

	return n;
}

static int cpufreq_boost_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	ret_device_file = device_create_file(&(dev->dev), &cpufreq_boost_attr);

	return ret_device_file;
}

struct platform_device cpufreq_boost_device = {
	.name   = "cpufreq_boost",
	.id        = -1,
};

int cpufreq_boost_suspend(struct device *dev)
{
	unsigned long flags;
	int i;
	/* cancel all boost jobs if system suspend is requested */
	for (i = 0; i < ARRAY_SIZE(cpuboost.state); ++i) {
		cancel_delayed_work_sync(&cpuboost.state[i].work);
		spin_lock_irqsave(&cpuboost.boost_lock, flags);
		cpuboost.state[i].request = 0;
		spin_unlock_irqrestore(&cpuboost.boost_lock, flags);
	}
	atomic_inc(&cpuboost.event);
	wake_up(&cpuboost.wq);
	return 0;
}

int cpufreq_boost_resume(struct device *dev)
{
	return 0;
}

static struct platform_driver cpufreq_boost_driver = {
	.probe      = cpufreq_boost_probe,
	.driver     = {
		.name = "cpufreq_boost",
		.pm = &(const struct dev_pm_ops){
			.suspend = cpufreq_boost_suspend,
			.resume = cpufreq_boost_resume,
		},
	},
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cpufreq_boost_early_suspend(struct early_suspend *h)
{
	unsigned long flags;

	spin_lock_irqsave(&cpuboost.boost_lock, flags);
	cpuboost.state[PRIO_RESET].request = 1;
	spin_unlock_irqrestore(&cpuboost.boost_lock, flags);

	atomic_inc(&cpuboost.event);
	wake_up(&cpuboost.wq);
	return;
}

static void cpufreq_boost_late_resume(struct early_suspend *h)
{
	unsigned long flags;

	/* make 2 A7 run all the time */
	spin_lock_irqsave(&cpuboost.boost_lock, flags);
	cpuboost.state[PRIO_RESET].request = 0;
	cpuboost.state[PRIO_MAX_CORES].request += 1;
	spin_unlock_irqrestore(&cpuboost.boost_lock, flags);

	atomic_inc(&cpuboost.event);
	wake_up(&cpuboost.wq);
	return;
}

static struct early_suspend cpufreq_boost_early_suspend_handler =
{
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 250,
	.suspend = cpufreq_boost_early_suspend,
	.resume  = cpufreq_boost_late_resume,
};
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND

static int __init cpufreq_boost_init(void)
{
	int ret = 0, i;
	ret = platform_device_register(&cpufreq_boost_device);
	if (ret)
		return ret;
	ret = platform_driver_register(&cpufreq_boost_driver);
	if (ret)
		return ret;

	spin_lock_init(&cpuboost.boost_lock);
	cpuboost.last_req_mode = PRIO_DEFAULT;
	atomic_set(&cpuboost.event, 0);

	for (i = 0; i < ARRAY_SIZE(cpuboost.state); ++i) {
		INIT_DELAYED_WORK(&cpuboost.state[i].work, cpufreq_boost_disable_work);
		if (i == PRIO_MAX_CORES)
			cpuboost.state[i].request = 1;
		else
			cpuboost.state[i].request = 0;
	}
	init_waitqueue_head(&cpuboost.wq);

	cpuboost.thread = kthread_run(cpufreq_boost_dvfs_hotplug_thread, &cpuboost, "cpufreq_boost");
	if (IS_ERR(cpuboost.thread))
		return -EINVAL;

#ifdef CONFIG_HAS_EARLYSUSPEND
    register_early_suspend(&cpufreq_boost_early_suspend_handler);
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND

	return 0;
}
late_initcall(cpufreq_boost_init);

static void __exit cpufreq_boost_exit(void)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&cpufreq_boost_early_suspend_handler);
#endif

	kthread_stop(cpuboost.thread);
}

module_exit(cpufreq_boost_exit);

MODULE_AUTHOR("Amazon Lab126");
MODULE_DESCRIPTION("performance boost driver");
MODULE_LICENSE("GPL");


