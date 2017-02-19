/******************************************************************************
 * Copyright (c) 2016, NXP Semiconductors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/
#include "internal.h"
#include "external.h"

int sja1105_config_add_entry(struct sja1105_table_header *hdr, void *buf, struct sja1105_config *config)
{
	switch (hdr->block_id) {
	case BLKID_SCHEDULE_TABLE:
	{
		struct sja1105_schedule_entry entry;
		int count;

		count = config->schedule_count;
		if (count >= MAX_SCHEDULE_COUNT) {
			printf("There can be no more than %d Schedule Table entries\n",
			       MAX_SCHEDULE_COUNT);
			return -1;
		}
		sja1105_schedule_entry_get(buf, &entry);
		config->schedule[count++] = entry;
		config->schedule_count = count;
		return SIZE_SCHEDULE_ENTRY;
	}
	case BLKID_SCHEDULE_ENTRY_POINTS_TABLE:
	{
		struct sja1105_schedule_entry_points_entry entry;
		int count;

		count = config->schedule_entry_points_count;
		if (count >= MAX_SCHEDULE_ENTRY_POINTS_COUNT) {
			printf("There can be no more than %d Schedule Entry Points entries\n",
			       MAX_SCHEDULE_ENTRY_POINTS_COUNT);
			return -1;
		}
		sja1105_schedule_entry_points_entry_get(buf, &entry);
		config->schedule_entry_points[count++] = entry;
		config->schedule_entry_points_count = count;
		return SIZE_SCHEDULE_ENTRY_POINTS_ENTRY;
	}
	case BLKID_VL_LOOKUP_TABLE:
		printf("VL Lookup Table Unimplemented\n");
		return SIZE_VL_LOOKUP_ENTRY;
	case BLKID_VL_POLICING_TABLE:
		printf("VL Policing Table Unimplemented\n");
		return SIZE_VL_POLICING_ENTRY;
	case BLKID_VL_FORWARDING_TABLE:
		printf("VL Forwarding Table Unimplemented\n");
		return SIZE_VL_FORWARDING_ENTRY;
	case BLKID_L2_LOOKUP_TABLE:
	{
		struct sja1105_l2_lookup_entry entry;
		int count;

		count = config->l2_lookup_count;
		if (count >= MAX_L2_LOOKUP_COUNT) {
			printf("There can be no more than %d L2 Lookup Table entries\n",
			       MAX_L2_LOOKUP_COUNT);
			return -1;
		}
		sja1105_l2_lookup_entry_get(buf, &entry);
		config->l2_lookup[config->l2_lookup_count++] = entry;
		return SIZE_L2_LOOKUP_ENTRY;
	}
	case BLKID_L2_POLICING_TABLE:
	{
		struct sja1105_l2_policing_entry entry;
		int count;

		count = config->l2_policing_count;
		if (count >= MAX_L2_POLICING_COUNT) {
			printf("There can be no more than %d L2 Policing Table entries\n",
			       MAX_L2_POLICING_COUNT);
			return -1;
		}
		sja1105_l2_policing_entry_get(buf, &entry);
		config->l2_policing[count++] = entry;
		config->l2_policing_count = count;
		return SIZE_L2_POLICING_ENTRY;
	}
	case BLKID_VLAN_LOOKUP_TABLE:
	{
		struct sja1105_vlan_lookup_entry entry;
		int count;

		count = config->vlan_lookup_count;
		if (count >= MAX_VLAN_LOOKUP_COUNT) {
			printf("There can be no more than %d VLAN Lookup Table entries\n",
			       MAX_VLAN_LOOKUP_COUNT);
			return -1;
		}
		sja1105_vlan_lookup_entry_get(buf, &entry);
		config->vlan_lookup[count++] = entry;
		config->vlan_lookup_count = count;
		return SIZE_VLAN_LOOKUP_ENTRY;
	}
	case BLKID_L2_FORWARDING_TABLE:
	{
		struct sja1105_l2_forwarding_entry entry;
		int count;

		count = config->l2_forwarding_count;
		if (count >= MAX_L2_FORWARDING_COUNT) {
			printf("There can be no more than %d L2 Forwarding Table entries\n",
			       MAX_L2_FORWARDING_PARAMS_COUNT);
			return -1;
		}
		sja1105_l2_forwarding_entry_get(buf, &entry);
		config->l2_forwarding[count++] = entry;
		config->l2_forwarding_count = count;
		return SIZE_L2_FORWARDING_ENTRY;
	}
	case BLKID_MAC_CONFIG_TABLE:
	{
		struct sja1105_mac_config_entry entry;
		int count;

		count = config->mac_config_count;
		if (count >= MAX_MAC_CONFIG_COUNT) {
			printf("There can be no more than %d MAC Config Table entries\n",
			       MAX_MAC_CONFIG_COUNT);
			return -1;
		}
		sja1105_mac_config_entry_get(buf, &entry);
		config->mac_config[count++] = entry;
		config->mac_config_count = count;
		return SIZE_MAC_CONFIG_ENTRY;
	}
	case BLKID_SCHEDULE_PARAMS_TABLE:
	{
		struct sja1105_schedule_params_entry entry;
		int count;

		count = config->schedule_params_count;
		if (count >= MAX_SCHEDULE_PARAMS_COUNT) {
			printf("There can be no more than %d Schedule Params Table entries\n",
			       MAX_SCHEDULE_PARAMS_COUNT);
			return -1;
		}
		sja1105_schedule_params_entry_get(buf, &entry);
		config->schedule_params[count++] = entry;
		config->schedule_params_count = count;
		return SIZE_SCHEDULE_PARAMS_ENTRY;
	}
	case BLKID_SCHEDULE_ENTRY_POINTS_PARAMS_TABLE:
	{
		struct sja1105_schedule_entry_points_params entry;
		int count;

		count = config->schedule_entry_points_params_count;
		if (count >= MAX_SCHEDULE_ENTRY_POINTS_PARAMS_COUNT) {
			printf("There can be no more than %d Schedule Entry Points Params Table entries\n",
			       MAX_SCHEDULE_ENTRY_POINTS_PARAMS_COUNT);
			return -1;
		}
		sja1105_schedule_entry_points_params_get(buf, &entry);
		config->schedule_entry_points_params[count++] = entry;
		config->schedule_entry_points_params_count = count;
		return SIZE_SCHEDULE_ENTRY_POINTS_PARAMS_ENTRY;
	}
	case BLKID_VL_FORWARDING_PARAMS_TABLE:
		printf("VL Forwarding Parameters Table Unimplemented\n");
		return SIZE_VL_FORWARDING_PARAMS_ENTRY;
	case BLKID_L2_LOOKUP_PARAMS_TABLE:
	{
		struct sja1105_l2_lookup_params_table table;
		int count;

		count = config->l2_lookup_params_count;
		if (count >= MAX_L2_LOOKUP_PARAMS_COUNT) {
			printf("There can be no more than %d L2 Lookup Params Table\n",
			       MAX_L2_LOOKUP_PARAMS_COUNT);
			return -1;
		}
		sja1105_l2_lookup_params_table_get(buf, &table);
		config->l2_lookup_params[count++] = table;
		config->l2_lookup_params_count = count;
		return SIZE_L2_LOOKUP_PARAMS_TABLE;
	}
	case BLKID_L2_FORWARDING_PARAMS_TABLE:
	{
		struct sja1105_l2_forwarding_params_table table;
		int count;

		count = config->l2_forwarding_params_count;
		if (count >= MAX_L2_FORWARDING_PARAMS_COUNT) {
			printf("There can be no more than %d L2 Forwarding Params Table\n",
			       MAX_L2_FORWARDING_PARAMS_COUNT);
			return -1;
		}
		sja1105_l2_forwarding_params_table_get(buf, &table);
		config->l2_forwarding_params[count++] = table;
		config->l2_forwarding_params_count = count;
		return SIZE_L2_FORWARDING_PARAMS_TABLE;
	}
	case BLKID_CLK_SYNC_PARAMS_TABLE:
		printf("Clock Synchronization Parameters Table Unimplemented\n");
		return SIZE_CLK_SYNC_PARAMS_TABLE;
	case BLKID_AVB_PARAMS_TABLE:
		printf("AVB Parameters Table Unimplemented\n");
		return SIZE_AVB_PARAMS_TABLE;
	case BLKID_GENERAL_PARAMS_TABLE:
	{
		struct sja1105_general_params_table table;
		int count;

		count = config->general_params_count;
		if (count >= MAX_GENERAL_PARAMS_COUNT) {
			printf("There can be no more than %d General Params Table\n",
			       MAX_GENERAL_PARAMS_COUNT);
			return -1;
		}
		sja1105_general_params_table_get(buf, &table);
		config->general_params[count++] = table;
		config->general_params_count = count;
		return SIZE_GENERAL_PARAMS_TABLE;
	}
	case BLKID_RETAGGING_TABLE:
		printf("Retagging Table Unimplemented\n");
		return SIZE_RETAGGING_ENTRY;
	case BLKID_XMII_MODE_PARAMS_TABLE:
	{
		struct sja1105_xmii_params_table table;
		int count;

		count = config->xmii_params_count;
		if (count >= MAX_XMII_PARAMS_COUNT) {
			printf("There can be no more than %d xMII Params Table\n",
			       MAX_XMII_PARAMS_COUNT);
			return -1;
		}
		sja1105_xmii_params_table_get(buf, &table);
		config->xmii_params[count++] = table;
		config->xmii_params_count = count;
		return SIZE_XMII_MODE_PARAMS_TABLE;
	}
	default:
		printf("Unknown Table %" PRIX64 "\n", hdr->block_id);
		return -1;
	}
	return 0;
}

int sja1105_config_hexdump(void *buf)
{
	struct sja1105_table_header hdr;
	struct sja1105_config config;
	char *p = buf;
	char *table_end;
	int bytes;

	memset(&config, 0, sizeof(config));
	while (1) {
		sja1105_table_header_get(p, &hdr);
		/* This should match on last table header */
		if (hdr.len == 0) {
			break;
		}
		sja1105_table_header_show(&hdr);
		printf("Header:\n");
		generic_table_hexdump(p, SIZE_TABLE_HEADER);
		p += SIZE_TABLE_HEADER;

		table_end = p + hdr.len * 4;
		while (p < table_end) {
			bytes = sja1105_config_add_entry(&hdr, p, &config);
			if (bytes < 0) {
				goto error;
			}
			printf("Entry (%d bytes):\n", bytes);
			generic_table_hexdump(p, bytes);
			p += bytes;
		};
		if (p != table_end) {
			fprintf(stderr, "WARNING: Incorrect table length specified in header!\n");
			printf("Extra:\n");
			generic_table_hexdump(p, (int) (table_end - p));
			p = table_end;
		}
		printf("Table Data CRC:\n");
		generic_table_hexdump(p, 4);
		p += 4;
		printf("\n");
	}
	return 0;
