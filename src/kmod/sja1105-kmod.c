/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2018, Sensor-Technik Wiedemann GmbH
 * Copyright (c) 2018, NXP Semiconductors
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/spi/spi.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <linux/netdev_features.h>
#include <linux/etherdevice.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/phy.h>
#include <linux/gpio/consumer.h>
#include "sja1105.h"
#include <lib/include/static-config.h>

MODULE_AUTHOR("Vladimir Oltean <vladimir.oltean@nxp.com>");
MODULE_AUTHOR("Georg Waibel <georg.waibel@sensor-technik.de>");
MODULE_DESCRIPTION("SJA1105 Driver");
MODULE_LICENSE("GPL");

/* Common function used for issuing a reset pulse
 * to either the switch (priv->reset_gpio) or to an individual
 * external PHY (port->reset_gpio).
 */
static int sja1105_hw_reset_chip(struct gpio_desc *gpio,
                                 unsigned int pulse_len,
                                 unsigned int startup_delay)
{
	if (IS_ERR(gpio))
		return PTR_ERR(gpio);

	gpiod_set_value_cansleep(gpio, 1);
	/* Wait for minimum reset pulse length */
	msleep(pulse_len);
	gpiod_set_value_cansleep(gpio, 0);
	/* Wait until chip is ready after reset */
	msleep(startup_delay);

	return 0;
}

static void sja1105_patch_mac_mii_settings(struct sja1105_spi_private *priv)
{
	struct list_head *pos, *q;
	struct sja1105_port *port;
	struct sja1105_xmii_params_entry *mii;
	struct sja1105_mac_config_entry *mac;
	int i;

	priv->static_config.xmii_params_count = 1;
	mii = &priv->static_config.xmii_params[0];
	/* Initialization */
	for (i = 0; i < SJA1105_NUM_PORTS; i++) {
		mii->xmii_mode[i] = XMII_MODE_TRISTATE;
		mii->phy_mac[i]   = XMII_MAC;
	}

	list_for_each_safe(pos, q, &(priv->port_list_head.list)) {
		port = list_entry(pos, struct sja1105_port, list);

		switch (port->phy_mode) {
		case PHY_INTERFACE_MODE_MII:
			mii->xmii_mode[port->index] = XMII_MODE_MII;
			break;
		case PHY_INTERFACE_MODE_RMII:
			mii->xmii_mode[port->index] = XMII_MODE_RMII;
			break;
		case PHY_INTERFACE_MODE_RGMII:
		case PHY_INTERFACE_MODE_RGMII_ID:
		case PHY_INTERFACE_MODE_RGMII_RXID:
		case PHY_INTERFACE_MODE_RGMII_TXID:
			mii->xmii_mode[port->index] = XMII_MODE_RGMII;
			break;
		case PHY_INTERFACE_MODE_SGMII:
			mii->xmii_mode[port->index] = XMII_MODE_SGMII;
			break;
		default:
			dev_err(&priv->spi_dev->dev,
			        "Unsupported PHY mode %s for port %d!\n",
			        phy_modes(port->phy_mode), port->index);
		}

		/* Always put the MAC speed in automatic mode, since it can
		 * be retrieved from the phy_dev object in both the fixed-link
		 * and physical PHY scenarios. The speed will actually be set
		 * during the sja1105_netdev_adjust_link callback.
		 */
		mac = &priv->static_config.mac_config[port->index];
		mac->speed = SJA1105_SPEED_AUTO;
		/* Only set default MAC/PHY mode for this port
		 * if the device tree override was not specified */
		mii->phy_mac[port->index] = port->dt_xmii_mode;
		if (port->dt_xmii_mode != -1)
			continue;

		if (of_phy_is_fixed_link(port->node))
			/* Default to PHY mode */
			mii->phy_mac[port->index] = XMII_PHY;
		else
			/* phy-handle present => put port in MAC mode */
			mii->phy_mac[port->index] = XMII_MAC;
	}
}

static void sja1105_patch_host_port(struct sja1105_spi_private *priv)
{
	priv->static_config.general_params[0].host_port =
		priv->switch_host_port->index;
}

/*
 * Load the SJA1105 static configuration from rootfs and unpack it
 * into private data.
 */
