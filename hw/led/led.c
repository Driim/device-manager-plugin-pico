/*
 * device-node
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/limits.h>
#include <glib.h>

#include <hw/led.h>
#include <hw/shared.h>

#ifndef CAMERA_BACK_PATH
#define CAMERA_BACK_PATH	"/sys/class/leds/ktd2692-flash"
#endif

#ifndef TOUCH_KEY_PATH
#define TOUCH_KEY_PATH          "/sys/class/leds/sec_touchkey"
#endif

#define GET_BRIGHTNESS(val)     (((val) >> 24) & 0xFF)

/* LED Notification (RGB) */
#define NOTI_COLOR_TYPE_PATH "/sys/class/leds/led.%d/color"
#define NOTI_COLOR_BRT_PATH "/sys/class/leds/led.%d/brightness"

#define GET_TYPE(a)       (((a) >> 24) & 0xFF)
#define GET_RED_ONLY(a)   ((a) & 0xFF0000)
#define GET_GREEN_ONLY(a) ((a) & 0x00FF00)
#define GET_BLUE_ONLY(a)  ((a) & 0x0000FF)
#define GET_RED_BRT(a)    (((a) >> 16) & 0xFF)
#define GET_GREEN_BRT(a)  (((a) >> 8) & 0xFF)
#define GET_BLUE_BRT(a)   ((a) & 0xFF)

typedef enum _led_rgb_type {
	LED_RED,
	LED_GREEN,
	LED_BLUE,
} led_rgb_type_e;

struct led_notification_node {
	char *name;
	led_rgb_type_e type;
	char *path;
	int brt;
} led_noti_nodes[] = {
	{ "RED",   LED_RED,   NULL, 0 },
	{ "GREEN", LED_GREEN, NULL, 0 },
	{ "BLUE",  LED_BLUE,  NULL, 0 },
};

struct notification_play_color_info {
	unsigned int color;
	int time;
};

struct notification_play_info {
	GList *play_list;
	int nr_play;
	int index;
	guint timer;
} play_info;


static int camera_back_set_state(struct led_state *state)
{
	static int max = -1;
	int brt, r;

	if (!state) {
		_E("wrong parameter");
		return -EINVAL;
	}

	if (state->type == LED_TYPE_BLINK) {
		_E("camera back led does not support LED_TYPE_BLINK mode");
		return -ENOTSUP;
	}

	/* if there is a max brightness of led */
	if (max < 0) {
		r = sys_get_int(CAMERA_BACK_PATH"/max_brightness", &max);
		if (r < 0) {
			_E("fail to get max brightness (errno:%d)", r);
			return r;
		}
	}

	brt = (state->color >> 24) & 0xFF;
	brt = brt / 255.f * max;

	r = sys_set_int(CAMERA_BACK_PATH"/brightness", brt);
	if (r < 0) {
		_E("fail to set brightness (errno:%d)", r);
		return r;
	}

	return 0;
}

static int touch_key_set_state(struct led_state *state)
{
	static int max = -1;
	int brt, r;

	if (!state)
		return -EINVAL;

	if (state->type == LED_TYPE_BLINK)
		return -ENOTSUP;

	/* if there is a max brightness of led */
	if (max < 0) {
		r = sys_get_int(TOUCH_KEY_PATH"/max_brightness", &max);
		if (r < 0)
			return r;
	}

	brt = GET_BRIGHTNESS(state->color);
	brt = brt / 255.f * max;

	r = sys_set_int(TOUCH_KEY_PATH"/brightness", brt);
	if (r < 0)
		return r;

	return 0;
}