error:
	return -1;
}

int sja1105_config_get(void *buf, struct sja1105_config *config)
{
	struct sja1105_table_header hdr;
	char *p = buf;
	char *table_end;
	int bytes;
	uint64_t read_crc;
	uint64_t computed_crc;

	memset(config, 0, sizeof(*config));
	while (1) {
		sja1105_table_header_get(p, &hdr);
		/* This should match on last table header */
		if (hdr.len == 0) {
			break;
		}
		if (general_config.verbose) {
			sja1105_table_header_show(&hdr);
		}
		computed_crc = ether_crc32_le(p, SIZE_TABLE_HEADER - 4);
		computed_crc &= 0xFFFFFFFF;
		read_crc = hdr.crc & 0xFFFFFFFF;
		if (read_crc != computed_crc) {
			fprintf(stderr, "Table header CRC is invalid, exiting.\n");
			fprintf(stderr, "Read %" PRIX64 ", computed %" PRIX64 "\n",
			        read_crc, computed_crc);
			goto error;
		}
		p += SIZE_TABLE_HEADER;

		table_end = p + hdr.len * 4;
		computed_crc = ether_crc32_le(p, hdr.len * 4);
		while (p < table_end) {
			bytes = sja1105_config_add_entry(&hdr, p, config);
			if (bytes < 0) {
				goto error;
			}
			p += bytes;
		};
		if (p != table_end) {
			fprintf(stderr, "WARNING: Incorrect table length specified in header!\n");
			p = table_end;
		}
		generic_table_field_get(p, &read_crc, 31, 0, 4);
		p += 4;
		if (computed_crc != read_crc) {
			fprintf(stderr, "Data CRC is invalid, exiting.\n");
			fprintf(stderr, "Read %" PRIX64 ", computed %" PRIX64 "\n",
			        read_crc, computed_crc);
			goto error;
		}
	}
	return 0;
error:
	return -1;
}

