/*
 * libdevice-node
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE
#include <hw/board.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define MMC_ID_PATH "/sys/class/mmc_host/mmc0/mmc0:0001/cid"
#define SERIAL_OFFSET 16
#define SERIAL_LEN 16
#define LINE_LEN (SERIAL_OFFSET + SERIAL_LEN + 2)

static int get_device_serial(char **out)
{
	FILE *fp;
	char line[LINE_LEN], *p;

	fp = fopen(MMC_ID_PATH, "r");
	if (!fp)
		return -1;

	p = fgets(line, LINE_LEN, fp);
	fclose(fp);
	if (p == NULL)
		return -1;

	*out = strndup(line + SERIAL_OFFSET, SERIAL_LEN);
	return 0;
}

static int board_open(struct hw_info *info,
		const char *id, struct hw_common **common)
{
	struct hw_board *b;

	if (!info || !common)
		return -EINVAL;

	b = calloc(1, sizeof(*b));
	if (!b)
		return -ENOMEM;

	b->common.info = info;
	b->get_device_serial = get_device_serial;

	*common = &b->common;
	return 0;
}

static int board_close(struct hw_common *common)
{
	struct hw_board *b;

	if (!common)
		return -EINVAL;

	b = container_of(common, struct hw_board, common);
	free(b);

	return 0;
}

HARDWARE_MODULE_STRUCTURE = {
	.magic = HARDWARE_INFO_TAG,
	.hal_version = HARDWARE_INFO_VERSION,
	.device_version = BOARD_HARDWARE_DEVICE_VERSION,
	.id = BOARD_HARDWARE_DEVICE_ID,
	.name = "device",
	.open = board_open,
	.close = board_close,
};
