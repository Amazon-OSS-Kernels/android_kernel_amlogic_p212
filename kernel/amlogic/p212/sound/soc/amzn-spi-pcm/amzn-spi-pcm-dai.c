/*
 * ALSA PCM platform driver to capture audio through SPI Interface
 *
 * Author: Amazon Lab126 2016
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/spi/spi.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/sched/rt.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include <linux/thread_info.h>
#include "amzn-spi-pcm.h"
#include "AudDrv_Gpio.h"

/* Debugging Purpose:: Enable Dummy codec to test with ADC scripts
 * #define SPI_DUMMY_CODEC
*/
#define AMZN_SPI_PCM_DAI (AMZN_SPI_PCM "-dai")

static int amzn_spi_dai_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	pr_info("%s = %x\n", __func__, fmt);
	return 0;
}

static const struct snd_soc_dai_ops amzn_spi_dai_dai_ops = {
	.startup = NULL,
	.trigger = NULL,
	.set_fmt = amzn_spi_dai_dai_fmt,
	.hw_params = NULL,
};

static struct snd_soc_dai_driver amzn_spi_dai_cpu_dai[] = {
	{
		.name = AMZN_SPI_PCM_DAI,
		.capture = {
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S24_3LE,
			.channels_min = SPI_N_CHANNELS,
			.channels_max = SPI_N_CHANNELS,
		},
		.ops = NULL,
	},
};

#ifdef SPI_DUMMY_CODEC
static struct snd_soc_dai_driver amzn_spi_dai_codec_dummy_dai[] = {
	{
		.name = "amzn-spi-codec-dummy-dai",
		.capture = {
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S24_3LE,
			.channels_min = SPI_N_CHANNELS,
			.channels_max = SPI_N_CHANNELS,

		},
		.ops = NULL,
	},
};

static int dummy_codec_probe(struct snd_soc_codec *codec)
{
	pr_warn("%s()\n", __func__);
	return 0;
}

static int dummy_codec_remove(struct snd_soc_codec *codec)
{
	pr_warn("%s()\n", __func__);
	return 0;
}

static struct snd_soc_codec_driver soc_dummy_codec_drv = {
	.probe = dummy_codec_probe,
	.remove = dummy_codec_remove,
};

static int dummy_codec_plat_probe(struct platform_device *pdev)
{
	pr_warn("%s: name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_codec(&pdev->dev, &soc_dummy_codec_drv,
		amzn_spi_dai_codec_dummy_dai,
		ARRAY_SIZE(amzn_spi_dai_codec_dummy_dai));
}

static int dummy_codec_plat_remove(struct platform_device *pdev)
{
	pr_warn("%s: name %s\n", __func__, dev_name(&pdev->dev));
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver codec_dummy_driver = {
	.driver = {

		.name = "amzn-spi-codec-dummy-dai",
		.owner = THIS_MODULE,
	},
	.probe = dummy_codec_plat_probe,
	.remove = dummy_codec_plat_remove,
};

static const struct snd_soc_component_driver
	amzn_spi_dai_codec_dummy_dai_component = {
	.name = "amzn-spi-codec-dummy-dai",
};
#endif

static const struct snd_soc_component_driver amzn_spi_dai_cpu_dai_component = {
	.name = "amzn-spi-cpu-dai",
};

static int amzn_spi_dai_probe(struct platform_device *pdev)
{
	int rc;

	pr_info("%s\n", __func__);

	/*
	 * Register CPU DAI and component
	 * Note: cpu_dai_name to be used in the machine driver will be the
	 * device name (amzn-spi-pcm), not the component name since there
	 * is only 1 DAI
	 */
	dev_set_name(&pdev->dev, "%s", AMZN_SPI_PCM_DAI);
	rc = snd_soc_register_component(&pdev->dev,
			&amzn_spi_dai_cpu_dai_component, amzn_spi_dai_cpu_dai,
			ARRAY_SIZE(amzn_spi_dai_cpu_dai));
	if (rc < 0) {
		pr_err("%s: cpu dai component registration error\n", __func__);
	}

#ifdef SPI_DUMMY_CODEC
	/*
	 * Register the Codec dummy DAI
	 */
	codec_pdev = platform_device_alloc("amzn-spi-codec-dummy-dai", -1);
	if (!codec_pdev) {
		pr_err("%s: couldn't alloc codec_pdev\n", __func__);
		rc = -ENOMEM;
	}

	rc = platform_device_add(codec_pdev);
	if (rc) {
		platform_device_put(codec_pdev);
		pr_err("%s: platform_device_add of codec_pdev failed\n",
			__func__);
	}

	rc = platform_driver_register(&codec_dummy_driver);
	if (rc) {
		pr_err("%s: Failed to register dummy codec driver\n", __func__);
	}
#endif

	return rc;
}


static const struct of_device_id amzn_spi_dai_dt_ids[] = {
	{ .compatible = "amzn, spi-audio-dai" },
	{},
};

MODULE_DEVICE_TABLE(of, amzn_spi_dai_dt_ids);

static struct platform_driver amzn_spi_dai_driver = {
	.driver = {
		.name  = "amzn_spi_dai_drv",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(amzn_spi_dai_dt_ids),
		.bus = &spi_bus_type,
	},
	.probe = amzn_spi_dai_probe,
};

static int __init spi_dai_init(void)
{
	int rc;

	rc = platform_driver_register(&amzn_spi_dai_driver);
	if (rc < 0) {
		pr_err("%s: Failed to reg spi dai driver\n", __func__);
		return rc;
	}
	pr_info("%s: ret %d\n", __func__, rc);

	return rc;
}

module_init(spi_dai_init);

static void __exit spi_dai_exit(void)
{
	platform_driver_unregister(&amzn_spi_dai_driver);
}

module_exit(spi_dai_exit);

MODULE_AUTHOR("Amazon Lab126 Inc.");
MODULE_DESCRIPTION("Amazon Lab126 SPI Audio Driver");
MODULE_LICENSE("GPL v2");
