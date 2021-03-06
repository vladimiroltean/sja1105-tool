/*
 * SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
 *
 * Copyright (c) 2016-2018, NXP Semiconductors
 */
#include "sja1105.h"
#include <lib/include/gtable.h>

/* AGU */
#define AGU_ADDR    0x100800

struct sja1105_cfg_pad_mii_tx {
	uint64_t d32_os;
	uint64_t d32_ipud;
	uint64_t d10_os;
	uint64_t d10_ipud;
	uint64_t ctrl_os;
	uint64_t ctrl_ipud;
	uint64_t clk_os;
	uint64_t clk_ih;
	uint64_t clk_ipud;
};


/* CGU */
#define CGU_ADDR    0x100000

/* UM10944 Table 82.
 * IDIV_0_C to IDIV_4_C control registers
 * (addr. 10000Bh to 10000Fh) */
struct sja1105_cgu_idiv {
	uint64_t clksrc;
	uint64_t autoblock;
	uint64_t idiv;
	uint64_t pd;
};

/* UM10944 Table 80.
 * PLL_x_S clock status registers 0 and 1
 * (address 100007h and 100009h) */
struct sja1105_cgu_pll_status {
	uint64_t lock;
};

/*
 * PLL_1_C control register
 *
 * SJA1105 E/T: UM10944 Table 81 (address 10000Ah)
 * SJA1105 P/Q/R/S: UM11040 Table 116 (address 10000Ah)
 *
 */
struct sja1105_cgu_pll_control {
	uint64_t pllclksrc;
	uint64_t msel;
	uint64_t nsel; /* Only for P/Q/R/S series */
	uint64_t autoblock;
	uint64_t psel;
	uint64_t direct;
	uint64_t fbsel;
	uint64_t p23en; /* Only for P/Q/R/S series */
	uint64_t bypass;
	uint64_t pd;
};

#define CLKSRC_MII0_TX_CLK 0x00
#define CLKSRC_MII0_RX_CLK 0x01
#define CLKSRC_MII1_TX_CLK 0x02
#define CLKSRC_MII1_RX_CLK 0x03
#define CLKSRC_MII2_TX_CLK 0x04
#define CLKSRC_MII2_RX_CLK 0x05
#define CLKSRC_MII3_TX_CLK 0x06
#define CLKSRC_MII3_RX_CLK 0x07
#define CLKSRC_MII4_TX_CLK 0x08
#define CLKSRC_MII4_RX_CLK 0x09
#define CLKSRC_PLL0        0x0B
#define CLKSRC_PLL1        0x0E
#define CLKSRC_IDIV0       0x11
#define CLKSRC_IDIV1       0x12
#define CLKSRC_IDIV2       0x13
#define CLKSRC_IDIV3       0x14
#define CLKSRC_IDIV4       0x15

/* UM10944 Table 83.
 * MIIx clock control registers 1 to 30
 * (addresses 100013h to 100035h) */
struct sja1105_cgu_mii_control {
	uint64_t clksrc;
	uint64_t autoblock;
	uint64_t pd;
};

static void sja1105_cgu_idiv_access(void *buf, struct sja1105_cgu_idiv *idiv,
                                    int write)
{
	int  (*pack_or_unpack)(void*, uint64_t*, int, int, int);
	int    size = 4;

	if (write == 0) {
		pack_or_unpack = gtable_unpack;
		memset(idiv, 0, sizeof(*idiv));
	} else {
		pack_or_unpack = gtable_pack;
		memset(buf, 0, size);
	}
	pack_or_unpack(buf, &idiv->clksrc,    28, 24, 4);
	pack_or_unpack(buf, &idiv->autoblock, 11, 11, 4);
	pack_or_unpack(buf, &idiv->idiv,       5,  2, 4);
	pack_or_unpack(buf, &idiv->pd,         0,  0, 4);
}
#define sja1105_cgu_idiv_pack(buf, idiv) sja1105_cgu_idiv_access(buf, idiv, 1)
#define sja1105_cgu_idiv_unpack(buf, idiv) sja1105_cgu_idiv_access(buf, idiv, 0)