/* Find sysfs nodes for RGB*/
static void notification_get_path(int index)
{
	char path[PATH_MAX];
	char buf[8];
	FILE *fp;
	int i;
	size_t len;

	snprintf(path, sizeof(path), NOTI_COLOR_TYPE_PATH, index);
	fp = fopen(path, "r");
	if (!fp)
		return;
	len = fread(buf, 1, sizeof(buf) - 1, fp);
	fclose(fp);
	if (len == 0) {
		_E("Failed to get contents");
		return;
	}
	buf[len] = '\0';

	for (i = 0 ; i < ARRAY_SIZE(led_noti_nodes) ; i++) {
		if (strncmp(buf, led_noti_nodes[i].name, strlen(led_noti_nodes[i].name)))
			continue;
		snprintf(path, sizeof(path), NOTI_COLOR_BRT_PATH, index);
		led_noti_nodes[i].path = strdup(path);
	}
}

static int notification_init_led(void)
{
	int i;

	notification_get_path(1);
	notification_get_path(2);
	notification_get_path(3);

	for (i = 0 ; i < ARRAY_SIZE(led_noti_nodes) ; i++)
		_I("NOTI LED %s (%s)", led_noti_nodes[i].name, led_noti_nodes[i].path);

	return 0;
}

/* Change led color and brightness */
static int notification_set_brightness(struct led_state *state)
{
	int i;
	int err, ret;
	int brt;
	unsigned int red, green, blue;

	if (!state)
		return -EINVAL;

	red = GET_RED_BRT(state->color);
	green = GET_GREEN_BRT(state->color);
	blue = GET_BLUE_BRT(state->color);

	_I("COLOR(%x) r(%x), g(%x), b(%x)", state->color, red, green, blue);

	err = 0;
	for (i = 0 ; i < ARRAY_SIZE(led_noti_nodes) ; i++) {
		if (!led_noti_nodes[i].path)
			continue;
		if (led_noti_nodes[i].type == LED_RED)
			brt = red;
		else if (led_noti_nodes[i].type == LED_GREEN)
			brt = green;
		else if (led_noti_nodes[i].type == LED_BLUE)
			brt = blue;
		else
			continue;

		ret = sys_set_int(led_noti_nodes[i].path, brt);
		if (ret < 0) {
			_E("Failed to change brt of led (%s) to (%d)(ret:%d)", led_noti_nodes[i].name, brt, ret);
			err = ret;
		}
	}

	return err;
}

/* turn off led */
static int notification_turn_off(struct led_state *state)
{
	struct led_state st = { 0, };
	return notification_set_brightness(&st);
}

/* release play list */
static void free_func(gpointer data)
{
	struct notification_play_color_info *color = data;
	free(color);
}

static void release_play_info(void)
{
	if (play_info.play_list) {
		g_list_free_full(play_info.play_list, free_func);
		play_info.play_list = NULL;
	}
	play_info.nr_play = 0;
	play_info.index = 0;
	if (play_info.timer) {
		g_source_remove(play_info.timer);
		play_info.timer = 0;
	}

	notification_turn_off(NULL);
}

/* timer callback to change colors which are stored in play_list */
static gboolean notification_timer_expired(gpointer data)
{
	struct notification_play_color_info *color;
	struct led_state state = { 0, };
	int ret;

	if (play_info.timer) {
		g_source_remove(play_info.timer);
		play_info.timer = 0;
	}

	color = g_list_nth_data(play_info.play_list, play_info.index);
	if (!color) {
		_E("Failed to get (%d)th item from the play list", play_info.index);
		goto out;
	}

	play_info.timer = g_timeout_add(color->time, notification_timer_expired, data);
	if (play_info.timer == 0) {
		_E("Failed to add timeout for LED blinking");
		goto out;
	}

	state.color = color->color;

	ret = notification_set_brightness(&state);
	if (ret < 0) {
		_E("Failed to set brightness (%d)", ret);
		goto out;
	}

	play_info.index++;
	if (play_info.index == play_info.nr_play)
		play_info.index = 0;

	return G_SOURCE_CONTINUE;

out:
	release_play_info();

	return G_SOURCE_REMOVE;
}

