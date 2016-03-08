/*
 * ASoC Driver for SabreBerry32
 *
 * Author: Satoru Kawase, Takahito Nishiara
 *      Copyright 2016
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../codecs/sabre9018q2c.h"


static int snd_rpi_sabreberry32_init(struct snd_soc_pcm_runtime *rtd)
{
    struct snd_soc_codec *codec = rtd->codec;

    /* Check Chip ID */
    if (sabre9018q2c_check_chip_id(codec) == false)
    {
        return (-EINVAL);
    }

#if (defined(MASTER_MODE) && (MASTER_MODE == 1))
    /* Switch to Master Mode */
    dev_info(codec->dev, "Master Mode\n");
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_10, 0x80, (1 << 7));
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_10, 0x60, (2 << 5));
#else
    /* Switch to Slave Mode */
    dev_info(codec->dev, "Slave Mode\n");
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_10, 0x80, (0 << 7));
#endif

    /* Initialize SABRE9018Q2C */
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_8,  0x0F, 2 << 0);
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_1,  0x0C, 0 << 2);
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_10, 0x0F, 0 << 0);
    snd_soc_write(codec, SABRE9018Q2C_REG_4,  0x06);
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_5,  0x80, 1 << 7);
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_5,  0x7F, 0x6F);
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_42, 0x40, 1 << 6);
    snd_soc_write(codec, SABRE9018Q2C_REG_15, 0x00);
    snd_soc_write(codec, SABRE9018Q2C_REG_16, 0x00);
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_17,
                                    0xFF, (MASTER_TRIM_VALUE >> 0));
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_18,
                                    0xFF, (MASTER_TRIM_VALUE >> 8));
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_19,
                                    0xFF, (MASTER_TRIM_VALUE >> 16));
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_20,
                                    0xFF, (MASTER_TRIM_VALUE >> 24));
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_7,  0xC0, 2 << 5);
    snd_soc_write(codec, SABRE9018Q2C_REG_12, 0x1A);
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_12, 0xF0, 1 << 4);
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_13, 0x40, 0 << 6);
    snd_soc_write(codec, SABRE9018Q2C_REG_23, 0x01);
    snd_soc_write(codec, SABRE9018Q2C_REG_22, 0x10);

    return 0;
}


static int snd_rpi_sabreberry32_clk_for_rate(int sample_rate)
{
    int type;

    switch (sample_rate) {
    case 11025:
    case 22050:
    case 44100:
    case 88200:
    case 176400:
        type = SAMPLE_RATE_TYPE_44_1;
        break;

    default:
        type = SAMPLE_RATE_TYPE_48;
        break;
    }

    return type;
}

static void snd_rpi_sabreberry32_set_mclk(
        struct snd_soc_codec *codec, int sample_rate)
{
    int clk_type;

    clk_type = snd_rpi_sabreberry32_clk_for_rate(sample_rate);
    if (clk_type == SAMPLE_RATE_TYPE_44_1) {
        /* Configure SABRE9018Q2C GPIOs : GPIO2 = Output Low */
        snd_soc_update_bits(codec, SABRE9018Q2C_REG_8, 0xF0, 7 << 4);
    } else {
        /* Configure SABRE9018Q2C GPIOs : GPIO2 = Output High */
        snd_soc_update_bits(codec, SABRE9018Q2C_REG_8, 0xF0, 15 << 4);
    }
}

