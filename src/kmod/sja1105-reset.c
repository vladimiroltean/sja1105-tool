/*
 * SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
 *
 * Copyright (c) 2016-2018, NXP Semiconductors
 */
#include "sja1105.h"
#include <lib/include/gtable.h>

/* RGU */
#define RGU_ADDR    0x100440

/* Back-ported structure from UM11040 Table 112.
 * Reset control register (addr. 100440h)
 * In the SJA1105 E/T, only warm_rst and cold_rst are
 * supported (exposed in UM10944 as rst_ctrl), but the bit
 * offsets of warm_rst and cold_rst are actually reversed.
 */
struct sja1105_reset_cmd {
	uint64_t switch_rst;
	uint64_t cfg_rst;
	uint64_t car_rst;
	uint64_t otp_rst;
	uint64_t warm_rst;
	uint64_t cold_rst;
	uint64_t por_rst;
};

static void
sja1105_reset_cmd_access(void *buf, struct sja1105_reset_cmd *reset,
                         int write, uint64_t device_id)
{
	int  (*pack_or_unpack)(void*, uint64_t*, int, int, int);
	int    size = 4;

	if (write == 0) {
		pack_or_unpack = gtable_unpack;
		memset(reset, 0, sizeof(*reset));
	} else {
		pack_or_unpack = gtable_pack;
		memset(buf, 0, size);
	}
	if (IS_ET(device_id)) {
		pack_or_unpack(buf, &reset->cold_rst,   3, 3, 4);
		pack_or_unpack(buf, &reset->warm_rst,   2, 2, 4);
	} else {
		pack_or_unpack(buf, &reset->switch_rst, 8, 8, 4);
		pack_or_unpack(buf, &reset->cfg_rst,    7, 7, 4);
		pack_or_unpack(buf, &reset->car_rst,    5, 5, 4);
		pack_or_unpack(buf, &reset->otp_rst,    4, 4, 4);
		pack_or_unpack(buf, &reset->warm_rst,   3, 3, 4);
		pack_or_unpack(buf, &reset->cold_rst,   2, 2, 4);
		pack_or_unpack(buf, &reset->por_rst,    1, 1, 4);
	}
}
#define sja1105_reset_cmd_pack(buf, reset, device_id) \
	sja1105_reset_cmd_access(buf, reset, 1, device_id)
#define sja1105_reset_cmd_unpack(buf, reset, device_id) \
	sja1105_reset_cmd_access(buf, reset, 0, device_id)

void sja1105_reset_cmd_show(struct sja1105_reset_cmd *reset)
{
	if (reset->switch_rst)
		logv("Main reset for all functional modules requested");
	if (reset->cfg_rst)
		logv("Chip configuration reset requested");
	if (reset->car_rst)
		logv("Clock and reset control logic reset requested");
	if (reset->otp_rst)
		logv("OTP read cycle for reading product "
		     "config settings requested");
	if (reset->warm_rst)
		logv("Warm reset requested");
	if (reset->cold_rst)
		logv("Cold reset requested");
	if (reset->por_rst)
		logv("Power-on reset requested");
}

int sja1105_reset_cmd_commit(struct sja1105_spi_private *priv,
                             struct sja1105_reset_cmd *reset)
{
	const int BUF_LEN = 4;
	uint8_t packed_buf[BUF_LEN];
	int rc;

	sja1105_reset_cmd_show(reset);
	if (IS_ET(priv->device_id) &&
	    (reset->switch_rst ||
	     reset->cfg_rst ||
	     reset->car_rst ||
	     reset->otp_rst ||
	     reset->por_rst)) {
		loge("Only warm and cold reset is supported for SJA1105 E/T!");
		rc = -EINVAL;
		goto out;
	}
	sja1105_reset_cmd_pack(packed_buf, reset, priv->device_id);

	rc = sja1105_spi_send_packed_buf(priv, SPI_WRITE, RGU_ADDR,
	                                 packed_buf, BUF_LEN);
out:
	return rc;
}

int sja1105_switch_core_reset(struct sja1105_spi_private *priv)
{
	struct sja1105_reset_cmd reset = {0};

	reset.switch_rst = 1;
	return sja1105_reset_cmd_commit(priv, &reset);
}

int sja1105_config_reset(struct sja1105_spi_private *priv)
{
	struct sja1105_reset_cmd reset = {0};

	reset.cfg_rst = 1;
	return sja1105_reset_cmd_commit(priv, &reset);
}

int sja1105_clocking_reset(struct sja1105_spi_private *priv)
{
	struct sja1105_reset_cmd reset = {0};

	reset.car_rst = 1;
	return sja1105_reset_cmd_commit(priv, &reset);
}

int sja1105_otp_reset(struct sja1105_spi_private *priv)
{
	struct sja1105_reset_cmd reset = {0};

	reset.otp_rst = 1;
	return sja1105_reset_cmd_commit(priv, &reset);
}

int sja1105_warm_reset(struct sja1105_spi_private *priv)
{
	struct sja1105_reset_cmd reset = {0};

	reset.warm_rst = 1;
	return sja1105_reset_cmd_commit(priv, &reset);
}

int sja1105_cold_reset(struct sja1105_spi_private *priv)
{
	struct sja1105_reset_cmd reset = {0};

	reset.cold_rst = 1;
	return sja1105_reset_cmd_commit(priv, &reset);
}

int sja1105_por_reset(struct sja1105_spi_private *priv)
{
	struct sja1105_reset_cmd reset = {0};

	reset.por_rst = 1;
	return sja1105_reset_cmd_commit(priv, &reset);
}