/* insert color info to the play_list */
static int notification_insert_play_list(unsigned color, int on, int off)
{
	struct notification_play_color_info *on_info, *off_info;

	if (color == 0)
		return -EINVAL;
	on_info = calloc(1, sizeof(struct notification_play_color_info));
	if (!on_info)
		return -ENOMEM;
	off_info = calloc(1, sizeof(struct notification_play_color_info));
	if (!off_info) {
		free(on_info);
		return -ENOMEM;
	}

	on_info->color = color;
	on_info->time = on;
	play_info.play_list = g_list_append(play_info.play_list, on_info);

	off_info->color = 0;
	off_info->time = off;
	play_info.play_list = g_list_append(play_info.play_list, off_info);

	return 0;
}

/* insert color info to the play list and start to play */
static int notification_set_brightness_blink(struct led_state *state)
{
	unsigned int val;
	int ret;

	val = GET_RED_ONLY(state->color);
	if (val) {
		ret = notification_insert_play_list(val, state->duty_on, state->duty_off);
		if (ret < 0)
			_E("Failed to insert color info to list (%d)", ret);
	}
	val = GET_GREEN_ONLY(state->color);
	if (val) {
		ret = notification_insert_play_list(val, state->duty_on, state->duty_off);
		if (ret < 0)
			_E("Failed to insert color info to list (%d)", ret);
	}
	val = GET_BLUE_ONLY(state->color);
	if (val) {
		ret = notification_insert_play_list(val, state->duty_on, state->duty_off);
		if (ret < 0)
			_E("Failed to insert color info to list (%d)", ret);
	}

	play_info.nr_play = g_list_length(play_info.play_list);
	play_info.index = 0;

	notification_timer_expired(NULL);
	return 0;
}

/* turn on led notification */
static int notification_turn_on(struct led_state *state)
{
	if (state->type == LED_TYPE_MANUAL)
		return notification_set_brightness(state);

	return notification_set_brightness_blink(state);
}

static int notification_set_state(struct led_state *state)
{
	if (!state)
		return -EINVAL;

	switch (state->type) {
	case LED_TYPE_BLINK:
	case LED_TYPE_MANUAL:
		break;
	default:
		_E("Not suppoted type (%d)", state->type);
		return -ENOTSUP;
	}

	release_play_info();

	if (GET_TYPE(state->color) == 0)
		return notification_turn_off(state);

	return notification_turn_on(state);
}

static int led_open(struct hw_info *info,
		const char *id, struct hw_common **common)
{
	struct led_device *led_dev;
	size_t len;

	if (!info || !id || !common)
		return -EINVAL;

	led_dev = calloc(1, sizeof(struct led_device));
	if (!led_dev)
		return -ENOMEM;

	led_dev->common.info = info;

	len = strlen(id) + 1;
	if (!strncmp(id, LED_ID_CAMERA_BACK, len))
		led_dev->set_state = camera_back_set_state;

	else if (!strncmp(id, LED_ID_TOUCH_KEY, len))
		led_dev->set_state = touch_key_set_state;

	else if (!strncmp(id, LED_ID_NOTIFICATION, len)) {
		notification_init_led();
		led_dev->set_state = notification_set_state;

	} else {
		free(led_dev);
		return -ENOTSUP;
	}

	*common = (struct hw_common *)led_dev;
	return 0;
}

static int led_close(struct hw_common *common)
{
	if (!common)
		return -EINVAL;

	free(common);
	return 0;
}

HARDWARE_MODULE_STRUCTURE = {
	.magic = HARDWARE_INFO_TAG,
	.hal_version = HARDWARE_INFO_VERSION,
	.device_version = LED_HARDWARE_DEVICE_VERSION,
	.id = LED_HARDWARE_DEVICE_ID,
	.name = "Default LED",
	.author = "Jiyoung Yun <jy910.yun@samsung.com>",
	.open = led_open,
	.close = led_close,
};
