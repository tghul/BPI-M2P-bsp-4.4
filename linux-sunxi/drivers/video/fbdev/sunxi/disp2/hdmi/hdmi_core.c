/*
 * Allwinner SoCs hdmi driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "hdmi_core.h"

static s32		hdmi_state = HDMI_State_Idle;
static u32		video_on;
static u32		audio_on;
static bool		video_enable;
static bool		audio_enable;
static u32		cts_enable;
static u32		hdcp_enable;
static s32		HPD;
static struct audio_para glb_audio_para;
static struct video_para glb_video_para;
static bool audio_cfged;
static HDMI_AUDIO_INFO audio_info;
static struct mutex hdmi_lock;
static s32 audio_config_internal(void);
u32	hdmi_print;
/* 0x10: force unplug; 0x11: force plug; 0x1xx: unreport hpd state */
u32 hdmi_hpd_mask;
static u32 hdmi_detect_time = 200;/* ms */
static bool hdmi_cec_enable;

static s32 video_config(u32 vic);

struct disp_video_timings video_timing[] = {
	{
			.vic = HDMI1440_480I,
			.tv_mode = 0,
			.pixel_clk = 13500000,
			.pixel_repeat = 1,
			.x_res = 720,
			.y_res = 480,
			.hor_total_time = 858,
			.hor_back_porch = 57,
			.hor_front_porch = 19,
			.hor_sync_time = 62,
			.ver_total_time = 525,
			.ver_back_porch = 15,
			.ver_front_porch = 4,
			.ver_sync_time = 3,
			.hor_sync_polarity = 0,
			.ver_sync_polarity = 0,
			.b_interlace = 1,
			.vactive_space = 0,
			.trd_mode = 0,

		},
		{
			.vic = HDMI1440_576I,
			.tv_mode = 0,
			.pixel_clk = 27000000,
			.pixel_repeat = 0,
			.x_res = 720,
			.y_res = 480,
			.hor_total_time = 858,
			.hor_back_porch = 60,
			.hor_front_porch = 16,
			.hor_sync_time = 62,
			.ver_total_time = 525,
			.ver_back_porch = 30,
			.ver_front_porch = 9,
			.ver_sync_time = 6,
			.hor_sync_polarity = 0,
			.ver_sync_polarity = 0,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI480P,
			.tv_mode = 0,
			.pixel_clk = 27000000,
			.pixel_repeat = 0,
			.x_res = 720,
			.y_res = 480,
			.hor_total_time = 858,
			.hor_back_porch = 60,
			.hor_front_porch = 16,
			.hor_sync_time = 62,
			.ver_total_time = 525,
			.ver_back_porch = 30,
			.ver_front_porch = 9,
			.ver_sync_time = 6,
			.hor_sync_polarity = 0,
			.ver_sync_polarity = 0,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI576P,
			.tv_mode = 0,
			.pixel_clk = 27000000,
			.pixel_repeat = 0,
			.x_res = 720,
			.y_res = 576,
			.hor_total_time = 864,
			.hor_back_porch = 68,
			.hor_front_porch = 12,
			.hor_sync_time = 64,
			.ver_total_time = 625,
			.ver_back_porch = 39,
			.ver_front_porch = 5,
			.ver_sync_time = 5,
			.hor_sync_polarity = 0,
			.ver_sync_polarity = 0,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI800_480,
			.tv_mode = 0,
			.pixel_clk = 33900000,
			.pixel_repeat = 0,
			.x_res = 800,
			.y_res = 480,
			.hor_total_time = 1056,
			.hor_back_porch = 124,
			.hor_front_porch = 44,
			.hor_sync_time = 88,
			.ver_total_time = 535,
			.ver_back_porch = 46,
			.ver_front_porch = 3,
			.ver_sync_time = 6,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI400_1280,
			.tv_mode = 0,
			.pixel_clk = 43000000,
			.pixel_repeat = 0,
			.x_res = 400,
			.y_res = 1280,
			.hor_total_time = 540,
			.hor_back_porch = 30,
			.hor_front_porch = 100,
			.hor_sync_time = 10,
			.ver_total_time = 1322,
			.ver_back_porch = 12,
			.ver_front_porch = 20,
			.ver_sync_time = 10,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI1024_600,
			.tv_mode = 0,
			.pixel_clk = 45000000,
			.pixel_repeat = 0,
			.x_res = 1024,
			.y_res = 600,
			.hor_total_time = 1200,
			.hor_back_porch = 96,
			.hor_front_porch = 48,
			.hor_sync_time = 32,
			.ver_total_time = 625,
			.ver_back_porch = 16,
			.ver_front_porch = 3,
			.ver_sync_time = 6,
			.hor_sync_polarity = 0,
			.ver_sync_polarity = 0,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI720P_50,
			.tv_mode = 0,
			.pixel_clk = 74250000,
			.pixel_repeat = 0,
			.x_res = 1280,
			.y_res = 720,
			.hor_total_time = 1980,
			.hor_back_porch = 220,
			.hor_front_porch = 440,
			.hor_sync_time = 40,
			.ver_total_time = 750,
			.ver_back_porch = 20,
			.ver_front_porch = 5,
			.ver_sync_time = 5,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI720P_60,
			.tv_mode = 0,
			.pixel_clk = 74250000,
			.pixel_repeat = 0,
			.x_res = 1280,
			.y_res = 720,
			.hor_total_time = 1650,
			.hor_back_porch = 220,
			.hor_front_porch = 110,
			.hor_sync_time = 40,
			.ver_total_time = 750,
			.ver_back_porch = 20,
			.ver_front_porch = 5,
			.ver_sync_time = 5,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI1280_800,
			.tv_mode = 0,
			.pixel_clk = 67500000,  //69300000
			.pixel_repeat = 0,
			.x_res = 1280,
			.y_res = 800,
			.hor_total_time = 1353,
			.hor_back_porch = 9,
			.hor_front_porch = 16,
			.hor_sync_time = 48,
			.ver_total_time = 854,
			.ver_back_porch = 50,
			.ver_front_porch = 1,
			.ver_sync_time = 3,
			.hor_sync_polarity = 0,
			.ver_sync_polarity = 0,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI1080I_50,
			.tv_mode = 0,
			.pixel_clk = 74250000,
			.pixel_repeat = 0,
			.x_res = 1920,
			.y_res = 1080,
			.hor_total_time = 2640,
			.hor_back_porch = 148,
			.hor_front_porch = 528,
			.hor_sync_time = 44,
			.ver_total_time = 1125,
			.ver_back_porch = 15,
			.ver_front_porch = 2,
			.ver_sync_time = 5,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 1,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI1080I_60,
			.tv_mode = 0,
			.pixel_clk = 74250000,
			.pixel_repeat = 0,
			.x_res = 1920,
			.y_res = 1080,
			.hor_total_time = 2200,
			.hor_back_porch = 148,
			.hor_front_porch = 88,
			.hor_sync_time = 44,
			.ver_total_time = 1125,
			.ver_back_porch = 15,
			.ver_front_porch = 2,
			.ver_sync_time = 5,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 1,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI1080P_50,
			.tv_mode = 0,
			.pixel_clk = 148500000,
			.pixel_repeat = 0,
			.x_res = 1920,
			.y_res = 1080,
			.hor_total_time = 2640,
			.hor_back_porch = 148,
			.hor_front_porch = 528,
			.hor_sync_time = 44,
			.ver_total_time = 1125,
			.ver_back_porch = 36,
			.ver_front_porch = 4,
			.ver_sync_time = 5,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI1080P_60,
			.tv_mode = 0,
			.pixel_clk = 148500000,
			.pixel_repeat = 0,
			.x_res = 1920,
			.y_res = 1080,
			.hor_total_time = 2200,
			.hor_back_porch = 148,
			.hor_front_porch = 88,
			.hor_sync_time = 44,
			.ver_total_time = 1125,
			.ver_back_porch = 36,
			.ver_front_porch = 4,
			.ver_sync_time = 5,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI1080P_24,
			.tv_mode = 0,
			.pixel_clk = 74250000,
			.pixel_repeat = 0,
			.x_res = 1920,
			.y_res = 1080,
			.hor_total_time = 2750,
			.hor_back_porch = 148,
			.hor_front_porch = 638,
			.hor_sync_time = 44,
			.ver_total_time = 1125,
			.ver_back_porch = 36,
			.ver_front_porch = 4,
			.ver_sync_time = 5,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI1080P_25,
			.tv_mode = 0,
			.pixel_clk = 74250000,
			.pixel_repeat = 0,
			.x_res = 1920,
			.y_res = 1080,
			.hor_total_time = 2640,
			.hor_back_porch = 148,
			.hor_front_porch = 528,
			.hor_sync_time = 44,
			.ver_total_time = 1125,
			.ver_back_porch = 36,
			.ver_front_porch = 4,
			.ver_sync_time = 5,
			.hor_sync_polarity = 0,
			.ver_sync_polarity = 0,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI1080P_30,
			.tv_mode = 0,
			.pixel_clk = 74250000,
			.pixel_repeat = 0,
			.x_res = 1920,
			.y_res = 1080,
			.hor_total_time = 2200,
			.hor_back_porch = 148,
			.hor_front_porch = 88,
			.hor_sync_time = 44,
			.ver_total_time = 1125,
			.ver_back_porch = 36,
			.ver_front_porch = 4,
			.ver_sync_time = 5,
			.hor_sync_polarity = 0,
			.ver_sync_polarity = 0,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI1080P_24_3D_FP,
			.tv_mode = 0,
			.pixel_clk = 148500000,
			.pixel_repeat = 0,
			.x_res = 1920,
			.y_res = 2160,
			.hor_total_time = 2750,
			.hor_back_porch = 148,
			.hor_front_porch = 638,
			.hor_sync_time = 44,
			.ver_total_time = 1125,
			.ver_back_porch = 36,
			.ver_front_porch = 4,
			.ver_sync_time = 5,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 45,
			.trd_mode = 1,
		},
		{
			.vic = HDMI720P_50_3D_FP,
			.tv_mode = 0,
			.pixel_clk = 148500000,
			.pixel_repeat = 0,
			.x_res = 1280,
			.y_res = 1440,
			.hor_total_time = 1980,
			.hor_back_porch = 220,
			.hor_front_porch = 440,
			.hor_sync_time = 40,
			.ver_total_time = 750,
			.ver_back_porch = 20,
			.ver_front_porch = 5,
			.ver_sync_time = 5,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 30,
			.trd_mode = 1,
		},
		{
			.vic = HDMI720P_60_3D_FP,
			.tv_mode = 0,
			.pixel_clk = 148500000,
			.pixel_repeat = 0,
			.x_res = 1280,
			.y_res = 1440,
			.hor_total_time = 1650,
			.hor_back_porch = 220,
			.hor_front_porch = 110,
			.hor_sync_time = 40,
			.ver_total_time = 750,
			.ver_back_porch = 20,
			.ver_front_porch = 5,
			.ver_sync_time = 5,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 30,
			.trd_mode = 1,
		},
		{
			.vic = HDMI3840_2160P_30,
			.tv_mode = 0,
			.pixel_clk = 297000000,
			.pixel_repeat = 0,
			.x_res = 3840,
			.y_res = 2160,
			.hor_total_time = 4400,
			.hor_back_porch = 296,
			.hor_front_porch = 176,
			.hor_sync_time = 88,
			.ver_total_time = 2250,
			.ver_back_porch = 72,
			.ver_front_porch = 8,
			.ver_sync_time = 10,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI3840_2160P_25,
			.tv_mode = 0,
			.pixel_clk = 297000000,
			.pixel_repeat = 0,
			.x_res = 3840,
			.y_res = 2160,
			.hor_total_time = 5280,
			.hor_back_porch = 296,
			.hor_front_porch = 1056,
			.hor_sync_time = 88,
			.ver_total_time = 2250,
			.ver_back_porch = 72,
			.ver_front_porch = 8,
			.ver_sync_time = 10,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI3840_2160P_24,
			.tv_mode = 0,
			.pixel_clk = 297000000,
			.pixel_repeat = 0,
			.x_res = 3840,
			.y_res = 2160,
			.hor_total_time = 5500,
			.hor_back_porch = 296,
			.hor_front_porch = 1276,
			.hor_sync_time = 88,
			.ver_total_time = 2250,
			.ver_back_porch = 72,
			.ver_front_porch = 8,
			.ver_sync_time = 10,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
		{
			.vic = HDMI4096_2160P_24,
			.tv_mode = 0,
			.pixel_clk = 297000000,
			.pixel_repeat = 0,
			.x_res = 4096,
			.y_res = 2160,
			.hor_total_time = 5500,
			.hor_back_porch = 296,
			.hor_front_porch = 1020,
			.hor_sync_time = 88,
			.ver_total_time = 2250,
			.ver_back_porch = 72,
			.ver_front_porch = 8,
			.ver_sync_time = 10,
			.hor_sync_polarity = 1,
			.ver_sync_polarity = 1,
			.b_interlace = 0,
			.vactive_space = 0,
			.trd_mode = 0,
		},
};

static void hdmi_para_reset(void)
{
	hdmi_state	  = HDMI_State_Idle;
	video_on = 0;
	audio_on = 0;
	video_enable = 0;
}

static void hdmi_para_init(void)
{
	glb_video_para.vic = HDMI720P_50;
	glb_video_para.csc = BT601;
	glb_video_para.is_hdmi = 1;
	glb_video_para.is_yuv = 0;
	glb_video_para.is_hcts = 0;
	glb_audio_para.type = 1; /* default pcm */
	glb_audio_para.sample_rate = 44100;
	glb_audio_para.sample_bit = 16;
	glb_audio_para.ch_num = 2;
	glb_audio_para.ca = 0;
	audio_enable = 0;
	audio_cfged = false;
}

static s32 hdmi_core_view_on(void)
{
	char buf[2];

	buf[0] = 0;
	buf[1] = HDMI_CEC_IMAGE_VIEW_ON;
	bsp_hdmi_cec_free_time_set(1);
	bsp_hdmi_cec_send(buf, 2);

	return 0;
}

s32 hdmi_core_initial(bool sw_only)
{
	hdmi_bsp_func func;

	func.delay_us = hdmi_delay_us;
	func.delay_ms = hdmi_delay_ms;
	memset(&audio_info, 0, sizeof(HDMI_AUDIO_INFO));
	mutex_init(&hdmi_lock);
	bsp_hdmi_set_version(hdmi_get_soc_version());
	bsp_hdmi_set_func(&func);
#if defined(HDMI_USING_INNER_BIAS)
	bsp_hdmi_set_bias_source(HDMI_USING_INNER_BIAS);
#endif
	hdmi_para_init();
	if (sw_only) {
		hdmi_state = HDMI_STATE_SMOOTH_DISPLAY;
	} else {
		bsp_hdmi_init();
	}

	return 0;
}

void hdmi_core_exit(void)
{
	mutex_destroy(&hdmi_lock);
}

void hdmi_core_set_base_addr(uintptr_t base_addr)
{
	bsp_hdmi_set_addr(base_addr);
}

static s32 main_Hpd_Check(void)
{
	s32 i, times;

	times	= 0;

	for (i = 0; i < 3; i++) {
		mutex_lock(&hdmi_lock);
		if (hdmi_hpd_mask & 0x10)
			times += (hdmi_hpd_mask & 0x1);/* for debug */
		else if (bsp_hdmi_get_hpd())
			times++;
		mutex_unlock(&hdmi_lock);
		if ((cts_enable == 1) && (hdcp_enable == 1))
			hdmi_delay_ms(20);
		else {
			if (hdmi_detect_time != 0)
				hdmi_delay_ms(hdmi_detect_time);
		}
	}

	if (times >= 3)
		return 1;
	else
		return 0;
}

s32 hdmi_core_loop(void)
{
	static u32 times;

	HPD = main_Hpd_Check();
	if (0 == HPD) {
		if ((hdmi_state > HDMI_State_Wait_Hpd) ||
				(hdmi_state == HDMI_State_Idle)) {
			hdmi_inf("plugout\n");
			hdmi_state = HDMI_State_Idle;
			if (video_on == 1)
				hdmi_clk_disable_prepare();
			video_on = 0;
			audio_on = 0;
			if (0 == (hdmi_hpd_mask & 0x100))
				hdmi_hpd_event();
		}

		if ((times++) >= 10) {
			times = 0;
			hdmi_inf("unplug state !!\n");
		}
	}

	switch (hdmi_state) {

	case HDMI_State_Idle:
		hdmi_inf("HDMI_State_Idle\n");
		bsp_hdmi_hrst();
		bsp_hdmi_standby();

		hdmi_state = HDMI_State_Wait_Hpd;
	case HDMI_State_Wait_Hpd:
		hdmi_inf("HDMI_State_Wait_Hpd\n");
		if (HPD) {
			hdmi_state = HDMI_State_EDID_Parse;
			hdmi_inf("plugin\n");
		} else
			return 0;
		if (hdmi_cec_enable)
			hdmi_core_view_on();
		msleep(200);
	case HDMI_State_Rx_Sense:

	case HDMI_State_EDID_Parse:
		hdmi_inf("HDMI_State_EDID_Parse\n");
		mutex_lock(&hdmi_lock);
		hdmi_edid_parse();
		mutex_unlock(&hdmi_lock);
		hdmi_state = HDMI_State_HPD_Done;
		if (0 == (hdmi_hpd_mask & 0x100))
			hdmi_hpd_event();

		if (video_enable)
			hdmi_core_set_video_enable(true);

	case HDMI_State_HPD_Done:
		if (video_on && hdcp_enable)
			bsp_hdmi_hdl();
		return 0;
	case HDMI_STATE_SMOOTH_DISPLAY:
		video_enable = 1;
		hdmi_state = HDMI_State_HPD_Done;
		if (HPD && hdmi_cec_enable)
			hdmi_core_view_on();

		if (HPD) {
			mutex_lock(&hdmi_lock);
			hdmi_edid_parse();
			mutex_unlock(&hdmi_lock);
			video_on = 1;
			if (0 == (hdmi_hpd_mask & 0x100))
				hdmi_hpd_event();
		}
		return 0;
	default:
		hdmi_wrn(" unkonwn hdmi state, set to idle\n");
		hdmi_state = HDMI_State_Idle;
		return 0;
	}
}

s32 hdmi_core_hpd_check(void)
{
	if (hdmi_state >= HDMI_State_HPD_Done)
		return 1;
	else
		return 0;
}

s32 hdmi_core_get_video_info(s32 vic)
{
	s32 i, count;

	count = sizeof(video_timing)/sizeof(struct disp_video_timings);
	for (i = 0; i < count; i++) {
		if (vic == video_timing[i].vic)
			return i;
	}
	hdmi_wrn("can't find the video timing parameters\n");
	return -1;
}

s32 hdmi_core_get_audio_info(s32 sample_rate)
{
	/* ACR_N 32000 44100 48000 88200 96000 176400 192000 */
	/* 4096  6272  6144  12544 12288  25088  24576 */
	hdmi_inf("sample_rate:%d in hdmi_core_get_audio_info\n", sample_rate);

	switch (sample_rate) {
	case 32000:{
		audio_info.ACR_N = 4096;
		audio_info.CH_STATUS0 = (3 << 24);
		audio_info.CH_STATUS1 = 0x0000000b;
		break;
		   }
	case 44100:{
		audio_info.ACR_N = 6272;
		audio_info.CH_STATUS0 = (0 << 24);
		audio_info.CH_STATUS1 = 0x0000000b;
		break;
		   }
	case 48000:{
		audio_info.ACR_N = 6144;
		audio_info.CH_STATUS0 = (2 << 24);
		audio_info.CH_STATUS1 = 0x0000000b;
		break;
		   }
	case 88200:{
		audio_info.ACR_N = 12544;
		audio_info.CH_STATUS0 = (8 << 24);
		audio_info.CH_STATUS1 = 0x0000000b;
		break;
		   }
	case 96000:{
		audio_info.ACR_N = 12288;
		audio_info.CH_STATUS0 = (10<<24);
		audio_info.CH_STATUS1 = 0x0000000b;
		break;
		   }
	case 176400:{
		audio_info.ACR_N = 25088;
		audio_info.CH_STATUS0 = (12<<24);
		audio_info.CH_STATUS1 = 0x0000000b;
		break;
		    }
	case 192000:{
		audio_info.ACR_N = 24576;
		audio_info.CH_STATUS0 = (14<<24);
		audio_info.CH_STATUS1 = 0x0000000b;
		break;
		    }
	default: {
		hdmi_wrn("un-support sample_rate,value=%d\n", sample_rate);
		return -1;
		 }
	}

	if ((glb_video_para.vic == HDMI1440_480I) ||
			(glb_video_para.vic == HDMI1440_576I) ||
			/*(glb_video_para.vic == HDMI480P) || */
			(glb_video_para.vic == HDMI576P)) {
		audio_info.CTS = ((27000000/100) * (audio_info.ACR_N / 128)) /
							(sample_rate/100);
	} else if ((glb_video_para.vic == HDMI720P_50) ||
			(glb_video_para.vic == HDMI720P_60) ||
			(glb_video_para.vic == HDMI1080I_50) ||
			(glb_video_para.vic == HDMI1080I_60) ||
			(glb_video_para.vic == HDMI1080P_24) ||
			(glb_video_para.vic == HDMI1080P_25) ||
			(glb_video_para.vic == HDMI1080P_30)) {
		audio_info.CTS = ((74250000/100) * (audio_info.ACR_N / 128)) /
							(sample_rate/100);
	} else if ((glb_video_para.vic == HDMI1080P_50) ||
			(glb_video_para.vic == HDMI1080P_60) ||
			(glb_video_para.vic == HDMI1080P_24_3D_FP) ||
			(glb_video_para.vic == HDMI720P_50_3D_FP) ||
			(glb_video_para.vic == HDMI720P_60_3D_FP)) {
		audio_info.CTS = ((148500000/100) * (audio_info.ACR_N / 128)) /
							(sample_rate/100);
	} else {
		hdmi_wrn("unkonwn video format when configure audio\n");
		return -1;
	}
	hdmi_inf("audio CTS calc:%d\n", audio_info.CTS);
	return 0;
}

s32 hdmi_core_set_hdcp_enable(u32 enable)
{
	hdcp_enable = enable;
	/* change the hdmi state, video will be reconfig
	 * if it output currently
	 */
	hdmi_state = HDMI_State_Idle;
	if (video_on == 1)
		hdmi_clk_disable_prepare();
	video_on = 0;
	audio_on = 0;

	return 0;
}

u32 hdmi_core_get_hdcp_enable(void)
{
	return hdcp_enable;
}

s32 hdmi_core_set_cts_enable(u32 enable)
{
	cts_enable = enable;

	return 0;
}

u32 hdmi_core_get_cts_enable(void)
{
	return cts_enable;
}

u32 hdmi_core_get_csc_type(void)
{
	int csc = 1;

	if ((hdmi_core_get_cts_enable() == 1) && (hdmi_edid_is_yuv() == 0))
		csc = 0;

	if ((is_exp == 1) &&
		((glb_video_para.vic == HDMI1080P_24)
		|| (glb_video_para.vic == HDMI1080P_24_3D_FP)
		|| (glb_video_para.vic == HDMI3840_2160P_24)
		|| (glb_video_para.vic == HDMI3840_2160P_30)
		|| (glb_video_para.vic == HDMI3840_2160P_25)
		|| (glb_video_para.vic == HDMI4096_2160P_24))
	) {
		csc = 0;
	}

	return csc;
}

s32 hdmi_core_set_audio_enable(bool enable)
{
	int ret = 0;

	mutex_lock(&hdmi_lock);
	hdmi_inf("set_audio_enable = %x!\n", enable);

	if (true == enable)
		audio_cfged = true;
	audio_enable = enable;

	if (((glb_audio_para.type != 1) && (true == audio_enable)) ||
		((glb_audio_para.type == 1) && (audio_cfged == true) &&
		(true == audio_enable))) {
		if (audio_config_internal()) {
			hdmi_wrn("audio_config_internal err!\n");
			ret = -1;
		}
	}
	mutex_unlock(&hdmi_lock);
	return ret;
}

bool hdmi_core_get_audio_enable(void)
{
	bool ret;

	mutex_lock(&hdmi_lock);
	ret = audio_enable;
	mutex_unlock(&hdmi_lock);
	return ret;
}

static s32 audio_config_internal(void)
{
	u8 isHDMI = hdmi_edid_is_hdmi();

	hdmi_inf("audio_config_internal, type code:%d\n", glb_audio_para.type);
	hdmi_inf("audio_config_internal, sample_rate:%d\n",
			glb_audio_para.sample_rate);
	hdmi_inf("audio_config_internal, sample_bit:%d\n",
			glb_audio_para.sample_bit);
	hdmi_inf("audio_config_internal, channel_num:%d\n",
			glb_audio_para.ch_num);
	hdmi_inf("audio_config_internal, channel allocation:%d\n",
			glb_audio_para.ca);

	if (video_on) {
		hdmi_inf("audio_config_internal when video on");
		if ((cts_enable == 1) && (isHDMI == 0)) {
			hdmi_inf("sink is not hdmi, not sending audio\n");
			return 0;
		}

		if (bsp_hdmi_audio(&glb_audio_para)) {
			hdmi_wrn("set hdmi audio error!\n");
			return -1;
	  }

	  audio_on = 1;
	} else
		hdmi_inf("audio_config_internal when video off");

	return 0;
}

s32 hdmi_core_audio_config(hdmi_audio_t *audio_param)
{
	int ret = 0;

	mutex_lock(&hdmi_lock);
	hdmi_inf("hdmi_core_audio_config\n");

	glb_audio_para.type = audio_param->data_raw;
	glb_audio_para.sample_rate = audio_param->sample_rate;
	glb_audio_para.sample_bit = audio_param->sample_bit;
	glb_audio_para.ch_num = audio_param->channel_num;
	glb_audio_para.ca = audio_param->ca;

	mutex_unlock(&hdmi_lock);
	return ret;
}

u32 hdmi_core_get_video_mode(void)
{
	u32 ret;

	mutex_lock(&hdmi_lock);
	if (video_enable == 0)
		ret = 0;
	else
		ret =  glb_video_para.vic;
	mutex_unlock(&hdmi_lock);
	return ret;
}

s32 hdmi_core_set_video_mode(u32 vic)
{
	u32 ret = 0;

	hdmi_inf("hdmi_core_set_video_mode = %x\n", vic);
	mutex_lock(&hdmi_lock);
	glb_video_para.vic = vic;
	glb_audio_para.vic = vic;
	mutex_unlock(&hdmi_lock);

	return ret;
}

s32 hdmi_core_set_video_enable(bool enable)
{
	int ret = 0;

	mutex_lock(&hdmi_lock);
	hdmi_inf("hdmi_core_set_video_enable enable=%x, video_on=%d!\n",
			enable, video_on);
	if ((hdmi_state == HDMI_State_HPD_Done) && enable && (video_on == 0)) {
		video_config(glb_video_para.vic);
		hdmi_inf("vic:%d,is_hdmi:%d,is_yuv:%d,is_hcts:%d\n",
			glb_video_para.vic, glb_video_para.is_hdmi,
			glb_video_para.is_yuv, glb_video_para.is_hcts);
		if (bsp_hdmi_video(&glb_video_para)) {
			hdmi_wrn("set hdmi video error!\n");
			ret = -1;
			goto video_en_end;
		}

		hdmi_clk_enable_prepare();
		bsp_hdmi_set_video_en(enable);
		video_on = 1;

		if (((glb_audio_para.type != 1) && (true == audio_enable)) ||
			((glb_audio_para.type == 1) && (audio_cfged == true))) {
			if (audio_config_internal()) {
				hdmi_wrn("set audio_config_internal error!\n");
				ret = -1;
				goto video_en_end;
			}
		}
	} else {
		if ((video_on == 1) && (!enable)) {
			bsp_hdmi_set_video_en(enable);
			hdmi_clk_disable_prepare();
		}
		video_on = 0;
	}

	video_enable = enable;

video_en_end:
	mutex_unlock(&hdmi_lock);
	return ret;
}

bool hdmi_core_get_video_enable(void)
{
	bool ret;

	mutex_lock(&hdmi_lock);
	ret = video_enable;
	mutex_unlock(&hdmi_lock);
	return ret;
}

s32 hdmi_core_get_list_num(void)
{
	return sizeof(video_timing)/sizeof(struct disp_video_timings);
}

static s32 video_config(u32 vic)
{
	int ret = 0;
	u8 isHDMI = hdmi_edid_is_hdmi();
	u8 YCbCr444_Support = hdmi_edid_is_yuv();
	struct disp_video_timings *info;
	int i;

	hdmi_inf("vic:%d,cts_enable:%d,isHDMI:%d,YCbCr444:%d,hdcp_enable:%d\n",
		vic, cts_enable, isHDMI, YCbCr444_Support, hdcp_enable);

	glb_video_para.vic = vic;
	if ((cts_enable == 1) && (isHDMI == 0))
		glb_video_para.is_hdmi = 0;
	else
		glb_video_para.is_hdmi = 1;

	glb_video_para.is_yuv = hdmi_core_get_csc_type();

	if (hdcp_enable) {
		glb_video_para.is_hcts = 1;
		bsp_hdmi_hrst();
		hdmi_inf("hdmi full function\n");
	} else {
		glb_video_para.is_hcts = 0;
		hdmi_inf("hdmi video + audio\n");
	}

	info = &video_timing[0];
	for (i = 0; i < ARRAY_SIZE(video_timing); i++) {
		if (info->vic == vic) {
			glb_video_para.pixel_clk = info->pixel_clk;
			glb_video_para.clk_div = hdmi_clk_get_div();
			glb_video_para.pixel_repeat = info->pixel_repeat;
			glb_video_para.x_res = info->x_res;
			glb_video_para.y_res = info->y_res;
			glb_video_para.hor_total_time = info->hor_total_time;
			glb_video_para.hor_back_porch = info->hor_back_porch;
			glb_video_para.hor_front_porch = info->hor_front_porch;
			glb_video_para.hor_sync_time = info->hor_sync_time;
			glb_video_para.ver_total_time = info->ver_total_time;
			glb_video_para.ver_back_porch = info->ver_back_porch;
			glb_video_para.ver_front_porch = info->ver_front_porch;
			glb_video_para.ver_sync_time = info->ver_sync_time;
			glb_video_para.hor_sync_polarity =
			    info->hor_sync_polarity;
			glb_video_para.ver_sync_polarity =
			    info->ver_sync_polarity;
			glb_video_para.b_interlace = info->b_interlace;
			break;
		}
		info++;
	}

	if (i >= ARRAY_SIZE(video_timing))
		hdmi_wrn("cant found proper video timing for vic %d\n", vic);

	hdmi_inf("video_on @ video_config = %d!\n", video_on);

	return ret;
}

s32 hdmi_core_enter_lp(void)
{
	hdmi_inf("video enter lp\n");

	hdmi_state = HDMI_State_Idle;
	bsp_hdmi_standby();
	hdmi_para_reset();

	return 0;
}

s32 hdmi_core_exit_lp(void)
{
	bsp_hdmi_init();

	return 0;
}

s32 hdmi_core_mode_support(u32 mode)
{
	if (hdmi_core_hpd_check() == 0)
		return 0;

	return Device_Support_VIC[mode];
}

s32 hdmi_core_dvi_support(void)
{
	if ((hdmi_edid_is_hdmi() == 0) && (hdmi_core_get_cts_enable() == 1))
		return 1;
	else
		return 0;
}

s32 hdmi_core_cec_enable(bool enable)
{
	hdmi_cec_enable = enable;

	return 0;
}

s32 hdmi_core_update_detect_time(u32 time_val)
{
	hdmi_detect_time = time_val;

	return 0;
}

int hdmi_core_cec_get_simple_msg(unsigned char *msg)
{
	int ret = -1;

	mutex_lock(&hdmi_lock);
	ret = bsp_hdmi_cec_get_simple_msg(msg);
	mutex_unlock(&hdmi_lock);

	return ret;
}