static int snd_rpi_sabreberry32_hw_params(
        struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
    struct snd_soc_pcm_runtime *rtd     = substream->private_data;
    struct snd_soc_dai         *cpu_dai = rtd->cpu_dai;
    struct snd_soc_codec       *codec   = rtd->codec;
    int bclk_ratio;
    unsigned int div_mode;

    /* Change MCLK Source */
    snd_rpi_sabreberry32_set_mclk(codec, params_rate(params));

    /* Change MCLK Divider */
    switch (params_rate(params))
    {
    case 44100:
    case 48000:
        div_mode = (2 << 5);
        break;

    case 88200:
    case 96000:
        div_mode = (1 << 5);
        break;

    case 176400:
    case 192000:
        div_mode = (0 << 5);
        break;

    default:
        return (-EINVAL);
    }
    snd_soc_update_bits(codec, SABRE9018Q2C_REG_10, 0x60, div_mode);

    bclk_ratio = snd_pcm_format_physical_width(
                            params_format(params)) * params_channels(params);
    return snd_soc_dai_set_bclk_ratio(cpu_dai, bclk_ratio);
}

/* machine stream operations */
static struct snd_soc_ops snd_rpi_sabreberry32_ops = {
    .hw_params = snd_rpi_sabreberry32_hw_params,
};


static struct snd_soc_dai_link snd_rpi_sabreberry32_dai[] = {
    {
        .name           = "SabreBerry32",
        .stream_name    = "SabreBerry32 DAC",
        .cpu_dai_name   = "bcm2708-i2s.0",
        .codec_dai_name = "sabre9018q2c-dai",
        .platform_name  = "bcm2708-i2s.0",
        .codec_name     = "sabre9018q2c-i2c.1-0048",
#if (defined(MASTER_MODE) && (MASTER_MODE == 1))
        .dai_fmt        = SND_SOC_DAIFMT_I2S
                            | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM,
#else
        .dai_fmt        = SND_SOC_DAIFMT_I2S
                            | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
#endif
        .init           = snd_rpi_sabreberry32_init,
        .ops            = &snd_rpi_sabreberry32_ops,
    }
};

/* audio machine driver */
static struct snd_soc_card snd_rpi_sabreberry32 = {
    .name      = "snd_rpi_sabreberry32",
    .dai_link  = snd_rpi_sabreberry32_dai,
    .num_links = ARRAY_SIZE(snd_rpi_sabreberry32_dai)
};


static int snd_rpi_sabreberry32_probe(struct platform_device *pdev)
{
    int ret = 0;

    snd_rpi_sabreberry32.dev = &pdev->dev;
    if (pdev->dev.of_node) {
        struct device_node *i2s_node;
        struct snd_soc_dai_link *dai;

        dai = &snd_rpi_sabreberry32_dai[0];
        i2s_node = of_parse_phandle(pdev->dev.of_node, "i2s-controller", 0);
        if (i2s_node) {
            dai->cpu_dai_name     = NULL;
            dai->cpu_of_node      = i2s_node;
            dai->platform_name    = NULL;
            dai->platform_of_node = i2s_node;
        } else {
            dev_err(&pdev->dev,
                    "Property 'i2s-controller' missing or invalid\n");
            return (-EINVAL);
        }
    }

    ret = snd_soc_register_card(&snd_rpi_sabreberry32);
    if (ret) {
        dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
    }

    return ret;
}

static int snd_rpi_sabreberry32_remove(struct platform_device *pdev)
{
    return snd_soc_unregister_card(&snd_rpi_sabreberry32);
}

static const struct of_device_id snd_rpi_sabreberry32_of_match[] = {
    { .compatible = "takazine,sabreberry32", },
    {}
};
MODULE_DEVICE_TABLE(of, snd_rpi_sabreberry32_of_match);

static struct platform_driver snd_rpi_sabreberry32_driver = {
    .driver = {
        .name           = "snd-rpi-sabreberry32",
        .owner          = THIS_MODULE,
        .of_match_table = snd_rpi_sabreberry32_of_match,
    },
    .probe  = snd_rpi_sabreberry32_probe,
    .remove = snd_rpi_sabreberry32_remove,
};
module_platform_driver(snd_rpi_sabreberry32_driver);


MODULE_DESCRIPTION("ASoC Driver for SabreBerry32");
MODULE_AUTHOR("Satoru Kawase <satoru.kawase@gmail.com>");
MODULE_LICENSE("GPL");
