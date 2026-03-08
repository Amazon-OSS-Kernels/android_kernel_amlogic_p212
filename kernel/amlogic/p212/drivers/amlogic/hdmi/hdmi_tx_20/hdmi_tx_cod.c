/*
 * Current overdraw detection logic
 */

#include <linux/amlogic/saradc.h>
#include <linux/delay.h>
#include <linux/switch.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_module.h>

#define WORK_TO_HDMI_DEV(work, name) \
	container_of((struct delayed_work *)work, struct hdmitx_dev, name)

#define COD_RATE 2*HZ

static int cnt, det, state;

static struct switch_dev sd = {
        .name = "hdmi_cod",
};

static int get_ma(unsigned int ch)
{
	// Cap ADC result to protect against garbage values
	int ma = min(get_adc_sample(0, ch), 1000);

	// Approximate mA to ADC ratio is 1 : ~5.5
	return ma * 10 / 55;
}

static void work_cod_handler(struct work_struct *work)
{
	struct hdmitx_dev * hdev = WORK_TO_HDMI_DEV(work, work_cod);
	int ma = get_ma(hdev->cod_ch);

	// Spec allows up to 50mA to be drawn by the sink
	if (state ? (ma < 50) : (ma >= 50)) {
		det++;
	}

	if (++cnt < 10) {
		if (det >= 5) {
			state = !state;
			pr_warn("current overdraw detection: %d\n", state);
			switch_set_state(&sd, state ? ma : 0);
			cnt = det = 0;
		}
	} else {
		cnt = det = 0;
	}

	queue_delayed_work(hdev->hdmi_wq, &hdev->work_cod, COD_RATE);
}

void cod_init(struct hdmitx_dev *hdev)
{
	if (hdev->cod_ch == ~0x0)
		return;

	switch_dev_register(&sd);
	INIT_DELAYED_WORK(&hdev->work_cod, work_cod_handler);
}

void cod_test(struct hdmitx_dev *hdev)
{
	if (hdev->cod_ch == ~0x0)
		return;

	cancel_delayed_work_sync(&hdev->work_cod);
	switch_set_state(&sd, 0);
	cnt = det = state = 0;
	queue_delayed_work(hdev->hdmi_wq, &hdev->work_cod, COD_RATE);
}

void cod_fini(struct hdmitx_dev *hdev)
{
	if (hdev->cod_ch == ~0x0)
		return;

	cancel_delayed_work_sync(&hdev->work_cod);
	switch_dev_unregister(&sd);
}

