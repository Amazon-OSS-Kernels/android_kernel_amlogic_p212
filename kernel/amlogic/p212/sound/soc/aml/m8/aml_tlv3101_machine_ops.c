
/*
 * sound/soc/aml/m8/aml_tlv3101_machine_ops.c
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
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "../../codecs/tlv320aic3101.h"
#include "aml_audio_hw.h"

#define RX_MASK 0xff /* ff for 8 channels */

#define LINEIN_ADC_DIS 0
static int linein_adc_enab = LINEIN_ADC_DIS;

static int tlv3101_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *codec_dai;

	pr_info("aic:%s\n", __func__);
	if (substream == NULL) {
		pr_err("invalid stream parameter\n");
		return -EINVAL;
	}

	rtd = substream->private_data;
	if (rtd == NULL) {
		pr_err("invalid runtime parameter\n");
		return -EINVAL;
	}

	codec_dai = rtd->codec_dai;
	if (codec_dai == NULL) {
		pr_err("invalid dai parameter\n");
		return -EINVAL;
	}

	snd_soc_dai_set_pll(codec_dai, AIC3101_PLL_BCLK,
			AIC3101_PLL_ADC_FS_CLKIN_MCLK,
			DEFAULT_MCLK, params_rate(params));

	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBM_CFM |
			SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_NB_NF);

	/* If line in is enabled at adc send new mask. New value only enables
	 * left channel from the 8th adc to propagate. We also have to change
	 * the slot width value. The 8th channel by default is cut off
	 * by the fpga. To counter this we set the offset value to 0.
	 */
	if (linein_adc_enab)
		snd_soc_dai_set_tdm_slot(codec_dai, 0x00, 0x80,
					params_channels(params), 0);
	else
		snd_soc_dai_set_tdm_slot(codec_dai, 0x00, RX_MASK, 
					params_channels(params),
					snd_pcm_format_width(params_format(params)));

	return 0;
}

struct snd_soc_ops tlv3101_machine_ops = {
	.hw_params = tlv3101_hw_params,
};

static int tlv32x4_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *codec_dai;

	if (substream == NULL) {
		pr_err("%s: invalid stream parameter\n", __func__);
		return -EINVAL;
	}

	rtd = substream->private_data;
	if (rtd == NULL) {
		pr_err("%s: invalid runtime parameter\n", __func__);
		return -EINVAL;
	}

	codec_dai = rtd->codec_dai;
	if (codec_dai == NULL) {
		pr_err("%s: invalid dai parameter\n", __func__);
		return -EINVAL;
	}

	/* set codec DAI configuration */
	if (snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS))
		pr_err("Failed to set fmt for %s\n", codec_dai->name);

	if (snd_soc_dai_set_sysclk(codec_dai, 0, DEFAULT_MCLK,
		SND_SOC_CLOCK_OUT))
		pr_err("%s: Failed to set sysclk for %s\n", __func__,
		rtd->codec_dai->name);

	return 0;
}

struct snd_soc_ops tlv32x4_machine_ops = {
	.hw_params = tlv32x4_hw_params,
};