int sja1105_config_set(void *buf, struct sja1105_config *config)
{
	struct sja1105_table_header header;
	char *p = buf;
	char *table_start;
	int i;

	memset(&header, 0, sizeof(header));
	if (config->schedule_count) {
		header.block_id = BLKID_SCHEDULE_TABLE;
		header.len = config->schedule_count * SIZE_SCHEDULE_ENTRY / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->schedule_count; i++) {
			sja1105_schedule_entry_set(p, &config->schedule[i]);
			p += SIZE_SCHEDULE_ENTRY;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	if (config->schedule_entry_points_count) {
		header.block_id = BLKID_SCHEDULE_ENTRY_POINTS_TABLE;
		header.len = config->schedule_entry_points_count * SIZE_SCHEDULE_ENTRY_POINTS_ENTRY / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->schedule_entry_points_count; i++) {
			sja1105_schedule_entry_points_entry_set(p, &config->schedule_entry_points[i]);
			p += SIZE_SCHEDULE_ENTRY_POINTS_ENTRY;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	if (config->l2_lookup_count) {
		header.block_id = BLKID_L2_LOOKUP_TABLE;
		header.len = config->l2_lookup_count * SIZE_L2_LOOKUP_ENTRY / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->l2_lookup_count; i++) {
			sja1105_l2_lookup_entry_set(p, &config->l2_lookup[i]);
			p += SIZE_L2_LOOKUP_ENTRY;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	if (config->l2_policing_count) {
		header.block_id = BLKID_L2_POLICING_TABLE;
		header.len = config->l2_policing_count * SIZE_L2_POLICING_ENTRY / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->l2_policing_count; i++) {
			sja1105_l2_policing_entry_set(p, &config->l2_policing[i]);
			p += SIZE_L2_POLICING_ENTRY;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	if (config->vlan_lookup_count) {
		header.block_id = BLKID_VLAN_LOOKUP_TABLE;
		header.len = config->vlan_lookup_count * SIZE_VLAN_LOOKUP_ENTRY / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->vlan_lookup_count; i++) {
			sja1105_vlan_lookup_entry_set(p, &config->vlan_lookup[i]);
			p += SIZE_VLAN_LOOKUP_ENTRY;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	if (config->l2_forwarding_count) {
		header.block_id = BLKID_L2_FORWARDING_TABLE;
		header.len = config->l2_forwarding_count * SIZE_L2_FORWARDING_ENTRY / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->l2_forwarding_count; i++) {
			sja1105_l2_forwarding_entry_set(p, &config->l2_forwarding[i]);
			p += SIZE_L2_FORWARDING_ENTRY;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	if (config->mac_config_count) {
		header.block_id = BLKID_MAC_CONFIG_TABLE;
		header.len = config->mac_config_count * SIZE_MAC_CONFIG_ENTRY / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->mac_config_count; i++) {
			sja1105_mac_config_entry_set(p, &config->mac_config[i]);
			p += SIZE_MAC_CONFIG_ENTRY;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	if (config->schedule_params_count) {
		header.block_id = BLKID_SCHEDULE_PARAMS_TABLE;
		header.len = config->schedule_params_count * SIZE_SCHEDULE_PARAMS_ENTRY / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->schedule_params_count; i++) {
			sja1105_schedule_params_entry_set(p, &config->schedule_params[i]);
			p += SIZE_SCHEDULE_PARAMS_ENTRY;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	if (config->schedule_entry_points_params_count) {
		header.block_id = BLKID_SCHEDULE_ENTRY_POINTS_PARAMS_TABLE;
		header.len = config->schedule_entry_points_params_count * SIZE_SCHEDULE_ENTRY_POINTS_PARAMS_ENTRY / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->schedule_entry_points_params_count; i++) {
			sja1105_schedule_entry_points_params_set(p, &config->schedule_entry_points_params[i]);
			p += SIZE_SCHEDULE_ENTRY_POINTS_PARAMS_ENTRY;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	if (config->l2_lookup_params_count) {
		header.block_id = BLKID_L2_LOOKUP_PARAMS_TABLE;
		header.len = config->l2_lookup_params_count * SIZE_L2_LOOKUP_PARAMS_TABLE / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->l2_lookup_params_count; i++) {
			sja1105_l2_lookup_params_table_set(p, &config->l2_lookup_params[i]);
			p += SIZE_L2_LOOKUP_PARAMS_TABLE;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	if (config->l2_forwarding_params_count) {
		header.block_id = BLKID_L2_FORWARDING_PARAMS_TABLE;
		header.len = config->l2_forwarding_params_count * SIZE_L2_FORWARDING_PARAMS_TABLE / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->l2_forwarding_params_count; i++) {
			sja1105_l2_forwarding_params_table_set(p, &config->l2_forwarding_params[i]);
			p += SIZE_L2_FORWARDING_PARAMS_TABLE;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	if (config->general_params_count) {
		header.block_id = BLKID_GENERAL_PARAMS_TABLE;
		header.len = config->general_params_count * SIZE_GENERAL_PARAMS_TABLE / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->general_params_count; i++) {
			sja1105_general_params_table_set(p, &config->general_params[i]);
			p += SIZE_GENERAL_PARAMS_TABLE;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	if (config->xmii_params_count) {
		header.block_id = BLKID_XMII_MODE_PARAMS_TABLE;
		header.len = config->xmii_params_count * SIZE_XMII_MODE_PARAMS_TABLE / 4;
		sja1105_table_header_set_with_crc(p, &header);
		p += SIZE_TABLE_HEADER;
		table_start = p;
		for (i = 0; i < config->xmii_params_count; i++) {
			sja1105_xmii_params_table_set(p, &config->xmii_params[i]);
			p += SIZE_XMII_MODE_PARAMS_TABLE;
		}
		sja1105_table_write_crc(table_start, p);
		p += 4;
	}
	/* Final header */
	header.block_id = 0;      /* Does not matter */
	header.len = 0;           /* Marks that header is final */
	header.crc = 0xDEADBEEF;  /* Will be replaced on-the-fly on "config upload" */
	sja1105_table_header_set(p, &header);
	return 0;
}

unsigned int sja1105_config_get_length(struct sja1105_config *config)
{
	unsigned int sum = 0;
	unsigned int header_count = 0;

	/* Table headers */
	header_count += (config->schedule_count != 0);
	header_count += (config->schedule_entry_points_count != 0);
	header_count += (config->l2_lookup_count != 0);
	header_count += (config->l2_policing_count != 0);
	header_count += (config->vlan_lookup_count != 0);
	header_count += (config->l2_forwarding_count != 0);
	header_count += (config->mac_config_count != 0);
	header_count += (config->schedule_params_count != 0);
	header_count += (config->schedule_entry_points_params_count != 0);
	header_count += (config->l2_lookup_params_count != 0);
	header_count += (config->l2_forwarding_params_count != 0);
	header_count += (config->general_params_count != 0);
	header_count += (config->xmii_params_count != 0);
	header_count += 1; /* Ending header */
	sum += header_count * (SIZE_TABLE_HEADER + 4); /* plus CRC at the end */
	sum += config->schedule_count * SIZE_SCHEDULE_ENTRY;
	sum += config->schedule_entry_points_count * SIZE_SCHEDULE_ENTRY_POINTS_ENTRY;
	sum += config->l2_lookup_count * SIZE_L2_LOOKUP_ENTRY;
	sum += config->l2_policing_count * SIZE_L2_POLICING_ENTRY;
	sum += config->vlan_lookup_count * SIZE_VLAN_LOOKUP_ENTRY;
	sum += config->l2_forwarding_count * SIZE_L2_FORWARDING_ENTRY;
	sum += config->mac_config_count * SIZE_MAC_CONFIG_ENTRY;
	sum += config->schedule_params_count * SIZE_SCHEDULE_PARAMS_ENTRY;
	sum += config->schedule_entry_points_params_count * SIZE_SCHEDULE_ENTRY_POINTS_PARAMS_ENTRY;
	sum += config->l2_lookup_params_count * SIZE_L2_LOOKUP_PARAMS_TABLE;
	sum += config->l2_forwarding_params_count * SIZE_L2_FORWARDING_PARAMS_TABLE;
	sum += config->general_params_count * SIZE_GENERAL_PARAMS_TABLE;
	sum += config->xmii_params_count * SIZE_XMII_MODE_PARAMS_TABLE;
	sum -= 4; /* Last header does not have an extra CRC because there is no data */
	if (general_config.verbose) {
		printf("total: %d bytes\n", sum);
	}
	return sum;
}