int sja1105_load_firmware(struct sja1105_spi_private *priv)
{
	int rc;
	void* buf;
	struct device *dev = &priv->spi_dev->dev;
	const struct firmware *fw_entry;

	/* Load firmware from user space (rootfs) and unpack it */
	rc = request_firmware(&fw_entry, priv->staging_area, dev);
	if (rc) {
		dev_err(dev, "Error requesting firmware file %s\n",
		        priv->staging_area);
		goto err_out;
	}

	/* sja1105_static_config_unpack() wants a non-const data pointer but
	 * fw_entry is const -> copy data to a local buffer */
	buf = kmalloc(fw_entry->size, GFP_KERNEL);
	if (!buf) {
		rc = -ENOMEM;
		goto err_fw;
	}
	memcpy(buf, fw_entry->data, fw_entry->size);

	rc = sja1105_static_config_unpack(buf, fw_entry->size,
	                                  &priv->static_config);
	if (rc < 0) {
		dev_err(dev, "Invalid static configuration data\n");
		goto err_mem;
	}

	/* Perform fixups to the staging area loaded from userspace */
	sja1105_patch_mac_mii_settings(priv);
	sja1105_patch_host_port(priv);

	/* Set some driver private data according to the newly received
	 * configuration from userspace.
	 */
	if (priv->static_config.schedule_entry_points_params_count > 0 &&
	    priv->static_config.schedule_entry_points_params[0].clksrc == 3) {

		u64 i, delta = 0;

		/* TTEthernet scheduling is enabled, and clock source is PTP */
		priv->configured_for_scheduling = true;

		/* Iterate through all TTEthernet timeslots to calculate
		 * total duration of the schedule.
		 */
		for (i = 0; i < priv->static_config.schedule_count; i++)
			delta += priv->static_config.schedule[i].delta;

		u64_to_timespec64(&priv->tas_cycle_len, delta * 200);
	} else
		priv->configured_for_scheduling = false;

err_mem:
	kfree(buf);
err_fw:
	release_firmware(fw_entry);
err_out:
	return rc;
}


static void sja1105_cleanup(struct sja1105_spi_private *priv)
{
	struct list_head *pos, *q;
	struct sja1105_port *port;

	sja1105_sysfs_remove(priv);
	sja1105_ptp_clock_unregister(priv);

	list_for_each_safe(pos, q, &(priv->port_list_head.list)) {
		port = list_entry(pos, struct sja1105_port, list);
		if (port) {
			dev_dbg(&priv->spi_dev->dev,
			        "Cleaning up port %s\n",
			        port->net_dev->name);
			if (!IS_ERR(port->reset_gpio))
				gpiod_put(port->reset_gpio);
			if (port->phy_dev && port->phy_dev->attached_dev)
				phy_disconnect(port->phy_dev);
			if (of_phy_is_fixed_link(port->node))
				of_phy_deregister_fixed_link(port->node);
			sja1105_netdev_remove_port(port);
			list_del(pos);
		}
	}

	if (!IS_ERR(priv->reset_gpio))
		gpiod_put(priv->reset_gpio);

	kfree(priv);
}