static int
sja1105_cgu_idiv_config(struct sja1105_port *port, int enabled, int factor)
{
	struct sja1105_spi_private *priv = spi_get_drvdata(port->spi_dev);
	struct device *dev = &port->spi_dev->dev;
	const int BUF_LEN = 4;
	/* UM10944.pdf, Table 78, CGU Register overview */
	const int idiv_offsets[] = {0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
	uint8_t packed_buf[BUF_LEN];
	struct sja1105_cgu_idiv idiv;

	if (enabled != 0 && enabled != 1) {
		dev_err(dev, "idiv enabled must be true or false\n");
		return -ERANGE;
	}
	if (enabled == 1 && factor != 1 && factor != 10) {
		dev_err(dev, "idiv factor must be 1 or 10\n");
		return -ERANGE;
	}

	/* Payload for packed_buf */
	idiv.clksrc    = 0x0A;            /* 25MHz */
	idiv.autoblock = 1;               /* Block clk automatically */
	idiv.idiv      = factor - 1;      /* Divide by 1 or 10 */
	idiv.pd        = enabled ? 0 : 1; /* Power down? */
	sja1105_cgu_idiv_pack(packed_buf, &idiv);

	return sja1105_spi_send_packed_buf(priv, SPI_WRITE,
	                                   CGU_ADDR + idiv_offsets[port->index],
	                                   packed_buf, BUF_LEN);
}

static void
sja1105_cgu_mii_control_access(void *buf, struct sja1105_cgu_mii_control
                               *mii_control, int write)
{
	int (*pack_or_unpack)(void*, uint64_t*, int, int, int);
	int   size = 4;

	if (write == 0) {
		pack_or_unpack = gtable_unpack;
		memset(mii_control, 0, sizeof(*mii_control));
	} else {
		pack_or_unpack = gtable_pack;
		memset(buf, 0, size);
	}
	pack_or_unpack(buf, &mii_control->clksrc,    28, 24, 4);
	pack_or_unpack(buf, &mii_control->autoblock, 11, 11, 4);
	pack_or_unpack(buf, &mii_control->pd,         0,  0, 4);
}
#define sja1105_cgu_mii_control_pack(buf, mii_control) \
	sja1105_cgu_mii_control_access(buf, mii_control, 1)
#define sja1105_cgu_mii_control_unpack(buf, mii_control) \
	sja1105_cgu_mii_control_access(buf, mii_control, 0)

static int
sja1105_cgu_mii_tx_clk_config(struct sja1105_port *port, int mii_mode)
{
	struct sja1105_spi_private *priv = spi_get_drvdata(port->spi_dev);
	const int BUF_LEN = 4;
	uint8_t packed_buf[BUF_LEN];
	struct  sja1105_cgu_mii_control mii_tx_clk;
	/* UM10944.pdf, Table 78, CGU Register overview */
	const int  mii_tx_clk_offsets_et[]   = {0x13, 0x1A, 0x21, 0x28, 0x2F};
	/* UM11040.pdf, Table 114 */
	const int  mii_tx_clk_offsets_pqrs[] = {0x13, 0x19, 0x1F, 0x25, 0x2B};
	const int *mii_tx_clk_offsets;
	const int mac_clk_sources[] = {
		CLKSRC_MII0_TX_CLK,
		CLKSRC_MII1_TX_CLK,
		CLKSRC_MII2_TX_CLK,
		CLKSRC_MII3_TX_CLK,
		CLKSRC_MII4_TX_CLK,
	};
	const int phy_clk_sources[] = {
		CLKSRC_IDIV0,
		CLKSRC_IDIV1,
		CLKSRC_IDIV2,
		CLKSRC_IDIV3,
		CLKSRC_IDIV4,
	};
	int clksrc;

	/* E/T and P/Q/R/S compatibility */
	mii_tx_clk_offsets = IS_ET(priv->device_id) ?
	                     mii_tx_clk_offsets_et :
	                     mii_tx_clk_offsets_pqrs;

	if (mii_mode == XMII_MAC)
		clksrc = mac_clk_sources[port->index];
	else
		clksrc = phy_clk_sources[port->index];

	/* Payload for packed_buf */
	mii_tx_clk.clksrc    = clksrc;
	mii_tx_clk.autoblock = 1;  /* Autoblock clk while changing clksrc */
	mii_tx_clk.pd        = 0;  /* Power Down off => enabled */
	sja1105_cgu_mii_control_pack(packed_buf, &mii_tx_clk);

	return sja1105_spi_send_packed_buf(priv, SPI_WRITE, CGU_ADDR +
	                                   mii_tx_clk_offsets[port->index],
	                                   packed_buf, BUF_LEN);
}

static int
sja1105_cgu_mii_rx_clk_config(struct sja1105_port *port)
{
	struct sja1105_spi_private *priv = spi_get_drvdata(port->spi_dev);
	const int BUF_LEN = 4;
	uint8_t packed_buf[BUF_LEN];
	struct  sja1105_cgu_mii_control mii_rx_clk;
	/* UM10944.pdf, Table 78, CGU Register overview */
	const int  mii_rx_clk_offsets_et[]   = {0x14, 0x1B, 0x22, 0x29, 0x30};
	/* UM11040.pdf, Table 114 */
	const int  mii_rx_clk_offsets_pqrs[] = {0x14, 0x1A, 0x20, 0x26, 0x2C};
	const int *mii_rx_clk_offsets;
	const int clk_sources[] = {
		CLKSRC_MII0_RX_CLK,
		CLKSRC_MII1_RX_CLK,
		CLKSRC_MII2_RX_CLK,
		CLKSRC_MII3_RX_CLK,
		CLKSRC_MII4_RX_CLK,
	};

	/* E/T and P/Q/R/S compatibility */
	mii_rx_clk_offsets = IS_ET(priv->device_id) ?
	                     mii_rx_clk_offsets_et :
	                     mii_rx_clk_offsets_pqrs;

	/* Payload for packed_buf */
	mii_rx_clk.clksrc    = clk_sources[port->index];
	mii_rx_clk.autoblock = 1;  /* Autoblock clk while changing clksrc */
	mii_rx_clk.pd        = 0;  /* Power Down off => enabled */
	sja1105_cgu_mii_control_pack(packed_buf, &mii_rx_clk);

	return sja1105_spi_send_packed_buf(priv, SPI_WRITE, CGU_ADDR +
	                                   mii_rx_clk_offsets[port->index],
	                                   packed_buf, BUF_LEN);
}

static int
sja1105_cgu_mii_ext_tx_clk_config(struct sja1105_port *port)
{
	struct sja1105_spi_private *priv = spi_get_drvdata(port->spi_dev);
	const int BUF_LEN = 4;
	uint8_t packed_buf[BUF_LEN];
	struct  sja1105_cgu_mii_control mii_ext_tx_clk;
	/* UM10944.pdf, Table 78, CGU Register overview */
	const int  mii_ext_tx_clk_offsets_et[]   = {0x18, 0x1F, 0x26, 0x2D, 0x34};
	/* UM11040.pdf, Table 114 */
	const int  mii_ext_tx_clk_offsets_pqrs[] = {0x17, 0x1D, 0x23, 0x29, 0x2F};
	const int *mii_ext_tx_clk_offsets;
	const int clk_sources[] = {
		CLKSRC_IDIV0,
		CLKSRC_IDIV1,
		CLKSRC_IDIV2,
		CLKSRC_IDIV3,
		CLKSRC_IDIV4,
	};

	/* E/T and P/Q/R/S compatibility */
	mii_ext_tx_clk_offsets = IS_ET(priv->device_id) ?
	                         mii_ext_tx_clk_offsets_et :
	                         mii_ext_tx_clk_offsets_pqrs;

	/* Payload for packed_buf */
	mii_ext_tx_clk.clksrc    = clk_sources[port->index];
	mii_ext_tx_clk.autoblock = 1; /* Autoblock clk while changing clksrc */
	mii_ext_tx_clk.pd        = 0; /* Power Down off => enabled */
	sja1105_cgu_mii_control_pack(packed_buf, &mii_ext_tx_clk);

	return sja1105_spi_send_packed_buf(priv, SPI_WRITE, CGU_ADDR +
	                                   mii_ext_tx_clk_offsets[port->index],
	                                   packed_buf, BUF_LEN);
}

static int
sja1105_cgu_mii_ext_rx_clk_config(struct sja1105_port *port)
{
	struct sja1105_spi_private *priv = spi_get_drvdata(port->spi_dev);
	const int BUF_LEN = 4;
	uint8_t packed_buf[BUF_LEN];
	struct  sja1105_cgu_mii_control mii_ext_rx_clk;
	/* UM10944.pdf, Table 78, CGU Register overview */
	const int  mii_ext_rx_clk_offsets_et[]   = {0x19, 0x20, 0x27, 0x2E, 0x35};
	/* UM11040.pdf, Table 114 */
	const int  mii_ext_rx_clk_offsets_pqrs[] = {0x18, 0x1E, 0x24, 0x2A, 0x30};
	const int *mii_ext_rx_clk_offsets;
	const int clk_sources[] = {
		CLKSRC_IDIV0,
		CLKSRC_IDIV1,
		CLKSRC_IDIV2,
		CLKSRC_IDIV3,
		CLKSRC_IDIV4,
	};

	/* E/T and P/Q/R/S compatibility */
	mii_ext_rx_clk_offsets = IS_ET(priv->device_id) ?
	                         mii_ext_rx_clk_offsets_et :
	                         mii_ext_rx_clk_offsets_pqrs;

	/* Payload for packed_buf */
	mii_ext_rx_clk.clksrc    = clk_sources[port->index];
	mii_ext_rx_clk.autoblock = 1; /* Autoblock clk while changing clksrc */
	mii_ext_rx_clk.pd        = 0; /* Power Down off => enabled */
	sja1105_cgu_mii_control_pack(packed_buf, &mii_ext_rx_clk);

	return sja1105_spi_send_packed_buf(priv, SPI_WRITE, CGU_ADDR +
	                                   mii_ext_rx_clk_offsets[port->index],
	                                   packed_buf, BUF_LEN);
}

static int
mii_clocking_setup(struct sja1105_port *port, int mii_mode)
{
	struct device *dev = &port->spi_dev->dev;
	int rc;

	if (mii_mode != XMII_MAC && mii_mode != XMII_PHY)
		goto error;

	dev_dbg(dev, "Configuring MII-%s clocking for port %s\n",
	       (mii_mode == XMII_MAC) ? "MAC" : "PHY", port->net_dev->name);
	/*   * If mii_mode is MAC, disable IDIV
	 *   * If mii_mode is PHY, enable IDIV and configure for 1/1 divider
	 */
	rc = sja1105_cgu_idiv_config(port, (mii_mode == XMII_PHY), 1);
	if (rc < 0)
		goto error;

	/* Configure CLKSRC of MII_TX_CLK_n
	 *   * If mii_mode is MAC, select TX_CLK_n
	 *   * If mii_mode is PHY, select IDIV_n
	 */
	rc = sja1105_cgu_mii_tx_clk_config(port, mii_mode);
	if (rc < 0)
		goto error;

	/* Configure CLKSRC of MII_RX_CLK_n
	 * Select RX_CLK_n
	 */
	rc = sja1105_cgu_mii_rx_clk_config(port);
	if (rc < 0)
		goto error;

	if (mii_mode == XMII_PHY) {
		/* In MII mode the PHY (which is us) drives the TX_CLK pin */

		/* Configure CLKSRC of EXT_TX_CLK_n
		 * Select IDIV_n
		 */
		rc = sja1105_cgu_mii_ext_tx_clk_config(port);
		if (rc < 0)
			goto error;

		/* Configure CLKSRC of EXT_RX_CLK_n
		 * Select IDIV_n
		 */
		rc = sja1105_cgu_mii_ext_rx_clk_config(port);
		if (rc < 0)
			goto error;
	}
	return 0;
error:
	return -1;
}

static void
sja1105_cgu_pll_control_access(void *buf, struct sja1105_cgu_pll_control
                               *pll_control, int write, uint64_t device_id)
{
	int (*pack_or_unpack)(void*, uint64_t*, int, int, int);
	int   size = 4;

	if (write == 0) {
		pack_or_unpack = gtable_unpack;
		memset(pll_control, 0, sizeof(*pll_control));
	} else {
		pack_or_unpack = gtable_pack;
		memset(buf, 0, size);
	}
	pack_or_unpack(buf, &pll_control->pllclksrc, 28, 24, 4);
	pack_or_unpack(buf, &pll_control->msel,      23, 16, 4);
	pack_or_unpack(buf, &pll_control->autoblock, 11, 11, 4);
	pack_or_unpack(buf, &pll_control->psel,       9,  8, 4);
	pack_or_unpack(buf, &pll_control->direct,     7,  7, 4);
	pack_or_unpack(buf, &pll_control->fbsel,      6,  6, 4);
	pack_or_unpack(buf, &pll_control->bypass,     1,  1, 4);
	pack_or_unpack(buf, &pll_control->pd,         0,  0, 4);
	if (IS_PQRS(device_id)) {
		pack_or_unpack(buf, &pll_control->nsel, 13, 12, 4);
		pack_or_unpack(buf, &pll_control->p23en, 2,  2, 4);
	}
}
#define sja1105_cgu_pll_control_pack(buf, pll_control, device_id) \
	sja1105_cgu_pll_control_access(buf, pll_control, 1, device_id)
#define sja1105_cgu_pll_control_unpack(buf, pll_control, device_id) \
	sja1105_cgu_pll_control_access(buf, pll_control, 0, device_id)

static int
sja1105_cgu_rgmii_tx_clk_config(struct sja1105_port *port, int speed_mbps)
{
	struct sja1105_spi_private *priv = spi_get_drvdata(port->spi_dev);
	int clksrc;
	const int BUF_LEN = 4;
	/* UM10944.pdf, Table 78, CGU Register overview */
	const int txc_offsets_et[] = {0x16, 0x1D, 0x24, 0x2B, 0x32};
	/* UM11040.pdf, Table 114, CGU Register overview */
	const int txc_offsets_pqrs[] = {0x16, 0x1C, 0x22, 0x28, 0x2E};
	const int *txc_offsets;
	uint8_t packed_buf[BUF_LEN];
	struct  sja1105_cgu_mii_control txc;

	/* E/T and P/Q/R/S compatibility */
	txc_offsets = IS_ET(priv->device_id) ?
	                     txc_offsets_et :
	                     txc_offsets_pqrs;

	if (speed_mbps == 1000)
		clksrc = CLKSRC_PLL0;
	else {
		int clk_sources[] = {CLKSRC_IDIV0, CLKSRC_IDIV1, CLKSRC_IDIV2,
		                     CLKSRC_IDIV3, CLKSRC_IDIV4};
		clksrc = clk_sources[port->index];
	}

	/* Payload */
	txc.clksrc    = clksrc; /* RGMII: 125MHz for 1000, */
	                        /*        25MHz for 100, 2.5MHz for 10 */
	txc.autoblock = 1;      /* Autoblock clk while changing clksrc */
	txc.pd        = 0;      /* Power Down off => enabled */
	sja1105_cgu_mii_control_pack(packed_buf, &txc);

	return sja1105_spi_send_packed_buf(priv,SPI_WRITE,
	                                   CGU_ADDR + txc_offsets[port->index],
	                                   packed_buf, BUF_LEN);
}

/* AGU */
static void
sja1105_cfg_pad_mii_tx_access(void *buf, struct sja1105_cfg_pad_mii_tx
                              *pad_mii_tx, int write)
{
	int  (*pack_or_unpack)(void*, uint64_t*, int, int, int);
	int    size = 4;

	if (write == 0) {
		pack_or_unpack = gtable_unpack;
		memset(pad_mii_tx, 0, sizeof(*pad_mii_tx));
	} else {
		pack_or_unpack = gtable_pack;
		memset(buf, 0, size);
	}
	pack_or_unpack(buf, &pad_mii_tx->d32_os,   28, 27, 4);
	pack_or_unpack(buf, &pad_mii_tx->d32_ipud, 25, 24, 4);
	pack_or_unpack(buf, &pad_mii_tx->d10_os,   20, 19, 4);
	pack_or_unpack(buf, &pad_mii_tx->d10_ipud, 17, 16, 4);
	pack_or_unpack(buf, &pad_mii_tx->ctrl_os,  12, 11, 4);
	pack_or_unpack(buf, &pad_mii_tx->ctrl_ipud, 9,  8, 4);
	pack_or_unpack(buf, &pad_mii_tx->clk_os,    4,  3, 4);
	pack_or_unpack(buf, &pad_mii_tx->clk_ih,    2,  2, 4);
	pack_or_unpack(buf, &pad_mii_tx->clk_ipud,  1,  0, 4);
}
#define sja1105_cfg_pad_mii_tx_pack(buf, pad_mii_tx) \
	sja1105_cfg_pad_mii_tx_access(buf, pad_mii_tx, 1)
#define sja1105_cfg_pad_mii_tx_unpack(buf, pad_mii_tx) \
	sja1105_cfg_pad_mii_tx_access(buf, pad_mii_tx, 0)

static int
sja1105_rgmii_cfg_pad_tx_config(struct sja1105_port *port)
{
	struct sja1105_spi_private *priv = spi_get_drvdata(port->spi_dev);
	const int BUF_LEN = 4;
	uint8_t packed_buf[BUF_LEN];
	/* UM10944.pdf, Table 86, ACU Register overview */
	int     pad_mii_tx_offsets[] = {0x00, 0x02, 0x04, 0x06, 0x08};
	struct  sja1105_cfg_pad_mii_tx pad_mii_tx;

	/* Payload */
	pad_mii_tx.d32_os    = 3; /* TXD[3:2] output stage: */
	                          /*          high noise/high speed */
	pad_mii_tx.d32_ipud  = 2; /* TXD[3:2] input stage: */
	                          /*          plain input (default) */
	pad_mii_tx.d10_os    = 3; /* TXD[1:0] output stage: */
	                          /*          high noise/high speed */
	pad_mii_tx.d10_ipud  = 2; /* TXD[1:0] input stage: */
	                          /*          plain input (default) */
	pad_mii_tx.ctrl_os   = 3; /* TX_CTL / TX_ER output stage */
	pad_mii_tx.ctrl_ipud = 2; /* TX_CTL / TX_ER input stage (default) */
	pad_mii_tx.clk_os    = 3; /* TX_CLK output stage */
	pad_mii_tx.clk_ih    = 0; /* TX_CLK input hysteresis (default) */
	pad_mii_tx.clk_ipud  = 2; /* TX_CLK input stage (default) */
	sja1105_cfg_pad_mii_tx_pack(packed_buf, &pad_mii_tx);

	return sja1105_spi_send_packed_buf(priv, SPI_WRITE, AGU_ADDR +
	                                   pad_mii_tx_offsets[port->index],
	                                   packed_buf, BUF_LEN);
}

static int
rgmii_clocking_setup(struct sja1105_port *port)
{
	int speed_mbps = sja1105_port_get_speed(port);
	struct device *dev = &port->spi_dev->dev;
	int rc = 0;

	dev_dbg(dev, "Configuring RGMII clocking for port %s, speed %dMbps\n",
	        port->net_dev->name, speed_mbps);

	switch (speed_mbps) {
	case 1000:
		/* 1000Mbps, IDIV disabled, divide by 1 */
		rc = sja1105_cgu_idiv_config(port, 0, 1);
		break;
	case 100:
		/* 100Mbps, IDIV enabled, divide by 1 */
		rc = sja1105_cgu_idiv_config(port, 1, 1);
		break;
	case 10:
		/* 10Mbps, IDIV enabled, divide by 10 */
		rc = sja1105_cgu_idiv_config(port, 1, 10);
		break;
	case 0:
		/* Skip CGU configuration if there is no speed available
		 * (e.g. link is not established yet) */
		dev_dbg(dev, "Link speed not available for port %s, skipping CGU configuration\n",
		        port->net_dev->name);
		rc = 0;
		goto out;
	default:
		rc = -EINVAL;
	}

	if (rc < 0) {
		dev_err(dev, "configuring idiv failed\n");
		goto out;
	}
	rc = sja1105_cgu_rgmii_tx_clk_config(port, speed_mbps);
	if (rc < 0) {
		dev_err(dev, "configuring rgmii tx clock failed\n");
		goto out;
	}
	rc = sja1105_rgmii_cfg_pad_tx_config(port);
	if (rc < 0) {
		dev_err(dev, "configuring tx pad registers failed\n");
		goto out;
	}
out:
	return rc;
}

static int
sja1105_cgu_rmii_ref_clk_config(struct sja1105_port *port)
{
	struct sja1105_spi_private *priv = spi_get_drvdata(port->spi_dev);
	const int BUF_LEN = 4;
	struct  sja1105_cgu_mii_control ref_clk;
	uint8_t packed_buf[BUF_LEN];
	/* UM10944.pdf, Table 78, CGU Register overview */
	const int ref_clk_offsets_et[] = {0x15, 0x1C, 0x23, 0x2A, 0x31};
	/* UM11040.pdf, Table 114, CGU Register overview */
	const int ref_clk_offsets_pqrs[] = {0x15, 0x1B, 0x21, 0x27, 0x2D};
	const int *ref_clk_offsets;
	const int clk_sources[] = {
		CLKSRC_MII0_TX_CLK,
		CLKSRC_MII1_TX_CLK,
		CLKSRC_MII2_TX_CLK,
		CLKSRC_MII3_TX_CLK,
		CLKSRC_MII4_TX_CLK,
	};

	/* E/T and P/Q/R/S compatibility */
	ref_clk_offsets = IS_ET(priv->device_id) ?
	                  ref_clk_offsets_et : ref_clk_offsets_pqrs;

	/* Payload for packed_buf */
	ref_clk.clksrc    = clk_sources[port->index];
	ref_clk.autoblock = 1;      /* Autoblock clk while changing clksrc */
	ref_clk.pd        = 0;      /* Power Down off => enabled */
	sja1105_cgu_mii_control_pack(packed_buf, &ref_clk);

	return sja1105_spi_send_packed_buf(priv, SPI_WRITE, CGU_ADDR +
	                                   ref_clk_offsets[port->index],
	                                   packed_buf, BUF_LEN);
}

static int
sja1105_cgu_rmii_ext_tx_clk_config(struct sja1105_port *port)
{
	struct sja1105_spi_private *priv = spi_get_drvdata(port->spi_dev);
	const int BUF_LEN = 4;
	struct  sja1105_cgu_mii_control ext_tx_clk;
	uint8_t packed_buf[BUF_LEN];
	/* UM10944.pdf, Table 78, CGU Register overview */
	const int ext_tx_clk_offsets_et[] = {0x18, 0x1F, 0x26, 0x2D, 0x34};
	/* UM11040.pdf, Table 114, CGU Register overview */
	const int ext_tx_clk_offsets_pqrs[] = {0x17, 0x1D, 0x23, 0x29, 0x2F};
	const int *ext_tx_clk_offsets;

	/* E/T and P/Q/R/S compatibility */
	ext_tx_clk_offsets = IS_ET(priv->device_id) ?
	                     ext_tx_clk_offsets_et :
	                     ext_tx_clk_offsets_pqrs;

	/* Payload for packed_buf */
	ext_tx_clk.clksrc    = CLKSRC_PLL1;
	ext_tx_clk.autoblock = 1;   /* Autoblock clk while changing clksrc */
	ext_tx_clk.pd        = 0;   /* Power Down off => enabled */
	sja1105_cgu_mii_control_pack(packed_buf, &ext_tx_clk);

	return sja1105_spi_send_packed_buf(priv, SPI_WRITE, CGU_ADDR +
	                                   ext_tx_clk_offsets[port->index],
	                                   packed_buf, BUF_LEN);
}

static int sja1105_cgu_rmii_pll_config(struct sja1105_spi_private *priv)
{
	struct device *dev = &priv->spi_dev->dev;
	struct  sja1105_cgu_pll_control pll;
	const int PLL1_OFFSET = 0x0A;
	const int BUF_LEN = 4;
	uint8_t packed_buf[BUF_LEN];
	int     rc;

	/* PLL1 must be enabled and output 50 Mhz.
	 * This is done by writing first 0x0A010941 to
	 * the PLL_1_C register and then deasserting
	 * power down (PD) 0x0A010940. */

	/* Step 1: PLL1 setup for 50Mhz */
	pll.pllclksrc = 0xA;
	pll.msel      = 0x1;
	pll.autoblock = 0x1;
	pll.psel      = 0x1;
	pll.direct    = 0x0;
	pll.fbsel     = 0x1;
	pll.bypass    = 0x0;
	pll.pd        = 0x1;
	/* P/Q/R/S only */
	pll.nsel      = 0x0; /* PLL pre-divider is 1 (nsel + 1) */
	pll.p23en     = 0x0; /* disable 120 and 240 degree phase PLL outputs */

	sja1105_cgu_pll_control_pack(packed_buf, &pll, priv->device_id);
	rc = sja1105_spi_send_packed_buf(priv, SPI_WRITE,
	                                 CGU_ADDR + PLL1_OFFSET,
	                                 packed_buf, BUF_LEN);
	if (rc < 0) {
		dev_err(dev, "failed to configure PLL1 for 50MHz\n");
		goto out;
	}

	/* Step 2: Enable PLL1 */
	pll.pd        = 0x0;

	sja1105_cgu_pll_control_pack(packed_buf, &pll, priv->device_id);
	rc = sja1105_spi_send_packed_buf(priv, SPI_WRITE,
	                                 CGU_ADDR + PLL1_OFFSET,
	                                 packed_buf, BUF_LEN);
	if (rc < 0) {
		dev_err(dev, "failed to enable PLL1\n");
		goto out;
	}
out:
	return rc;
}

static int rmii_clocking_setup(struct sja1105_port *port, int rmii_mode)
{
	struct sja1105_spi_private *priv = spi_get_drvdata(port->spi_dev);
	struct device *dev = &port->spi_dev->dev;
	int rc;

	if (rmii_mode != XMII_MAC && rmii_mode != XMII_PHY) {
		dev_err(dev, "RMII mode must either be MAC or PHY\n");
		rc = -EINVAL;
		goto out;
	}
	dev_dbg(dev, "Configuring RMII-%s clocking for port %s\n",
	       (rmii_mode == XMII_MAC) ? "MAC" : "PHY", port->net_dev->name);
	/* AH1601.pdf chapter 2.5.1. Sources */
	if (rmii_mode == XMII_MAC) {
		/* Configure and enable PLL1 for 50Mhz output */
		rc = sja1105_cgu_rmii_pll_config(priv);
		if (rc < 0)
			goto out;
	}
	/* Disable IDIV for this port */
	rc = sja1105_cgu_idiv_config(port, 0, 1);
	if (rc < 0)
		goto out;
	/* Source to sink mappings */
	rc = sja1105_cgu_rmii_ref_clk_config(port);
	if (rc < 0)
		goto out;
	if (rmii_mode == XMII_MAC) {
		rc = sja1105_cgu_rmii_ext_tx_clk_config(port);
		if (rc < 0)
			goto out;
	}
out:
	return rc;
}

/* TODO:
 * Standard clause 22 registers for the internal SGMII PCS are
 * memory-mapped starting at SPI address 0x1F0000.
 * The SGMII port should already have a basic initialization done
 * through the static configuration tables.
 * If any further SGMII initialization steps (autonegotiation or
 * checking the link training status) need to be done, they
 * might as well be added here.
 */
static int
sgmii_clocking_setup(struct sja1105_port *port)
{
	dev_err(&port->spi_dev->dev,
	        "TODO: Configure SGMII clocking for port %s\n",
	        port->net_dev->name);
	return 0;
}

int sja1105_clocking_setup_port(struct sja1105_port *port)
{
	struct sja1105_spi_private *priv = spi_get_drvdata(port->spi_dev);
	struct sja1105_xmii_params_entry *mii;
	struct device *dev = &port->spi_dev->dev;
	int rc = 0;

	mii = priv->static_config.xmii_params;

	switch (mii->xmii_mode[port->index]) {
	case XMII_MODE_MII:
		mii_clocking_setup(port, mii->phy_mac[port->index]);
		break;
	case XMII_MODE_RMII:
		rmii_clocking_setup(port, mii->phy_mac[port->index]);
		break;
	case XMII_MODE_RGMII:
		rgmii_clocking_setup(port);
		break;
	case XMII_MODE_SGMII:
		if (!IS_PQRS(priv->device_id)) {
			dev_err(dev, "SGMII mode only supported on P/Q/R/S!\n");
			rc = -EINVAL;
			goto out;
		}
		if ((IS_R(priv->device_id, priv->part_nr) ||
		     IS_S(priv->device_id, priv->part_nr)) &&
		    (port->index == 4))
			sgmii_clocking_setup(port);
		else
			dev_dbg(dev, "Port %s is tri-stated\n",
			        port->net_dev->name);
		break;
	default:
		dev_err(dev, "Invalid MII mode specified for port %s: %" PRIu64,
		        port->net_dev->name, mii->xmii_mode[port->index]);
		rc = -EINVAL;
	}
out:
	return rc;
}

int sja1105_clocking_setup(struct sja1105_spi_private *priv)
{
	struct list_head *pos, *q;
	struct sja1105_port *port;
	int rc = 0;

	list_for_each_safe(pos, q, &(priv->port_list_head.list)) {
		port = list_entry(pos, struct sja1105_port, list);
		rc = sja1105_clocking_setup_port(port);
		if (rc)
			break;
	}
	return rc;
}