static int sja1105_parse_dt(struct sja1105_spi_private *priv)
{
	struct device       *dev = &priv->spi_dev->dev;
	struct device_node  *switch_node = dev->of_node;
	struct device_node  *switch_host_port_node = NULL;
	struct device_node  *host_port_node;
	struct device_node  *child;
	const char          *port_name;
	struct sja1105_port *port;
	phy_interface_t     phy_mode;
	u32 port_index;
	int rc, k = 0;

	rc = of_property_read_string(switch_node, "sja1105,staging-area",
	                             &priv->staging_area);
	if (rc) {
		static const char def_name[] = "sja1105.bin";
		dev_err(dev, "Staging area node not present in device tree, trying %s\n",
		        def_name);
		priv->staging_area = def_name;
	}

	host_port_node = of_parse_phandle(switch_node, "sja1105,host-port-connection", 0);
	if (host_port_node) {
		priv->host_net_dev = of_find_net_device_by_node(host_port_node);
		of_node_put(host_port_node);
		if (!priv->host_net_dev) {
			dev_err(dev, "Could not find host port by phandle!\n");
			goto err_out;
		}
		switch_host_port_node = of_parse_phandle(switch_node,
				"sja1105,host-port-connection", 1);
		if (!switch_host_port_node) {
			dev_err(dev, "External host netdev specified, but not "
			        "the SJA1105 host-facing port!\n");
			goto err_out;
		}
	}

	for_each_child_of_node(switch_node, child) {
		rc = of_property_read_string(child, "sja1105,port-label",
		                            &port_name);
		if (rc) {
			dev_err(dev, "Invalid port node in device tree\n");
			goto err_out;
		}

		/* Get switch port number from DT */
		rc = of_property_read_u32(child, "reg", &port_index);
		if (rc) {
			dev_err(dev, "Port number not defined in device tree (property \"reg\")\n");
			goto err_out;
		}

		/* Get PHY mode from DT */
		phy_mode = of_get_phy_mode(child);
		if (phy_mode < 0) {
			dev_err(dev, "Failed to read %s phy-mode or phy-interface-type property\n",
			        port_name);
			goto err_out;
		}

		/* Create a net_device for the port */
		port = sja1105_netdev_create_port(priv, port_name);
		if (IS_ERR(port))
			goto err_out;

		port->index = port_index;
		port->node = child;
		port->phy_mode = phy_mode;

		if (host_port_node && switch_host_port_node == child)
			priv->switch_host_port = port;
		else
			port->mgmt_slot = k++;

		if (of_property_read_bool(child, "sja1105,mac-mode"))
			port->dt_xmii_mode = XMII_MAC;
		else if (of_property_read_bool(child, "sja1105,phy-mode"))
			port->dt_xmii_mode = XMII_PHY;
		else
			port->dt_xmii_mode = -1;

		/* Configure reset pin and bring up PHY */
		port->reset_gpio =
#if KERNEL_VERSION(4, 10, 0) <= LINUX_VERSION_CODE
			devm_fwnode_get_gpiod_from_child(dev, "phy-reset",
			                                &port->node->fwnode,
			                                GPIOD_OUT_HIGH,
			                                port->node->name);
#elif KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE
			devm_get_gpiod_from_child(dev, "phy-reset",
			                          &port->node->fwnode);
#else
			/* TODO there are kernel versions where we are still
			 * incompatible with GPIOD API */
			devm_get_gpiod_from_child(dev, "phy-reset", NULL,
			                          &port->node->fwnode);
#endif
		if (IS_ERR(port->reset_gpio)) {
			dev_dbg(dev, "%s: phy-reset-gpios not defined, ignoring...\n",
			        port_name);
		} else {
			port->reset_duration = 1; /* preset with default value */
			port->reset_delay = 10;   /* preset with default value */
			of_property_read_u32(port->node, "phy-reset-duration",
			                     &port->reset_duration);
			of_property_read_u32(port->node, "phy-reset-delay",
			                     &port->reset_delay);
		}
		port->phy_node = of_parse_phandle(port->node, "phy-handle", 0);
		if (IS_ERR_OR_NULL(port->phy_node)) {
			if (!of_phy_is_fixed_link(port->node)) {
				dev_err(dev, "%s: phy-handle or fixed-link properties missing!\n",
				        port_name);
				goto err_out;
			}
			/* phy-handle is missing, but fixed-link isn't.
			 * So it's a fixed link.
			 */
			if (of_phy_register_fixed_link(port->node) != 0) {
				dev_err(dev, "Failed to register %s as fixed link!\n",
				        port_name);
				goto err_out;
			}
			/* In the case of a fixed PHY, the DT node associated
			 * to the PHY is the Ethernet MAC DT node.
			 */
			port->phy_node = of_node_get(port->node);
		}

		list_add_tail(&(port->list), &(priv->port_list_head.list));
	}

	if (host_port_node)
		of_node_put(switch_host_port_node);

	return 0;

err_out:
	if (host_port_node)
		of_node_put(switch_host_port_node);

	return -ENODEV;
}

static int sja1105_connect_phy(struct sja1105_spi_private *priv)
{
	struct device *dev = &priv->spi_dev->dev;
	struct net_device *net_dev;
	struct sja1105_port *port;
	struct list_head *pos, *q;
	int rc = 0;

	list_for_each_safe(pos, q, &(priv->port_list_head.list)) {
		port = list_entry(pos, struct sja1105_port, list);
		net_dev = port->net_dev;

		/* Release the PHY's hardware reset line */
		if (!IS_ERR(port->reset_gpio))
			sja1105_hw_reset_chip(port->reset_gpio,
			                      port->reset_duration,
			                      port->reset_delay);

		port->phy_dev = of_phy_connect(net_dev, port->phy_node,
		                               sja1105_netdev_adjust_link,
		                               0 /* flags */, port->phy_mode);
		net_dev->phydev = port->phy_dev;
		/* Regardless of error status, decrement refcount now
		 * (of_parse_phandle)
		 */
		of_node_put(port->phy_node);
		/* Never use phy_node again after of_phy_connect complete */
		port->phy_node = NULL;
		if (IS_ERR_OR_NULL(port->phy_dev)) {
			dev_err(dev, "%s: Could not connect to PHY\n",
			        net_dev->name);
			sja1105_netdev_remove_port(port);
			rc = -ENODEV;
			goto out;
		}
		if (of_phy_is_fixed_link(port->node))
			dev_dbg(dev, "Probed %s as fixed link\n", net_dev->name);
		else
			dev_dbg(dev, "Probed %s: %s\n", net_dev->name,
			        port->phy_dev->drv->name);
	}
out:
	return rc;
}

static int sja1105_probe(struct spi_device *spi)
{
	int rc;
	struct device *dev = &spi->dev;
	struct sja1105_spi_private *priv;

	if (!dev->of_node) {
		dev_err(dev, "No DTS bindings for SJA1105 driver\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(struct sja1105_spi_private), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	INIT_LIST_HEAD(&(priv->port_list_head.list));
	mutex_init(&priv->lock);

	mutex_lock(&priv->lock); /* Lock mutex until end of initialization */

	/* Populate our driver private structure (priv) based on
	 * the device tree node that was probed (spi) */
	priv->spi_dev = spi;
	spi_set_drvdata(spi, priv);

	/* Configure the SPI bus */
	spi->mode = SPI_CPHA;
	spi->bits_per_word = 8;
	rc = spi_setup(spi);
	if (rc) {
		dev_err(dev, "Could not init SPI\n");
		goto err_out;
	}

	/* Configure reset pin and bring up switch */
	priv->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->reset_gpio))
		dev_dbg(dev, "reset-gpios (switch reset pin) not defined, ignoring...\n");
	else
		sja1105_hw_reset_chip(priv->reset_gpio, 1, 1);

	/* Probe device */
	rc = sja1105_device_id_get(priv);
	if (rc < 0)
		goto err_out;
	dev_dbg(dev, "Probed switch chip: %s\n", sja1105_device_id_string_get(
	        priv->device_id, priv->part_nr));

	rc = sja1105_ptp_clock_register(priv);
	if (rc < 0)
		goto err_out;

	rc = sja1105_sysfs_init(priv);
	if (rc) {
		dev_err(dev, "Failed to create sysfs entries\n");
		goto err_out;
	}

	/* Parse device tree */
	rc = sja1105_parse_dt(priv);
	if (rc < 0)
		goto err_out;

	/* Load firmware from user space (rootfs) and unpack it */
	rc = sja1105_load_firmware(priv);
	if (rc)
		goto err_out;

	/* Upload static configuration */
	rc = sja1105_static_config_flush_ports_disabled(priv);
	if (rc < 0)
		goto err_out;
	dev_dbg(dev, "Uploaded static configuration to device\n");

	/* Connect and bring up PHYs */
	rc = sja1105_connect_phy(priv);
	if (rc)
		goto err_out;

	mutex_unlock(&priv->lock);
	return 0;
err_out:
	sja1105_cleanup(priv);
	mutex_unlock(&priv->lock);
	return rc;
}

static int sja1105_remove(struct spi_device *spi)
{
	struct sja1105_spi_private *priv = spi_get_drvdata(spi);

	mutex_lock(&priv->lock);
	sja1105_cleanup(priv);
	mutex_unlock(&priv->lock);
	return 0;
}

static const struct of_device_id sja1105_dt_ids[] = {
	{ .compatible = "nxp,sja1105e" },
	{ .compatible = "nxp,sja1105t" },
	{ .compatible = "nxp,sja1105p" },
	{ .compatible = "nxp,sja1105q" },
	{ .compatible = "nxp,sja1105r" },
	{ .compatible = "nxp,sja1105s" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, sja1105_dt_ids);

static struct spi_driver sja1105_driver = {
	.driver = {
		.name  = "sja1105",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sja1105_dt_ids),
	},
	.probe    = sja1105_probe,
	.remove   = sja1105_remove,
};

static int __init sja1105_init(void)
{
	int rc;

	rc = spi_register_driver(&sja1105_driver);
	if (rc < 0)
		printk(KERN_ERR "NXP SJA1105 driver failed to register\n");

	return rc;
}
module_init(sja1105_init);

static void __exit sja1105_exit(void)
{
	spi_unregister_driver(&sja1105_driver);
}
module_exit(sja1105_exit);

