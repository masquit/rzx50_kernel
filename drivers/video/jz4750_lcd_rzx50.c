/*
 * linux/drivers/video/jz4750_lcd.c -- Ingenic Jz4750 LCD frame buffer device
 *
 * Copyright (C) 2005-2008, Ingenic Semiconductor Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * --------------------------------
 * NOTE:
 * This LCD driver support TFT16 TFT32 LCD, not support STN and Special TFT LCD 
 * now.
 * 	It seems not necessory to support STN and Special TFT.
 * 	If it's necessary, update this driver in the future.
 * 	<Wolfgang Wang, Jun 10 2008>
 */

/*
 * Mod: <maddrone@gmail.com>
 * */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_legacy.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>

#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/processor.h>
#include <asm/jzsoc.h>


#include "console/fbcon.h"

#include "jz4750_lcd_rzx50.h"
#include "jz4750_tve.h"

#ifdef CONFIG_JZ4750_SLCD_KGM701A3_TFT_SPFD5420A
#include "jz_kgm_spfd5420a.h"
#endif

#ifdef CONFIG_JZ4750_SLCD_L009_ILI9331  
#include "umido_slcd_ILI9331.h"
#endif

#ifdef CONFIG_JZ4750_SLCD_L009_ILI9325  
#include "umido_slcd_ILI9331.h"
#endif

//maddrone add
//#include "bootpic.h"
//#include "L009_scale.h"
#include "lockpic.h"

//maddrone add
extern unsigned char *vmfbmem_addr;
extern u32 phy_vmfbmem_addr;
static int tvout_640_480 = 0;


MODULE_DESCRIPTION("Jz4750 LCD Controller driver");
MODULE_AUTHOR("Wolfgang Wang, <lgwang@ingenic.cn>");
MODULE_LICENSE("GPL");

//0: lcd   1 : 480X272->640X480    2:640X480 
static unsigned long tvout_flag  = 0;

static int resize_go_out = 0;
static int tvout_display_w = 320;
static int tvout_display_h = 240;

static void display_h_color_bar(int w, int h, int bpp);
//#define DEBUG
#undef DEBUG
#define TVOUT_2x

//#define DEBUG_L009

#ifdef DEBUG_L009
#define lprint_dbg(f, arg...) printk("dbg::" __FILE__ ",LINE(%d): " f "\n", __LINE__, ## arg)
#else
#define lprint_dbg(f, arg...) do {} while (0)
#endif



#ifdef DEBUG
#define dprintk(x...)	printk(x)
#define print_dbg(f, arg...) printk("dbg::" __FILE__ ",LINE(%d): " f "\n", __LINE__, ## arg)
#else
#define dprintk(x...)
#define print_dbg(f, arg...) do {} while (0)
#endif

#define print_err(f, arg...) printk(KERN_ERR DRIVER_NAME ": " f "\n", ## arg)
#define print_warn(f, arg...) printk(KERN_WARNING DRIVER_NAME ": " f "\n", ## arg)
#define print_info(f, arg...) printk(KERN_INFO DRIVER_NAME ": " f "\n", ## arg)

struct lcd_cfb_info {
	struct fb_info		fb;
	struct display_switch	*dispsw;
	signed int		currcon;
	int			func_use_count;

	struct {
		u16 red, green, blue;
	} palette[NR_PALETTE];
#ifdef CONFIG_PM
	struct pm_dev		*pm;
#endif
};

// write 1 to /proc/jz/lcd_flush to start SLCD Flush
// write 0 to /proc/jz/lcd_flush to stop SLCD Flush
static unsigned int lcd_flush_flag = 1;
static unsigned int lcd_a320_flag = 0;


static struct lcd_cfb_info *jz4750fb_info;
static struct jz4750_lcd_dma_desc *dma_desc_base;
static struct jz4750_lcd_dma_desc *dma0_desc_palette, *dma0_desc0, *dma0_desc1, *dma1_desc0, *dma1_desc1;
#define DMA_DESC_NUM 		6

static unsigned char *lcd_palette;
static unsigned char *lcd_frame0, *lcd_frame01;
static unsigned char *lcd_frame1;

static struct jz4750_lcd_dma_desc *dma0_desc_cmd0, *dma0_desc_cmd;
static unsigned char *lcd_cmdbuf ;

static void jz4750fb_set_mode( struct jz4750lcd_info * lcd_info );
static void jz4750fb_deep_set_mode( struct jz4750lcd_info * lcd_info );


struct jz4750lcd_info jz4750_lcd_panel = {
#if defined(CONFIG_JZ4750_LCD_SAMSUNG_LTP400WQF02)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */ 
		LCD_CFG_NEWDES | /* 8words descriptor */
		LCD_CFG_MODE_GENERIC_TFT | /* General TFT panel */
		LCD_CFG_MODE_TFT_18BIT | 	/* output 18bpp */
		LCD_CFG_HSP | 	/* Hsync polarity: active low */
		LCD_CFG_VSP,	/* Vsync polarity: leading edge is falling edge */
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16,	/* 16words burst, enable out FIFO underrun irq */
		480, 272, 60, 41, 10, 2, 2, 2, 2,
	},
	.osd = {
		 .osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
//		 LCD_OSDC_ALPHAEN | /* enable alpha */
		 LCD_OSDC_F0EN,	/* enable Foreground0 */
		 .osd_ctrl = 0,		/* disable ipu,  */
		 .rgb_ctrl = 0,
		 .bgcolor = 0x000000, /* set background color Black */
		 .colorkey0 = 0, /* disable colorkey */
		 .colorkey1 = 0, /* disable colorkey */
		 .alpha = 0xA0,	/* alpha value */
		 .ipu_restart = 0x80001000, /* ipu restart */
		 .fg_change = FG_CHANGE_ALL, /* change all initially */
		 .fg0 = {32, 0, 0, 480, 272}, /* bpp, x, y, w, h */
		 .fg1 = {32, 0, 0, 480, 272}, /* bpp, x, y, w, h */
	 },
#elif defined(CONFIG_JZ4750_LCD_AUO_A043FL01V2)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */ 
		LCD_CFG_NEWDES | /* 8words descriptor */
		LCD_CFG_MODE_GENERIC_TFT | /* General TFT panel */
		LCD_CFG_MODE_TFT_24BIT | 	/* output 18bpp */
		LCD_CFG_HSP | 	/* Hsync polarity: active low */
		LCD_CFG_VSP,	/* Vsync polarity: leading edge is falling edge */
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16,	/* 16words burst, enable out FIFO underrun irq */
		480, 272, 60, 41, 10, 8, 4, 4, 2,
	},
	.osd = {
		 .osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
//		 LCD_OSDC_ALPHAEN | /* enable alpha */
//		 LCD_OSDC_F1EN | /* enable Foreground1 */
		 LCD_OSDC_F0EN,	/* enable Foreground0 */
		 .osd_ctrl = 0,		/* disable ipu,  */
		 .rgb_ctrl = 0,
		 .bgcolor = 0x000000, /* set background color Black */
		 .colorkey0 = 0, /* disable colorkey */
		 .colorkey1 = 0, /* disable colorkey */
		 .alpha = 0xA0,	/* alpha value */
		 .ipu_restart = 0x80001000, /* ipu restart */
		 .fg_change = FG_CHANGE_ALL, /* change all initially */
		 .fg0 = {16, 0, 0, 480, 272}, /* bpp, x, y, w, h */
		 .fg1 = {16, 0, 0, 480, 272}, /* bpp, x, y, w, h */
	 },
#elif defined(CONFIG_JZ4750_LCD_TOPPOLY_TD043MGEB1)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */ 
		LCD_CFG_NEWDES | /* 8words descriptor */
		LCD_CFG_MODE_GENERIC_TFT | /* General TFT panel */
		LCD_CFG_MODE_TFT_24BIT | 	/* output 18bpp */
		LCD_CFG_HSP | 	/* Hsync polarity: active low */
		LCD_CFG_VSP,	/* Vsync polarity: leading edge is falling edge */
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16,	/* 16words burst, enable out FIFO underrun irq */
		800, 480, 60, 1, 1, 40, 215, 10, 34,
	},
	.osd = {
		 .osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
//		 LCD_OSDC_ALPHAEN | /* enable alpha */
//		 LCD_OSDC_F1EN | /* enable Foreground1 */
		 LCD_OSDC_F0EN,	/* enable Foreground0 */
		 .osd_ctrl = 0,		/* disable ipu,  */
		 .rgb_ctrl = 0,
		 .bgcolor = 0xff, /* set background color Black */
		 .colorkey0 = 0, /* disable colorkey */
		 .colorkey1 = 0, /* disable colorkey */
		 .alpha = 0xA0,	/* alpha value */
		 .ipu_restart = 0x80001000, /* ipu restart */
		 .fg_change = FG_CHANGE_ALL, /* change all initially */
		 .fg0 = {32, 0, 0, 800, 480}, /* bpp, x, y, w, h */
		 .fg1 = {32, 0, 0, 800, 480}, /* bpp, x, y, w, h */
	 },
#elif defined(CONFIG_JZ4750_LCD_TRULY_TFT_GG1P0319LTSW_W)
	.panel = {
		 .cfg = LCD_CFG_LCDPIN_SLCD | /* Underrun recover*/ 
		 LCD_CFG_NEWDES | /* 8words descriptor */
		 LCD_CFG_MODE_SLCD, /* TFT Smart LCD panel */
		 .slcd_cfg = SLCD_CFG_DWIDTH_16BIT | SLCD_CFG_CWIDTH_16BIT | SLCD_CFG_CS_ACTIVE_LOW | SLCD_CFG_RS_CMD_LOW | SLCD_CFG_CLK_ACTIVE_FALLING | SLCD_CFG_TYPE_PARALLEL,
		 .ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16,	/* 16words burst, enable out FIFO underrun irq */
		 240, 320, 60, 0, 0, 0, 0, 0, 0,
	 },
	.osd = {
		 .osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
//		 LCD_OSDC_ALPHAEN | /* enable alpha */
//		 LCD_OSDC_F1EN | /* enable Foreground0 */
		 LCD_OSDC_F0EN,	/* enable Foreground0 */
		 .osd_ctrl = 0,		/* disable ipu,  */
		 .rgb_ctrl = 0,
		 .bgcolor = 0x000000, /* set background color Black */
		 .colorkey0 = 0, /* disable colorkey */
		 .colorkey1 = 0, /* disable colorkey */
		 .alpha = 0xA0,	/* alpha value */
		 .ipu_restart = 0x80001000, /* ipu restart */
		 .fg_change = FG_CHANGE_ALL, /* change all initially */
		 .fg0 = {32, 0, 0, 240, 320}, /* bpp, x, y, w, h */
		 .fg1 = {32, 0, 0, 240, 320}, /* bpp, x, y, w, h */
	 },

#elif defined(CONFIG_JZ4750_LCD_FOXCONN_PT035TN01)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */ 
		LCD_CFG_NEWDES | /* 8words descriptor */
		LCD_CFG_MODE_GENERIC_TFT | /* General TFT panel */
//		LCD_CFG_MODE_TFT_18BIT | 	/* output 18bpp */
		LCD_CFG_MODE_TFT_24BIT | 	/* output 24bpp */
		LCD_CFG_HSP | 	/* Hsync polarity: active low */
		LCD_CFG_VSP |	/* Vsync polarity: leading edge is falling edge */
		LCD_CFG_PCP,	/* Pix-CLK polarity: data translations at falling edge */
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16,	/* 16words burst, enable out FIFO underrun irq */
		320, 240, 80, 1, 1, 10, 50, 10, 13
	},
	.osd = {
		 .osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
//		 LCD_OSDC_ALPHAEN | /* enable alpha */
//		 LCD_OSDC_F1EN |	/* enable Foreground1 */
		 LCD_OSDC_F0EN,	/* enable Foreground0 */
		 .osd_ctrl = 0,		/* disable ipu,  */
		 .rgb_ctrl = 0,
		 .bgcolor = 0x000000, /* set background color Black */
		 .colorkey0 = 0, /* disable colorkey */
		 .colorkey1 = 0, /* disable colorkey */
		 .alpha = 0xA0,	/* alpha value */
		 .ipu_restart = 0x80001000, /* ipu restart */
		 .fg_change = FG_CHANGE_ALL, /* change all initially */
		 .fg0 = {32, 0, 0, 320, 240}, /* bpp, x, y, w, h */
		 .fg1 = {32, 0, 0, 320, 240}, /* bpp, x, y, w, h */
	 },
#elif defined(CONFIG_JZ4750_LCD_INNOLUX_PT035TN01_SERIAL)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */ 
		LCD_CFG_NEWDES | /* 8words descriptor */
		LCD_CFG_MODE_GENERIC_TFT | /* Serial TFT panel */
		LCD_CFG_MODE_TFT_24BIT | 	/* output 24bpp */
		LCD_CFG_HSP | 	/* Hsync polarity: active low */
		LCD_CFG_VSP ,	/* Vsync polarity: leading edge is falling edge */
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16,	/* 16words burst, enable out FIFO underrun irq */
                480, 272,  40, 1, 1, 40, 215, 0, 45,
	},
	.osd = {
		 .osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
//		 LCD_OSDC_ALPHAEN | /* enable alpha */
		 LCD_OSDC_F0EN,	/* enable Foreground0 */
		 .osd_ctrl = 0,		/* disable ipu,  */
		 .rgb_ctrl = 0,
		 .bgcolor = 0x000000, /* set background color Black */
		 .colorkey0 = 0, /* disable colorkey */
		 .colorkey1 = 0, /* disable colorkey */
		 .alpha = 0xA0,	/* alpha value */
		 .ipu_restart = 0x80001000, /* ipu restart */
		 .fg_change = FG_CHANGE_ALL, /* change all initially */
		 .fg0 = {16, 0, 0, 480, 272}, /* bpp, x, y, w, h */
		 .fg1 = {32, 0, 0, 480, 272}, /* bpp, x, y, w, h */
	 },
#elif defined(CONFIG_JZ4750_SLCD_KGM701A3_TFT_SPFD5420A)
	.panel = {
//		 .cfg = LCD_CFG_LCDPIN_SLCD | LCD_CFG_RECOVER | /* Underrun recover*/ 
		 .cfg = LCD_CFG_LCDPIN_SLCD | /* Underrun recover*/ 
//		 LCD_CFG_DITHER | /* dither */
		 LCD_CFG_NEWDES | /* 8words descriptor */
		 LCD_CFG_MODE_SLCD, /* TFT Smart LCD panel */
		 .slcd_cfg = SLCD_CFG_DWIDTH_18BIT | SLCD_CFG_CWIDTH_18BIT | SLCD_CFG_CS_ACTIVE_LOW | SLCD_CFG_RS_CMD_LOW | SLCD_CFG_CLK_ACTIVE_FALLING | SLCD_CFG_TYPE_PARALLEL,
		 .ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16,	/* 16words burst, enable out FIFO underrun irq */
		 400, 240, 60, 0, 0, 0, 0, 0, 0,
	 },
	.osd = {
		 .osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
//		 LCD_OSDC_ALPHAEN | /* enable alpha */
//		 LCD_OSDC_ALPHAMD | /* alpha blending mode */
//		 LCD_OSDC_F1EN | /* enable Foreground1 */
		 LCD_OSDC_F0EN,	/* enable Foreground0 */
		 .osd_ctrl = 0,		/* disable ipu,  */
		 .rgb_ctrl = 0,
		 .bgcolor = 0x000000, /* set background color Black */
		 .colorkey0 = 0, /* disable colorkey */
		 .colorkey1 = 0, /* disable colorkey */
		 .alpha = 0xA0,	/* alpha value */
		 .ipu_restart = 0x80001000, /* ipu restart */
		 .fg_change = FG_CHANGE_ALL, /* change all initially */
//		 .fg0 = {32, 0, 0, 400, 240}, /* bpp, x, y, w, h */
		 .fg0 = {32, 0, 0, 320, 240}, /* bpp, x, y, w, h */
		 .fg1 = {32, 0, 0, 400, 240}, /* bpp, x, y, w, h */
	 },
#elif defined(CONFIG_JZ4750_SLCD_L009_ILI9331)  
	.panel = {
//		 .cfg = LCD_CFG_LCDPIN_SLCD | LCD_CFG_RECOVER | /* Underrun recover*/ 
		 .cfg = LCD_CFG_LCDPIN_SLCD | /* Underrun recover*/ 
//		 LCD_CFG_DITHER | /* dither */
		 LCD_CFG_NEWDES | /* 8words descriptor */
		 LCD_CFG_MODE_SLCD, /* TFT Smart LCD panel */
		 .slcd_cfg = SLCD_CFG_DWIDTH_8BIT_x2 | SLCD_CFG_CWIDTH_8BIT | SLCD_CFG_CS_ACTIVE_LOW | SLCD_CFG_RS_CMD_LOW | SLCD_CFG_CLK_ACTIVE_FALLING | SLCD_CFG_TYPE_PARALLEL,
		 //.ctrl = LCD_CTRL_BST_16 | LCD_CTRL_BPP_18_24 | LCD_CTRL_OFUM,	/* 16words burst, enable out FIFO underrun irq */
		 //.ctrl = LCD_CTRL_BST_16 | LCD_CTRL_OFUM,	/* 16words burst, enable out FIFO underrun irq */
		 .ctrl = LCD_CTRL_BST_16 | LCD_CTRL_BPP_16 | LCD_CTRL_OFUM,	/* 16words burst, enable out FIFO underrun irq */
		 400, 240, 200, 0, 0, 0, 0, 0, 0,
	 },
	.osd = {
		 .osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
//		 LCD_OSDC_ALPHAEN | /* enable alpha */
//		 LCD_OSDC_ALPHAMD | /* alpha blending mode */
//		 LCD_OSDC_F1EN | /* enable Foreground1 */
		 LCD_OSDC_F0EN,	/* enable Foreground0 */
		 .osd_ctrl = 0,		/* disable ipu,  */
		 .rgb_ctrl = 0,
		 .bgcolor = 0x0000FF, /* set background color Black */
		 .colorkey0 = 0, /* disable colorkey */
		 .colorkey1 = 0, /* disable colorkey */
		 .alpha = 0x00,	/* alpha value */
		 .ipu_restart = 0x80001000, /* ipu restart */
		 .fg_change = FG_CHANGE_ALL, /* change all initially */
		 //.fg0 = {16, 0, 0, 720, 540}, /* bpp, x, y, w, h */
		 .fg1 = {32, 0, 0, 320, 240}, /* bpp, x, y, w, h */
		 //.fg0 = {32, 0, 0, 320, 240}, /* bpp, x, y, w, h */
		 .fg0 = {16, 0, 0, 320, 240}, /* bpp, x, y, w, h */
	 },
#elif defined(CONFIG_JZ4750D_VGA_DISPLAY)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER |/* Underrun recover */ 
		LCD_CFG_NEWDES | /* 8words descriptor */
		LCD_CFG_MODE_GENERIC_TFT | /* General TFT panel */
		LCD_CFG_MODE_TFT_24BIT | 	/* output 18bpp */
		LCD_CFG_HSP | 	/* Hsync polarity: active low */
		LCD_CFG_VSP,	/* Vsync polarity: leading edge is falling edge */
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16,	/* 16words burst, enable out FIFO underrun irq */
//		800, 600, 60, 128, 4, 40, 88, 0, 23
		640, 480, 54, 96, 2, 16, 48, 10, 33
//		1280, 720, 50, 152, 15, 22, 200, 14, 1
	},
	.osd = {
		 .osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
//		 LCD_OSDC_ALPHAEN | /* enable alpha */
//		 LCD_OSDC_F1EN | /* enable Foreground1 */
		 LCD_OSDC_F0EN,	/* enable Foreground0 */
		 .osd_ctrl = 0,		/* disable ipu,  */
		 .rgb_ctrl = 0,
		 .bgcolor = 0x000000, /* set background color Black */
		 .colorkey0 = 0, /* disable colorkey */
		 .colorkey1 = 0, /* disable colorkey */
		 .alpha = 0xA0,	/* alpha value */
		 .ipu_restart = 0x80001000, /* ipu restart */
		 .fg_change = FG_CHANGE_ALL, /* change all initially */
		 .fg0 = {32, 0, 0, 640, 480}, /* bpp, x, y, w, h */
		 .fg1 = {32, 0, 0, 640, 480}, /* bpp, x, y, w, h */
	 },
#else
#error "Select LCD panel first!!!"
#endif
};

#if 0
struct jz4750lcd_info jz4750_info_tve = {
	.panel = {
		.cfg = LCD_CFG_TVEN | /* output to tve */
		LCD_CFG_NEWDES | /* 8words descriptor */
		LCD_CFG_RECOVER | /* underrun protect */
		LCD_CFG_MODE_INTER_CCIR656, /* Interlace CCIR656 mode */
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16,	/* 16words burst */
		TVE_WIDTH_PAL, TVE_HEIGHT_PAL, TVE_FREQ_PAL, 0, 0, 0, 0, 0, 0,
	},
	.osd = {
		 .osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
//		 LCD_OSDC_ALPHAEN | /* enable alpha */
		 LCD_OSDC_F0EN,	/* enable Foreground0 */
		 .osd_ctrl = 0,		/* disable ipu,  */
		 .rgb_ctrl = LCD_RGBC_YCC, /* enable RGB => YUV */
		 .bgcolor = 0x00000000, /* set background color Black */
		 .colorkey0 = 0, /* disable colorkey */
		 .colorkey1 = 0, /* disable colorkey */
		 .alpha = 0xA0,	/* alpha value */
		 .ipu_restart = 0x80000100, /* ipu restart */
		 .fg_change = FG_CHANGE_ALL, /* change all initially */
		 .fg0 = {32,},	/*  */
		 .fg0 = {32,},
	},
};
#endif
struct jz4750lcd_info jz4750_info_tve = {
	.panel = {
		.cfg = LCD_CFG_TVEN | /* output to tve */
		LCD_CFG_NEWDES | /* 8words descriptor */
		LCD_CFG_TVEPEH | 
		//LCD_CFG_RECOVER | /* underrun protect */
		LCD_CFG_MODE_INTER_CCIR656, /* Interlace CCIR656 mode */
		//.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16,	/* 16words burst */
		//.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16,	/* 16words burst */
		.ctrl = LCD_CTRL_BST_16,	/* 16words burst */
		//TVE_WIDTH_PAL, TVE_HEIGHT_PAL, TVE_FREQ_PAL, 0, 0, 0, 0, 0, 0,
                TVE_WIDTH_NTSC , TVE_HEIGHT_NTSC, TVE_FREQ_NTSC, 0, 0, 0, 0, 0, 0,
	},
	.osd = {
		 .osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
//		 LCD_OSDC_ALPHAEN | /* enable alpha */
		 LCD_OSDC_F0EN,	/* enable Foreground0 */
		 .osd_ctrl = 0,		/* disable ipu,  */
		 .rgb_ctrl = LCD_RGBC_YCC, /* enable RGB => YUV */
		 //.rgb_ctrl = 0, /* enable RGB => YUV */
		 //.bgcolor = 0x008080, /* set background color Black */
		 .bgcolor = 0x0, /* set background color Black */
		 .colorkey0 = 0, /* disable colorkey */
		 .colorkey1 = 0, /* disable colorkey */
		 .alpha = 0x30,	/* alpha value */
		 .ipu_restart = 0x80000100, /* ipu restart */
		 .fg_change = FG_CHANGE_ALL, /* change all initially */
                 //.fg0 = {16,0,0,TVE_WIDTH_PAL,TVE_HEIGHT_PAL},	/*  */
                 .fg0 = {16,0,0,TVE_WIDTH_NTSC,TVE_HEIGHT_NTSC},	/*  */
		 .fg1 = {16,0,0,0,0},
	},
};


struct jz4750lcd_info *jz4750_lcd_info = &jz4750_lcd_panel; /* default output to lcd panel */

#ifdef  DEBUG
static void print_lcdc_registers(void)	/* debug */
{
	/* LCD Controller Resgisters */
	printk("REG_LCD_CFG:\t0x%08x\n", REG_LCD_CFG);
	printk("REG_LCD_CTRL:\t0x%08x\n", REG_LCD_CTRL);
	printk("REG_LCD_STATE:\t0x%08x\n", REG_LCD_STATE);
	printk("REG_LCD_OSDC:\t0x%08x\n", REG_LCD_OSDC);
	printk("REG_LCD_OSDCTRL:\t0x%08x\n", REG_LCD_OSDCTRL);
	printk("REG_LCD_OSDS:\t0x%08x\n", REG_LCD_OSDS);
	printk("REG_LCD_BGC:\t0x%08x\n", REG_LCD_BGC);
	printk("REG_LCD_KEK0:\t0x%08x\n", REG_LCD_KEY0);
	printk("REG_LCD_KEY1:\t0x%08x\n", REG_LCD_KEY1);
	printk("REG_LCD_ALPHA:\t0x%08x\n", REG_LCD_ALPHA);
	printk("REG_LCD_IPUR:\t0x%08x\n", REG_LCD_IPUR);
	printk("REG_LCD_VAT:\t0x%08x\n", REG_LCD_VAT);
	printk("REG_LCD_DAH:\t0x%08x\n", REG_LCD_DAH);
	printk("REG_LCD_DAV:\t0x%08x\n", REG_LCD_DAV);
	printk("REG_LCD_XYP0:\t0x%08x\n", REG_LCD_XYP0);
	printk("REG_LCD_XYP1:\t0x%08x\n", REG_LCD_XYP1);
	printk("REG_LCD_SIZE0:\t0x%08x\n", REG_LCD_SIZE0);
	printk("REG_LCD_SIZE1:\t0x%08x\n", REG_LCD_SIZE1);
	printk("REG_LCD_RGBC\t0x%08x\n", REG_LCD_RGBC);
	printk("REG_LCD_VSYNC:\t0x%08x\n", REG_LCD_VSYNC);
	printk("REG_LCD_HSYNC:\t0x%08x\n", REG_LCD_HSYNC);
	printk("REG_LCD_PS:\t0x%08x\n", REG_LCD_PS);
	printk("REG_LCD_CLS:\t0x%08x\n", REG_LCD_CLS);
	printk("REG_LCD_SPL:\t0x%08x\n", REG_LCD_SPL);
	printk("REG_LCD_REV:\t0x%08x\n", REG_LCD_REV);
	printk("REG_LCD_IID:\t0x%08x\n", REG_LCD_IID);
	printk("REG_LCD_DA0:\t0x%08x\n", REG_LCD_DA0);
	printk("REG_LCD_SA0:\t0x%08x\n", REG_LCD_SA0);
	printk("REG_LCD_FID0:\t0x%08x\n", REG_LCD_FID0);
	printk("REG_LCD_CMD0:\t0x%08x\n", REG_LCD_CMD0);
	printk("REG_LCD_OFFS0:\t0x%08x\n", REG_LCD_OFFS0);
	printk("REG_LCD_PW0:\t0x%08x\n", REG_LCD_PW0);
	printk("REG_LCD_CNUM0:\t0x%08x\n", REG_LCD_CNUM0);
	printk("REG_LCD_DESSIZE0:\t0x%08x\n", REG_LCD_DESSIZE0);
	printk("REG_LCD_DA1:\t0x%08x\n", REG_LCD_DA1);
	printk("REG_LCD_SA1:\t0x%08x\n", REG_LCD_SA1);
	printk("REG_LCD_FID1:\t0x%08x\n", REG_LCD_FID1);
	printk("REG_LCD_CMD1:\t0x%08x\n", REG_LCD_CMD1);
	printk("REG_LCD_OFFS1:\t0x%08x\n", REG_LCD_OFFS1);
	printk("REG_LCD_PW1:\t0x%08x\n", REG_LCD_PW1);
	printk("REG_LCD_CNUM1:\t0x%08x\n", REG_LCD_CNUM1);
	printk("REG_LCD_DESSIZE1:\t0x%08x\n", REG_LCD_DESSIZE1);
	printk("==================================\n");
	printk("REG_LCD_VSYNC:\t%d:%d\n", REG_LCD_VSYNC>>16, REG_LCD_VSYNC&0xfff);
	printk("REG_LCD_HSYNC:\t%d:%d\n", REG_LCD_HSYNC>>16, REG_LCD_HSYNC&0xfff);
	printk("REG_LCD_VAT:\t%d:%d\n", REG_LCD_VAT>>16, REG_LCD_VAT&0xfff);
	printk("REG_LCD_DAH:\t%d:%d\n", REG_LCD_DAH>>16, REG_LCD_DAH&0xfff);
	printk("REG_LCD_DAV:\t%d:%d\n", REG_LCD_DAV>>16, REG_LCD_DAV&0xfff);
	printk("==================================\n");

	/* Smart LCD Controller Resgisters */
	printk("REG_SLCD_CFG:\t0x%08x\n", REG_SLCD_CFG);
	printk("REG_SLCD_CTRL:\t0x%08x\n", REG_SLCD_CTRL);
	printk("REG_SLCD_STATE:\t0x%08x\n", REG_SLCD_STATE);
	printk("==================================\n");

	/* TVE Controller Resgisters */
	printk("REG_TVE_CTRL:\t0x%08x\n", REG_TVE_CTRL);
	printk("REG_TVE_FRCFG:\t0x%08x\n", REG_TVE_FRCFG);
	printk("REG_TVE_SLCFG1:\t0x%08x\n", REG_TVE_SLCFG1);
	printk("REG_TVE_SLCFG2:\t0x%08x\n", REG_TVE_SLCFG2);
	printk("REG_TVE_SLCFG3:\t0x%08x\n", REG_TVE_SLCFG3);
	printk("REG_TVE_LTCFG1:\t0x%08x\n", REG_TVE_LTCFG1);
	printk("REG_TVE_LTCFG2:\t0x%08x\n", REG_TVE_LTCFG2);
	printk("REG_TVE_CFREQ:\t0x%08x\n", REG_TVE_CFREQ);
	printk("REG_TVE_CPHASE:\t0x%08x\n", REG_TVE_CPHASE);
	printk("REG_TVE_CBCRCFG:\t0x%08x\n", REG_TVE_CBCRCFG);
	printk("REG_TVE_WSSCR:\t0x%08x\n", REG_TVE_WSSCR);
	printk("REG_TVE_WSSCFG1:\t0x%08x\n", REG_TVE_WSSCFG1);
	printk("REG_TVE_WSSCFG2:\t0x%08x\n", REG_TVE_WSSCFG2);
	printk("REG_TVE_WSSCFG3:\t0x%08x\n", REG_TVE_WSSCFG3);

	printk("==================================\n");

	if ( 1 ) {
		unsigned int * pii = (unsigned int *)dma_desc_base;
		int i, j;
		for (j=0;j< DMA_DESC_NUM ; j++) {
			printk("dma_desc%d(0x%08x):\n", j, (unsigned int)pii);
			for (i =0; i<8; i++ ) {
				printk("\t\t0x%08x\n", *pii++);
			}
		}
	}
}
#else
#define print_lcdc_registers()
#endif

static inline u_int chan_to_field(u_int chan, struct fb_bitfield *bf)
{
        chan &= 0xffff;
        chan >>= 16 - bf->length;
        return chan << bf->offset;
}

static int jz4750fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			  u_int transp, struct fb_info *info)
{
	struct lcd_cfb_info *cfb = (struct lcd_cfb_info *)info;
	unsigned short *ptr, ctmp;

//	print_dbg("regno:%d,RGBt:(%d,%d,%d,%d)\t", regno, red, green, blue, transp);
	if (regno >= NR_PALETTE)
		return 1;

	cfb->palette[regno].red		= red ;
	cfb->palette[regno].green	= green;
	cfb->palette[regno].blue	= blue;
	if (cfb->fb.var.bits_per_pixel <= 16) {
		red	>>= 8;
		green	>>= 8;
		blue	>>= 8;

		red	&= 0xff;
		green	&= 0xff;
		blue	&= 0xff;
	}
	switch (cfb->fb.var.bits_per_pixel) {
	case 1:
	case 2:
	case 4:
	case 8:
		if (((jz4750_lcd_info->panel.cfg & LCD_CFG_MODE_MASK) == LCD_CFG_MODE_SINGLE_MSTN ) ||
		    ((jz4750_lcd_info->panel.cfg & LCD_CFG_MODE_MASK) == LCD_CFG_MODE_DUAL_MSTN )) {
			ctmp = (77L * red + 150L * green + 29L * blue) >> 8;
			ctmp = ((ctmp >> 3) << 11) | ((ctmp >> 2) << 5) |
				(ctmp >> 3);
		} else {
			/* RGB 565 */
			if (((red >> 3) == 0) && ((red >> 2) != 0))
			red = 1 << 3;
			if (((blue >> 3) == 0) && ((blue >> 2) != 0))
				blue = 1 << 3;
			ctmp = ((red >> 3) << 11) 
				| ((green >> 2) << 5) | (blue >> 3);
		}

		ptr = (unsigned short *)lcd_palette;
		ptr = (unsigned short *)(((u32)ptr)|0xa0000000);
		ptr[regno] = ctmp;

		break;
		
	case 15:
		if (regno < 16)
			((u32 *)cfb->fb.pseudo_palette)[regno] =
				((red >> 3) << 10) | 
				((green >> 3) << 5) |
				(blue >> 3);
		break;
	case 16:
		if (regno < 16) {
			((u32 *)cfb->fb.pseudo_palette)[regno] =
				((red >> 3) << 11) | 
				((green >> 2) << 5) |
				(blue >> 3); 
		}
		break;
	case 17 ... 32:
		if (regno < 16)
			((u32 *)cfb->fb.pseudo_palette)[regno] =
				(red << 16) | 
				(green << 8) |
				(blue << 0); 

/*		if (regno < 16) {
			unsigned val;
                        val  = chan_to_field(red, &cfb->fb.var.red);
                        val |= chan_to_field(green, &cfb->fb.var.green);
                        val |= chan_to_field(blue, &cfb->fb.var.blue);
			((u32 *)cfb->fb.pseudo_palette)[regno] = val;
		}
*/

		break;
	}
	return 0;
}

static struct task_struct * resize_task;
static struct task_struct * resize_a320_original_task;
static struct task_struct * resize_a320_full_screen_task;
unsigned int resize_a320_go_out = 0;
unsigned short *frame_dst;
unsigned short *frame_src; 
//unsigned short frame_temp[640*480];
        
#define SCALE_WIDTH  160 //x_src = (x_dest * SCALE_WIDTH) >> 8
#define SCALE_HEIGHT  128 //y_src = (y_dest * SCALE_HEIGHT) >> 8
#define GET_R(temp) temp>>11
#define GET_G(temp) (temp&0x7e0)>>5
#define GET_B(temp) (temp&0x1f)
#define CREATE_RGB(r,g,b) (r << 11) | (g << 5) | b
//char r_pix[640*480];
//char g_pix[640*480];
//char b_pix[640*480];

static void fb_a320_thread(unsigned int src_weight, unsigned int src_height, unsigned int dst_weight, unsigned int dst_height,int mode)
{
  frame_dst = (unsigned short *)lcd_frame01;
  frame_src = (unsigned short *)lcd_frame0;
  unsigned short *lcd_frame_temp;
#define FRACTION_STEP 0x10000
  const unsigned int  x_fraction=src_weight*FRACTION_STEP/dst_weight;
  const unsigned int  y_fraction=src_height*FRACTION_STEP/dst_height;
  unsigned int x_temp = 0;
  unsigned int y_temp = 0;
  y_temp = y_fraction;
  unsigned int original_size_offset = ((dst_height - src_height) >> 1)*dst_weight;

  int i,j;
#define A320_FULLSCREEN 1
#define A320_ORIGINAL   2
  switch (mode)
  {
    case A320_FULLSCREEN:
      for(j = 0; j < dst_height; j++)
      {
        y_temp += y_fraction;
        if(y_temp >= FRACTION_STEP)
        {
          y_temp -= FRACTION_STEP;
          //scale horiontal
          x_temp = x_fraction;
          for(i = 0; i < dst_weight; i++)
          {
            x_temp += x_fraction;
            *frame_dst = *frame_src;
            if(x_temp >= FRACTION_STEP) 
            {
              frame_src++;
              x_temp -= FRACTION_STEP; 
            }
            frame_dst++;
          }
          frame_src += 480-320;
        }
        else
        {
          lcd_frame_temp = frame_dst - dst_weight;
          for(i = 0; i < dst_weight; i++)
          {
            *frame_dst++ = *lcd_frame_temp++;
          }
        }
      }
      break;
    case A320_ORIGINAL:
      frame_dst += original_size_offset;
      for(j = 0; j < src_height; j++)
      {
        frame_dst +=(dst_weight - src_weight)/2;
        for(i = 0; i < src_weight; i++)
        {
          *frame_dst = *frame_src;
          frame_dst++;
          frame_src++;
        }
        frame_src += 480-320;
        frame_dst +=(dst_weight - src_weight)/2;
      }
      break;
  }
}
static void fb2x(void)
{
	frame_dst = (unsigned short *)lcd_frame01;
	frame_src = (unsigned short *)lcd_frame0;
        unsigned short *lcd_frame_temp;
#define TV_OUT_WEIGHT 640
#define TV_OUT_HEIGHT 480
#define SCREEN_WEIGHT 480
#define SCREEN_HEIGHT 272
#define FRACTION_STEP 0x10000
        const unsigned int  x_fraction=SCREEN_WEIGHT*FRACTION_STEP/TV_OUT_WEIGHT;
        const unsigned int  y_fraction=SCREEN_HEIGHT*FRACTION_STEP/TV_OUT_HEIGHT;
        unsigned int x_temp = 0;
        unsigned int y_temp = 0;
        y_temp = y_fraction;

        int i,j;
        for(j = 0; j < TV_OUT_HEIGHT; j++)
        {
          y_temp += y_fraction;
          if(y_temp >= FRACTION_STEP)
          {
            y_temp -= FRACTION_STEP;
            //scale horiontal
            x_temp = x_fraction;
            for(i = 0; i < TV_OUT_WEIGHT; i++)
            {
              x_temp += x_fraction;
              *frame_dst = *frame_src;
              if(x_temp >= FRACTION_STEP) 
              {
                frame_src++;
                x_temp -= FRACTION_STEP; 
              }
              frame_dst++;
            }
          }
          else
          {
            lcd_frame_temp = frame_dst - TV_OUT_WEIGHT;
            for(i = 0; i < TV_OUT_WEIGHT; i++)
            {
              *frame_dst++ = *lcd_frame_temp++;
            }
          }
        }
#if 0
        int i,j,l;
        int  m = 0;
        int  k = 0;
     
	for(i=0; i<240; i++)
	{
		l = m;
		for(j=0; j<320; j++)
		{
			frame_dst[k++] = frame_src[l];
			frame_dst[k++] = frame_src[l++];
		}
		
		l = m;
		for(j=0; j<320; j++)
		{
			frame_dst[k++] = frame_src[l];
			frame_dst[k++] = frame_src[l++]; 
		}
		
		m += 320;
	}
#endif
#if 0
        int i,j,l;
        int  m = 0;
        int  k = 0;
        for(i=0; i<240; i++)
        {
		l = m;
		for(j=0; j<80; j++)
		{
				frame_dst[k++] = frame_src[l];
				frame_dst[k++] = frame_src[l++];
				frame_dst[k++] = frame_src[l++];
				frame_dst[k++] = frame_src[l];
				frame_dst[k++] = frame_src[l++];
				frame_dst[k++] = frame_src[l];
				frame_dst[k++] = frame_src[l++];
				frame_dst[k++] = frame_src[l++];
		}
		
		l = m;
		for(j=0; j<80; j++)
		{
				frame_dst[k++] = frame_src[l];
				frame_dst[k++] = frame_src[l++];
				frame_dst[k++] = frame_src[l++];
				frame_dst[k++] = frame_src[l];
				frame_dst[k++] = frame_src[l++];
				frame_dst[k++] = frame_src[l];
				frame_dst[k++] = frame_src[l++];
				frame_dst[k++] = frame_src[l++];
		}
		
		m += 400;
	}
#if 0	
        //memcpy(frame_temp,&frame_dst,640*480*2);
        for(i = 0; i < 640*480; i++)
        {
          frame_temp[i] = frame_dst[i];
        }
        printk("%d \n",__LINE__);
        char r1,g1,b1;
        char r2,g2,b2;
        char r3,g3,b3;
        char r4,g4,b4;
        char r5,g5,b5;
        char r6,g6,b6;
        char r7,g7,b7;
        char r8,g8,b8;
        char r9,g9,b9;
        char r,g,b;
        short temp;
        k = 0;
        int temp_y = 0;
        for(i = 0; i < 480; i++)
        {
          temp_y = i*640;

          for(j = 0; j < 640; j++)
          {
            temp = frame_temp[j+i];
            r_pix[j+i] = GET_R(temp);;
            g_pix[j+i] = GET_G(temp);;
            b_pix[j+i] = GET_B(temp);;
          }
        }
        for(i =1 ; i < 480-2; i++)
        {
          temp_y = i*640;
          k = temp_y + 1;
          for(j = 1; j < 640-2; j++)
          {
#if 0
            temp = frame_temp[j-1+temp_y-640]; 
            r1 = GET_R(temp); 
            g1 = GET_G(temp); 
            b1 = GET_B(temp); 
            temp = frame_temp[j+temp_y-640]; 
            r2 = GET_R(temp); 
            g2 = GET_G(temp); 
            b2 = GET_B(temp); 
            temp = frame_temp[j+1+temp_y-640]; 
            r3 = GET_R(temp); 
            g3 = GET_G(temp); 
            b3 = GET_B(temp); 
            temp = frame_temp[j-1+temp_y]; 
            r4 = GET_R(temp); 
            g4 = GET_G(temp); 
            b4 = GET_B(temp); 
            temp = frame_temp[j+temp_y]; 
            r5 = GET_R(temp); 
            g5 = GET_G(temp); 
            b5 = GET_B(temp); 
            temp = frame_temp[j+1+temp_y]; 
            r6 = GET_R(temp); 
            g6 = GET_G(temp); 
            b6 = GET_B(temp); 
            temp = frame_temp[j-1+temp_y+640]; 
            r7 = GET_R(temp); 
            g7 = GET_G(temp); 
            b7 = GET_B(temp); 
            temp = frame_temp[j+temp_y+640]; 
            r8 = GET_R(temp); 
            g8 = GET_G(temp); 
            b8 = GET_B(temp); 
            temp = frame_temp[j+1+temp_y+640]; 
            r9 = GET_R(temp); 
            g9 = GET_G(temp); 
            b9 = GET_B(temp); 
#else
            r1 = r_pix[j-1+temp_y-640]; 
            g1 = g_pix[j-1+temp_y-640]; 
            b1 = b_pix[j-1+temp_y-640]; 
            r2 = r_pix[j+temp_y-640]; 
            g2 = g_pix[j+temp_y-640]; 
            b2 = b_pix[j+temp_y-640]; 
            r3 = r_pix[j+1+temp_y-640]; 
            g3 = g_pix[j+1+temp_y-640]; 
            b3 = b_pix[j+1+temp_y-640]; 
            r4 = r_pix[j-1+temp_y]; 
            g4 = g_pix[j-1+temp_y]; 
            b4 = b_pix[j-1+temp_y]; 
            r5 = r_pix[j+temp_y]; 
            g5 = g_pix[j+temp_y]; 
            b5 = b_pix[j+temp_y]; 
            r6 = r_pix[j+1+temp_y]; 
            g6 = g_pix[j+1+temp_y]; 
            b6 = b_pix[j+1+temp_y]; 
            r7 = r_pix[j-1+temp_y+640]; 
            g7 = g_pix[j-1+temp_y+640]; 
            b7 = b_pix[j-1+temp_y+640]; 
            r8 = r_pix[j+temp_y+640]; 
            g8 = g_pix[j+temp_y+640]; 
            b8 = b_pix[j+temp_y+640]; 
            r9 = r_pix[j+1+temp_y+640]; 
            g9 = g_pix[j+1+temp_y+640]; 
            b9 = b_pix[j+1+temp_y+640]; 

#endif
            r = (r1+r2<<1+r3+r4<<1+r5<<2+r6<<1+r7+r8<<1+r9) >> 4;
            g = (g1+g2<<1+g3+g4<<1+g5<<2+g6<<1+g7+g8<<1+g9) >> 4;
            b = (b1+b2<<1+b3+b4<<1+b5<<2+b6<<1+b7+b8<<1+b9) >> 4;
            frame_dst[k++] = CREATE_RGB(r,g,b);

          }
        }
#endif
#else
#if 0
        int i,j;
        int r,g,b;
        int r1,g1,b1;
        int r2,g2,b2;
        int r3,g3,b3;
        int r4,g4,b4;
        int temp = 0;
        int temp_y = 0;
        int k = 0;
        register int tmp_x_y;//,tmp_uv,tmp_1uv,tmp_u1v,tmp_1u1v;
        for(i =0; i < 480; i++)
        {
          temp_y = y_dest_array[i]*400;
          for(j = 0; j < 640; j++)
          {
            temp = frame_src[x_dest_array[j]+temp_y]; 
            r1 = GET_R(temp); 
            g1 = GET_G(temp); 
            b1 = GET_B(temp); 
            temp = frame_src[x_dest_array[j] + 1 + temp_y]; 
            r2 = GET_R(temp); 
            g2 = GET_G(temp); 
            b2 = GET_B(temp); 
            temp = frame_src[x_dest_array[j] +temp_y +400]; 
            r3 = GET_R(temp); 
            g3 = GET_G(temp); 
            b3 = GET_B(temp); 
            temp = frame_src[x_dest_array[j] + 1 +temp_y +400]; 
            r4 = GET_R(temp); 
            g4 = GET_G(temp); 
            b4 = GET_B(temp); 
#if 0
            r =  ((4-u_dest_array[j])*(4-v_dest_array[i])*r1+(u_dest_array[j])*(4-v_dest_array[i])*r2 + (4-u_dest_array[j])*(v_dest_array[i])*r3+ (u_dest_array[j])*(v_dest_array[i])*r4) >> 4;
            g =  ((4-u_dest_array[j])*(4-v_dest_array[i])*g1+(u_dest_array[j])*(4-v_dest_array[i])*g2 + (4-u_dest_array[j])*(v_dest_array[i])*g3+ (u_dest_array[j])*(v_dest_array[i])*g4) >> 4;
            b =  ((4-u_dest_array[j])*(4-v_dest_array[i])*b1+(u_dest_array[j])*(4-v_dest_array[i])*b2 + (4-u_dest_array[j])*(v_dest_array[i])*b3+ (u_dest_array[j])*(v_dest_array[i])*b4) >> 4;
#else
tmp_x_y= i*640+j;
//tmp_uv,tmp_1uv,tmp_u1v,tmp_1u1v;
        
            r =  (u_1_v_1_dest_array[tmp_x_y]*r1+u_v_1_dest_array[tmp_x_y]*r2 + u_1_v_dest_array[tmp_x_y]*r3+ u_v_dest_array[tmp_x_y]*r4) >> 4;
            g =  (u_1_v_1_dest_array[tmp_x_y]*g1+u_v_1_dest_array[tmp_x_y]*g2 + u_1_v_dest_array[tmp_x_y]*g3+ u_v_dest_array[tmp_x_y]*g4) >> 4;
            b =  (u_1_v_1_dest_array[tmp_x_y]*b1+u_v_1_dest_array[tmp_x_y]*b2 + u_1_v_dest_array[tmp_x_y]*b3+ u_v_dest_array[tmp_x_y]*b4) >> 4;

#endif
            frame_dst[k++] = CREATE_RGB(r,g,b);
          }
        }
#endif
#endif
	//memset(lcd_frame01, 0x0, 640 * 480 * 2);
}

static int fb_resize_thread(void *unused)
{
	printk("kernel frame buffer resize thread start!\n");
	while(1)
	{
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ/20);
		fb2x();

		if(resize_go_out)
		break;
	}

}
static void fb_resize_start()
{
#ifdef TVOUT_2x
	resize_go_out = 0;
	resize_task = kthread_run(fb_resize_thread, NULL, "fb_resize");
	if(IS_ERR(resize_task))
	{
		printk("Kernel fb resize thread start error!\n");
		return;
	}
#endif	
}


/* 
 * switch to tve mode from lcd mode
 * mode:
 * 	PANEL_MODE_TVE_PAL: switch to TVE_PAL mode
 * 	PANEL_MODE_TVE_NTSC: switch to TVE_NTSC mode
 */
static void jz4750lcd_info_switch_to_TVE(int mode)
{
	struct jz4750lcd_info *info;
	struct jz4750lcd_osd_t *osd_lcd;
	int x, y, w, h;

	info = jz4750_lcd_info = &jz4750_info_tve;
	osd_lcd = &jz4750_lcd_panel.osd;
	//maddrone add
        //jz4750lcd_ipu_init();

	switch ( mode ) {
	case PANEL_MODE_TVE_PAL:
		info->panel.cfg |= LCD_CFG_TVEPEH; /* TVE PAL enable extra halfline signal */
		info->panel.w = TVE_WIDTH_PAL;
		info->panel.h = TVE_HEIGHT_PAL;
		info->panel.fclk = TVE_FREQ_PAL;
		//w = ( osd_lcd->fg0.w < TVE_WIDTH_PAL )? osd_lcd->fg0.w:TVE_WIDTH_PAL;
		//h = ( osd_lcd->fg0.h < TVE_HEIGHT_PAL )?osd_lcd->fg0.h:TVE_HEIGHT_PAL;
		//w = TVE_WIDTH_PAL;
		//h = TVE_HEIGHT_PAL;
#ifdef TVOUT_2x
		w = 640;
		h = 480;
#else
		w = tvout_display_w;
		h = tvout_display_h;
#endif
		//x = ((TVE_WIDTH_PAL - w) >> 2) << 1;
		//y = ((TVE_HEIGHT_PAL - h) >> 2) << 1;
#ifdef TVOUT_2x
		x = (TVE_WIDTH_PAL - 640)/2;
		y = (TVE_HEIGHT_PAL - 480)/2;
#else
		x = (TVE_WIDTH_PAL - w)/2;
		y = (TVE_HEIGHT_PAL - h)/2;
#endif
		info->osd.fg0.bpp = osd_lcd->fg0.bpp;
		info->osd.fg0.x = x;
		info->osd.fg0.y = y;
		info->osd.fg0.w = w;
		info->osd.fg0.h = h;
		w = ( osd_lcd->fg1.w < TVE_WIDTH_PAL )? osd_lcd->fg1.w:TVE_WIDTH_PAL;
		h = ( osd_lcd->fg1.h < TVE_HEIGHT_PAL )?osd_lcd->fg1.h:TVE_HEIGHT_PAL;
		x = ((TVE_WIDTH_PAL-w) >> 2) << 1;
		y = ((TVE_HEIGHT_PAL-h) >> 2) << 1;
		//info->osd.fg1.bpp = 32;	/* use RGB888 in TVE mode*/
		info->osd.fg1.bpp = 16;	/* use RGB565 in TVE mode*/
		info->osd.fg1.x = x;
		info->osd.fg1.y = y;
		info->osd.fg1.w = w;
		info->osd.fg1.h = h;
		printk("lcd_info Switch to PAL\n");
		break;
	case PANEL_MODE_TVE_NTSC:
		info->panel.cfg &= ~LCD_CFG_TVEPEH; /* TVE NTSC disable extra halfline signal */
		info->panel.w = TVE_WIDTH_NTSC;
		info->panel.h = TVE_HEIGHT_NTSC;
		info->panel.fclk = TVE_FREQ_NTSC;
		//w = ( osd_lcd->fg0.w < TVE_WIDTH_NTSC )? osd_lcd->fg0.w:TVE_WIDTH_NTSC;
		//h = ( osd_lcd->fg0.h < TVE_HEIGHT_NTSC)?osd_lcd->fg0.h:TVE_HEIGHT_NTSC;
		//x = ((TVE_WIDTH_NTSC - w) >> 2) << 1;
		//y = ((TVE_HEIGHT_NTSC - h) >> 2) << 1;
//		x = 0;
//		y = 0;
#ifdef TVOUT_2x
		w = 640;
		h = 480;
#else
		w = tvout_display_w;
		h = tvout_display_h;
#endif
		//x = ((TVE_WIDTH_PAL - w) >> 2) << 1;
		//y = ((TVE_HEIGHT_PAL - h) >> 2) << 1;
#ifdef TVOUT_2x
		x = (TVE_WIDTH_NTSC - 640)/2;
		//y = (TVE_HEIGHT_NTSC- 480)/2;
                y = 2;//y must be even
#else
		x = (TVE_WIDTH_PAL - w)/2;
		y = (TVE_HEIGHT_PAL - h)/2;
#endif

		info->osd.fg0.bpp = osd_lcd->fg0.bpp;
		info->osd.fg0.x = x;
		info->osd.fg0.y = y;
		info->osd.fg0.w = w;
		info->osd.fg0.h = h;
		w = ( osd_lcd->fg1.w < TVE_WIDTH_NTSC )? osd_lcd->fg1.w:TVE_WIDTH_NTSC;
		h = ( osd_lcd->fg1.h < TVE_HEIGHT_NTSC)?osd_lcd->fg1.h:TVE_HEIGHT_NTSC;
		x = ((TVE_WIDTH_NTSC - w) >> 2) << 1;
		y = ((TVE_HEIGHT_NTSC - h) >> 2) << 1;
        //info->osd.fg1.bpp = 32;	/* use RGB888 int TVE mode */
        info->osd.fg1.bpp = 16;	/* use RGB565 in TVE mode*/
        info->osd.fg1.x = x;
		info->osd.fg1.y = y;
		info->osd.fg1.w = w;
		info->osd.fg1.h = h;
		break;
	default:
		printk("%s, %s: Unknown tve mode\n", __FILE__, __FUNCTION__);
	}
}

static int jz4750fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

        void __user *argp = (void __user *)arg;

//	struct jz4750lcd_info *lcd_info = jz4750_lcd_info;


	switch (cmd) {
	case FBIOSETBACKLIGHT:
		__lcd_set_backlight_level(arg);	/* We support 8 levels here. */
		break;
	case FBIODISPON:
		REG_LCD_STATE = 0; /* clear lcdc status */
		//__lcd_slcd_special_on();
		__lcd_clr_dis();
		__lcd_set_ena(); /* enable lcdc */
		__lcd_display_on();
		break;
	case FBIODISPOFF:
		__lcd_display_off();
		#if 1
		if ( jz4750_lcd_info->panel.cfg & LCD_CFG_LCDPIN_SLCD || 
			jz4750_lcd_info->panel.cfg & LCD_CFG_TVEN ) /*  */
			__lcd_clr_ena(); /* Smart lcd and TVE mode only support quick disable */
		else {
			int cnt;
			/* when CPU main freq is 336MHz,wait for 16ms */
			cnt = 336000 * 16;
			__lcd_set_dis(); /* regular disable */
			while(!__lcd_disable_done() && cnt) {
				cnt--;
			}
			if (cnt == 0)
				printk("LCD disable timeout! REG_LCD_STATE=0x%08xx\n",REG_LCD_STATE);
			REG_LCD_STATE &= ~LCD_STATE_LDD;
		}
		#endif
		break;
	case FBIOPRINT_REG:
		print_lcdc_registers();
		break;
	case FBIO_GET_MODE:
		print_dbg("fbio get mode\n");
		if (copy_to_user(argp, jz4750_lcd_info, sizeof(struct jz4750lcd_info)))
			return -EFAULT;
		break;
	case FBIO_SET_MODE:
		print_dbg("fbio set mode\n");
		#if 1
		if (copy_from_user(jz4750_lcd_info, argp, sizeof(struct jz4750lcd_info)))
			return -EFAULT;
		/* set mode */
		jz4750fb_set_mode(jz4750_lcd_info);
		#endif
		break;
	case FBIO_DEEP_SET_MODE:
		print_dbg("fbio deep set mode\n");
		#if 1
		if (copy_from_user(jz4750_lcd_info, argp, sizeof(struct jz4750lcd_info)))
			return -EFAULT;
		//jz4750fb_deep_set_mode(jz4750_lcd_info);
		#endif
		break;
#ifdef CONFIG_FB_JZ4750_TVE
	case FBIO_MODE_SWITCH:
		//print_dbg("lcd mode switch between tve and lcd, arg=%lu\n", arg);
		printk("lcd mode switch between tve and lcd, arg=%lu\n", arg);
		switch ( arg ) {
		case PANEL_MODE_TVE_PAL: 	/* switch to TVE_PAL mode */
		case PANEL_MODE_TVE_NTSC: 	/* switch to TVE_NTSC mode */
			printk("IOCTL: arg=PANEL_MODE_TVE_PAL\n");
			if(arg == PANEL_MODE_TVE_PAL)
            {
              tvout_display_w = 720;
              tvout_display_h = 540;
            }
			else
			{
              tvout_display_w = 720;
              tvout_display_h = 482;
            }
			jz4750lcd_info_switch_to_TVE(arg);
			jz4750tve_init(arg); /* tve controller init */
			udelay(100);
			printk("Enable TVE\n");
			jz4750tve_enable_tve();
			/* turn off lcd backlight */
			__lcd_display_off();
			break;
		case PANEL_MODE_LCD_PANEL: 	/* switch to LCD mode */
		default :
			/* turn off TVE, turn off DACn... */
			jz4750tve_disable_tve();
			jz4750_lcd_info = &jz4750_lcd_panel;
			/* turn on lcd backlight */
			__lcd_display_on();
			break;
		}
		jz4750fb_deep_set_mode(jz4750_lcd_info);
		//display_h_color_bar(720, 540, 16);
		fb_resize_start();
		break;
	case FBIO_GET_TVE_MODE:
		print_dbg("fbio get TVE mode\n");
		if (copy_to_user(argp, jz4750_tve_info, sizeof(struct jz4750tve_info)))
			return -EFAULT;
		break;
	case FBIO_SET_TVE_MODE:
		print_dbg("fbio set TVE mode\n");
		if (copy_from_user(jz4750_tve_info, argp, sizeof(struct jz4750tve_info)))
			return -EFAULT;
		/* set tve mode */
		jz4750tve_set_tve_mode(jz4750_tve_info);
		break;
#endif
	default:
		printk("%s, unknown command(0x%x)", __FILE__, cmd);
		break;
	}

	return ret;
}

/* Use mmap /dev/fb can only get a non-cacheable Virtual Address. */
static int jz4750fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct lcd_cfb_info *cfb = (struct lcd_cfb_info *)info;
	unsigned long start;
	unsigned long off;
	u32 len;
	lprint_dbg("the smem_start is 0x%x\n",cfb->fb.fix.smem_start);
	off = vma->vm_pgoff << PAGE_SHIFT;
	//fb->fb_get_fix(&fix, PROC_CONSOLE(info), info);

	/* frame buffer memory */
	start = cfb->fb.fix.smem_start;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + cfb->fb.fix.smem_len);
	start &= PAGE_MASK;

	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;
	off += start;

	vma->vm_pgoff = off >> PAGE_SHIFT;
	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);	/* Uncacheable */

#if 1
 	pgprot_val(vma->vm_page_prot) &= ~_CACHE_MASK;
 	pgprot_val(vma->vm_page_prot) |= _CACHE_UNCACHED;		/* Uncacheable */
//	pgprot_val(vma->vm_page_prot) |= _CACHE_CACHABLE_NONCOHERENT;	/* Write-Back */
#endif

	if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot)) {
		return -EAGAIN;
	}
	return 0;
}

/* checks var and eventually tweaks it to something supported,
 * DO NOT MODIFY PAR */
static int jz4750fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	//printk("jz4750fb_check_var, not implement\n");
	return 0;
}


/* 
 * set the video mode according to info->var
 */
static int jz4750fb_set_par(struct fb_info *info)
{
	//printk("jz4750fb_set_par, not implemented\n");
	return 0;
}


/*
 * (Un)Blank the display.
 * Fix me: should we use VESA value?
 */
static int jz4750fb_blank(int blank_mode, struct fb_info *info)
{
	dprintk("jz4750 fb_blank %d %p", blank_mode, info);
	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		//case FB_BLANK_NORMAL:
			/* Turn on panel */
		__lcd_set_ena();
		__lcd_display_on();
		break;

	case FB_BLANK_NORMAL:
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
#if 0
		/* Turn off panel */
		__lcd_display_off();
		__lcd_set_dis();
#endif
		break;
	default:
		break;

	}
	return 0;
}

/* 
 * pan display
 */
static int jz4750fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct lcd_cfb_info *cfb = (struct lcd_cfb_info *)info;
	int dy;

	if (!var || !cfb) {
		return -EINVAL;
	}

	if (var->xoffset - cfb->fb.var.xoffset) {
		/* No support for X panning for now! */
		return -EINVAL;
	}

	dy = var->yoffset;
	print_dbg("var.yoffset: %d", dy);
	if (dy) {
		dma1_desc0->databuf = (unsigned int)virt_to_phys((void *)lcd_frame0 + (cfb->fb.fix.line_length * dy));
		dma_cache_wback((unsigned int)(dma1_desc0), sizeof(struct jz4750_lcd_dma_desc));
	}
	else {
		dma1_desc0->databuf = (unsigned int)virt_to_phys((void *)lcd_frame0);
		dma_cache_wback((unsigned int)(dma1_desc0), sizeof(struct jz4750_lcd_dma_desc));
	}

	return 0;
}


/* use default function cfb_fillrect, cfb_copyarea, cfb_imageblit */
static struct fb_ops jz4750fb_ops = {
	.owner			= THIS_MODULE,
	.fb_setcolreg		= jz4750fb_setcolreg,
	.fb_check_var 		= jz4750fb_check_var,
	.fb_set_par 		= jz4750fb_set_par,
	.fb_blank		= jz4750fb_blank,
	.fb_pan_display		= jz4750fb_pan_display,
	.fb_fillrect		= cfb_fillrect,
	.fb_copyarea		= cfb_copyarea,
	.fb_imageblit		= cfb_imageblit,
	.fb_mmap		= jz4750fb_mmap,
	.fb_ioctl		= jz4750fb_ioctl,
};

static int jz4750fb_set_var(struct fb_var_screeninfo *var, int con,
			struct fb_info *info)
{
	struct lcd_cfb_info *cfb = (struct lcd_cfb_info *)info;
	struct jz4750lcd_info *lcd_info = jz4750_lcd_info;
	int chgvar = 0;
	
	//maddrone modify here
	var->height	            = lcd_info->osd.fg0.h;	/* tve mode */
	var->width	            = lcd_info->osd.fg0.w;

        if(tvout_640_480 == 0)
        {
          var->height = 272;
          var->width = 480;
        }
        else if(tvout_640_480 == 1)
        {
          var->height = 480;
          var->width = 640;
        }
        lprint_dbg("var->height is %d var->width is %d\n",var->height,var->width);
#if 0
	//maddrone add

	if(tvout_640_480)
	{
	var->height	            = 480;	/* tve mode */
	var->width	            = 640;
	}
	else
	{
	var->height	            = 272;	/* tve mode */
	var->width	            = 480;
	}
#endif

	var->bits_per_pixel	    = lcd_info->osd.fg0.bpp;

	var->vmode                  = FB_VMODE_NONINTERLACED;
	var->activate               = cfb->fb.var.activate;
	var->xres                   = var->width;
	var->yres                   = var->height;
	var->xres_virtual           = var->width;
	var->yres_virtual           = var->height;
	var->xoffset                = 0;
	var->yoffset                = 0;
	var->pixclock               = 0;
	var->left_margin            = 0;
	var->right_margin           = 0;
	var->upper_margin           = 0;
	var->lower_margin           = 0;
	var->hsync_len              = 0;
	var->vsync_len              = 0;
	var->sync                   = 0;
	var->activate              &= ~FB_ACTIVATE_TEST;
    
	/*
	 * CONUPDATE and SMOOTH_XPAN are equal.  However,
	 * SMOOTH_XPAN is only used internally by fbcon.
	 */
	if (var->vmode & FB_VMODE_CONUPDATE) {
		var->vmode |= FB_VMODE_YWRAP;
		var->xoffset = cfb->fb.var.xoffset;
		var->yoffset = cfb->fb.var.yoffset;
	}

	if (var->activate & FB_ACTIVATE_TEST)
		return 0;

	if ((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NOW)
		return -EINVAL;

	if (cfb->fb.var.xres != var->xres)
		chgvar = 1;
	if (cfb->fb.var.yres != var->yres)
		chgvar = 1;
	if (cfb->fb.var.xres_virtual != var->xres_virtual)
		chgvar = 1;
	if (cfb->fb.var.yres_virtual != var->yres_virtual)
		chgvar = 1;
	if (cfb->fb.var.bits_per_pixel != var->bits_per_pixel)
		chgvar = 1;

	//display = fb_display + con;

	var->red.msb_right	= 0;
	var->green.msb_right	= 0;
	var->blue.msb_right	= 0;

	switch(var->bits_per_pixel){
	case 1:	/* Mono */
		cfb->fb.fix.visual	= FB_VISUAL_MONO01;
		cfb->fb.fix.line_length	= (var->xres * var->bits_per_pixel) / 8;
		break;
	case 2:	/* Mono */
		var->red.offset		= 0;
		var->red.length		= 2;
		var->green.offset	= 0;
		var->green.length	= 2;
		var->blue.offset	= 0;
		var->blue.length	= 2;

		cfb->fb.fix.visual	= FB_VISUAL_PSEUDOCOLOR;
		cfb->fb.fix.line_length	= (var->xres * var->bits_per_pixel) / 8;
		break;
	case 4:	/* PSEUDOCOLOUR*/
		var->red.offset		= 0;
		var->red.length		= 4;
		var->green.offset	= 0;
		var->green.length	= 4;
		var->blue.offset	= 0;
		var->blue.length	= 4;

		cfb->fb.fix.visual	= FB_VISUAL_PSEUDOCOLOR;
		cfb->fb.fix.line_length	= var->xres / 2;
		break;
	case 8:	/* PSEUDOCOLOUR, 256 */
		var->red.offset		= 0;
		var->red.length		= 8;
		var->green.offset	= 0;
		var->green.length	= 8;
		var->blue.offset	= 0;
		var->blue.length	= 8;

		cfb->fb.fix.visual	= FB_VISUAL_PSEUDOCOLOR;
		cfb->fb.fix.line_length	= var->xres ;
		break;
	case 15: /* DIRECTCOLOUR, 32k */
		var->bits_per_pixel	= 15;
		var->red.offset		= 10;
		var->red.length		= 5;
		var->green.offset	= 5;
		var->green.length	= 5;
		var->blue.offset	= 0;
		var->blue.length	= 5;

		cfb->fb.fix.visual	= FB_VISUAL_DIRECTCOLOR;
		cfb->fb.fix.line_length	= var->xres_virtual * 2;
		break;
	case 16: /* DIRECTCOLOUR, 64k */
		var->bits_per_pixel	= 16;
		var->red.offset		= 11;
		var->red.length		= 5;
		var->green.offset	= 5;
		var->green.length	= 6;
		var->blue.offset	= 0;
		var->blue.length	= 5;

		cfb->fb.fix.visual	= FB_VISUAL_TRUECOLOR;
		cfb->fb.fix.line_length	= var->xres_virtual * 2;
		break;
	case 17 ... 32:
		/* DIRECTCOLOUR, 256 */
		var->bits_per_pixel	= 32;

		var->red.offset		= 16;
		var->red.length		= 8;
		var->green.offset	= 8;
		var->green.length	= 8;
		var->blue.offset	= 0;
		var->blue.length	= 8;
		var->transp.offset  	= 24;
		var->transp.length 	= 8;

		cfb->fb.fix.visual	= FB_VISUAL_TRUECOLOR;
		cfb->fb.fix.line_length	= var->xres_virtual * 4;
		break;

	default: /* in theory this should never happen */
		printk(KERN_WARNING "%s: don't support for %dbpp\n",
		       cfb->fb.fix.id, var->bits_per_pixel);
		break;
	}

	cfb->fb.var = *var;
	cfb->fb.var.activate &= ~FB_ACTIVATE_ALL;
        lprint_dbg("cfb->fb.fix.line_length is %d\n",cfb->fb.fix.line_length);
	/*
	 * Update the old var.  The fbcon drivers still use this.
	 * Once they are using cfb->fb.var, this can be dropped.
	 *					--rmk
	 */
	//display->var = cfb->fb.var;
	/*
	 * If we are setting all the virtual consoles, also set the
	 * defaults used to create new consoles.
	 */
	fb_set_cmap(&cfb->fb.cmap, &cfb->fb);
	dprintk("jz4750fb_set_var: after fb_set_cmap...\n");

	return 0;
}

static struct lcd_cfb_info * jz4750fb_alloc_fb_info(void)
{
 	struct lcd_cfb_info *cfb;

	cfb = kmalloc(sizeof(struct lcd_cfb_info) + sizeof(u32) * 16, GFP_KERNEL);

	if (!cfb)
		return NULL;

	jz4750fb_info = cfb;

	memset(cfb, 0, sizeof(struct lcd_cfb_info) );

	cfb->currcon		= -1;


	strcpy(cfb->fb.fix.id, "jz-lcd");
	cfb->fb.fix.type	= FB_TYPE_PACKED_PIXELS;
	cfb->fb.fix.type_aux	= 0;
	cfb->fb.fix.xpanstep	= 1;
	cfb->fb.fix.ypanstep	= 1;
	cfb->fb.fix.ywrapstep	= 0;
	cfb->fb.fix.accel	= FB_ACCEL_NONE;

	cfb->fb.var.nonstd	= 0;
	cfb->fb.var.activate	= FB_ACTIVATE_NOW;
	cfb->fb.var.height	= -1;
	cfb->fb.var.width	= -1;
	cfb->fb.var.accel_flags	= FB_ACCELF_TEXT;

	cfb->fb.fbops		= &jz4750fb_ops;
	cfb->fb.flags		= FBINFO_FLAG_DEFAULT;

	cfb->fb.pseudo_palette	= (void *)(cfb + 1);

	switch (jz4750_lcd_info->osd.fg0.bpp) {
	case 1:
		fb_alloc_cmap(&cfb->fb.cmap, 4, 0);
		break;
	case 2:
		fb_alloc_cmap(&cfb->fb.cmap, 8, 0);
		break;
	case 4:
		fb_alloc_cmap(&cfb->fb.cmap, 32, 0);
		break;
	case 8:

	default:
		fb_alloc_cmap(&cfb->fb.cmap, 256, 0);
		break;
	}
	dprintk("fb_alloc_cmap,fb.cmap.len:%d....\n", cfb->fb.cmap.len);

	return cfb;
}

/*
 * Map screen memory
 */
static int jz4750fb_map_smem(struct lcd_cfb_info *cfb)
{
	unsigned long page;
	unsigned int page_shift, needroom, needroom1, bpp, w, h;

	bpp = jz4750_lcd_info->osd.fg0.bpp;
	if ( bpp == 18 || bpp == 24)
		bpp = 32;
	if ( bpp == 15 )
		bpp = 16;
#ifndef CONFIG_FB_JZ4750_TVE
	w = jz4750_lcd_info->osd.fg0.w;
	h = jz4750_lcd_info->osd.fg0.h;
#else
	w = ( jz4750_lcd_info->osd.fg0.w > TVE_WIDTH_PAL )?jz4750_lcd_info->osd.fg0.w:TVE_WIDTH_PAL;
	h = ( jz4750_lcd_info->osd.fg0.h > TVE_HEIGHT_PAL )?jz4750_lcd_info->osd.fg0.h:TVE_HEIGHT_PAL;
#endif
	needroom1 = needroom = ((w * bpp + 7) >> 3) * h;
#if defined(CONFIG_FB_JZ4750_LCD_USE_2LAYER_FRAMEBUFFER)
	bpp = jz4750_lcd_info->osd.fg1.bpp;
	if ( bpp == 18 || bpp == 24)
		bpp = 32;
	if ( bpp == 15 )
		bpp = 16;

#ifndef CONFIG_FB_JZ4750_TVE
	w = jz4750_lcd_info->osd.fg1.w;
	h = jz4750_lcd_info->osd.fg1.h;
#else
	w = ( jz4750_lcd_info->osd.fg1.w > TVE_WIDTH_PAL )?jz4750_lcd_info->osd.fg1.w:TVE_WIDTH_PAL;
	h = ( jz4750_lcd_info->osd.fg1.h > TVE_HEIGHT_PAL )?jz4750_lcd_info->osd.fg1.h:TVE_HEIGHT_PAL;
#endif
	needroom += ((w * bpp + 7) >> 3) * h;
#endif // two layer

	
	printk("FrameBuffer bpp = %d\n",bpp);

	for (page_shift = 0; page_shift < 12; page_shift++)
		if ((PAGE_SIZE << page_shift) >= needroom)
			break;
	lcd_palette = (unsigned char *)__get_free_pages(GFP_KERNEL, 0);
	lcd_frame0 = (unsigned char *)__get_free_pages(GFP_KERNEL, page_shift);
#ifdef TVOUT_2x
	lcd_frame01 = (unsigned char *)__get_free_pages(GFP_KERNEL, page_shift);
#endif
	//maddrone add for mplayer trans fb
	vmfbmem_addr = lcd_frame01;
        memset(vmfbmem_addr,0xff,480*272*2);
        phy_vmfbmem_addr = virt_to_phys((void *)vmfbmem_addr);

	if ((!lcd_palette) || (!lcd_frame0))
		return -ENOMEM;
	memset((void *)lcd_palette, 0, PAGE_SIZE);
	memset((void *)lcd_frame0, 0, PAGE_SIZE << page_shift);

	
	dma_desc_base = (struct jz4750_lcd_dma_desc *)((void*)lcd_palette + ((PALETTE_SIZE+3)/4)*4);

#if defined(CONFIG_FB_JZ4750_SLCD)
	printk("=========== CONFIG FB JZ4750 sllcd.. \n");	

	lcd_cmdbuf = (unsigned char *)__get_free_pages(GFP_KERNEL, 0);
	memset((void *)lcd_cmdbuf, 0, PAGE_SIZE);

	{	int data, i, *ptr;
		ptr = (unsigned int *)lcd_cmdbuf;
		data = WR_GRAM_CMD;
		data = ((data & 0xff) << 1) | ((data & 0xff00) << 2);
		for(i = 0; i < 3; i++){
			ptr[i] = data;
		}
	}
#endif

#if defined(CONFIG_FB_JZ4750_LCD_USE_2LAYER_FRAMEBUFFER)
	lcd_frame1 = lcd_frame0 + needroom1;
#endif

	/*
	 * Set page reserved so that mmap will work. This is necessary
	 * since we'll be remapping normal memory.
	 */
	page = (unsigned long)lcd_palette;
	SetPageReserved(virt_to_page((void*)page));
	
	for (page = (unsigned long)lcd_frame0;
	     page < PAGE_ALIGN((unsigned long)lcd_frame0 + (PAGE_SIZE<<page_shift));
	     page += PAGE_SIZE) {
		SetPageReserved(virt_to_page((void*)page));
	}
        for (page = (unsigned long)lcd_frame01;
            page < PAGE_ALIGN((unsigned long)lcd_frame01 + (PAGE_SIZE<<page_shift));
            page += PAGE_SIZE) {
		SetPageReserved(virt_to_page((void*)page));
	}


	cfb->fb.fix.smem_start = virt_to_phys((void *)lcd_frame0);
	cfb->fb.fix.smem_len = (PAGE_SIZE << page_shift); /* page_shift/2 ??? */
	cfb->fb.screen_base =
		(unsigned char *)(((unsigned int)lcd_frame0&0x1fffffff) | 0xa0000000);

	if (!cfb->fb.screen_base) {
		printk("jz4750fb, %s: unable to map screen memory\n", cfb->fb.fix.id);
		return -ENOMEM;
	}

	return 0;
}

static void jz4750fb_free_fb_info(struct lcd_cfb_info *cfb)
{
	if (cfb) {
		fb_alloc_cmap(&cfb->fb.cmap, 0, 0);
		kfree(cfb);
	}
}

static void jz4750fb_unmap_smem(struct lcd_cfb_info *cfb)
{
	struct page * map = NULL;
	unsigned char *tmp;
	unsigned int page_shift, needroom, bpp, w, h;

	bpp = jz4750_lcd_info->osd.fg0.bpp;
	if ( bpp == 18 || bpp == 24)
		bpp = 32;
	if ( bpp == 15 )
		bpp = 16;
	w = jz4750_lcd_info->osd.fg0.w;
	h = jz4750_lcd_info->osd.fg0.h;
	needroom = ((w * bpp + 7) >> 3) * h;
#if defined(CONFIG_FB_JZ4750_LCD_USE_2LAYER_FRAMEBUFFER)
	bpp = jz4750_lcd_info->osd.fg1.bpp;
	if ( bpp == 18 || bpp == 24)
		bpp = 32;
	if ( bpp == 15 )
		bpp = 16;
	w = jz4750_lcd_info->osd.fg1.w;
	h = jz4750_lcd_info->osd.fg1.h;
	needroom += ((w * bpp + 7) >> 3) * h;
#endif

	for (page_shift = 0; page_shift < 12; page_shift++)
		if ((PAGE_SIZE << page_shift) >= needroom)
			break;

	if (cfb && cfb->fb.screen_base) {
		iounmap(cfb->fb.screen_base);
		cfb->fb.screen_base = NULL;
		release_mem_region(cfb->fb.fix.smem_start,
				   cfb->fb.fix.smem_len);
	}

	if (lcd_palette) {
		map = virt_to_page(lcd_palette);
		clear_bit(PG_reserved, &map->flags);
		free_pages((int)lcd_palette, 0);
	}

	if (lcd_frame0) {
		for (tmp=(unsigned char *)lcd_frame0; 
		     tmp < lcd_frame0 + (PAGE_SIZE << page_shift); 
		     tmp += PAGE_SIZE) {
			map = virt_to_page(tmp);
			clear_bit(PG_reserved, &map->flags);
		}
		free_pages((int)lcd_frame0, page_shift);
	}
}

/* initial dma descriptors */
static void jz4750fb_descriptor_init( struct jz4750lcd_info * lcd_info )
{
	unsigned int pal_size;

	switch ( lcd_info->osd.fg0.bpp ) {
	case 1:
		pal_size = 4;
		break;
	case 2:
		pal_size = 8;
		break;
	case 4:
		pal_size = 32;
		break;
	case 8:
	default:
		pal_size = 512;
	}

	pal_size /= 4;

	dma0_desc_palette 	= dma_desc_base + 0;
	dma0_desc0 		= dma_desc_base + 1;
	dma0_desc1 		= dma_desc_base + 2;
	dma0_desc_cmd0 		= dma_desc_base + 3; /* use only once */
	dma0_desc_cmd 		= dma_desc_base + 4;
	dma1_desc0 		= dma_desc_base + 5;
	dma1_desc1 		= dma_desc_base + 6;

	/*
	 * Normal TFT panel's DMA Chan0: 
	 *	TO LCD Panel: 	
	 * 		no palette:	dma0_desc0 <<==>> dma0_desc0 
	 * 		palette :	dma0_desc_palette <<==>> dma0_desc0
	 *	TO TV Encoder:
	 * 		no palette:	dma0_desc0 <<==>> dma0_desc1
	 * 		palette:	dma0_desc_palette --> dma0_desc0 
	 * 				--> dma0_desc1 --> dma0_desc_palette --> ...
	 * 				
	 * SMART LCD TFT panel(dma0_desc_cmd)'s DMA Chan0: 
	 *	TO LCD Panel:
	 * 		no palette:	dma0_desc_cmd <<==>> dma0_desc0 
	 * 		palette :	dma0_desc_palette --> dma0_desc_cmd
	 * 				--> dma0_desc0 --> dma0_desc_palette --> ...
	 *	TO TV Encoder:
	 * 		no palette:	dma0_desc_cmd --> dma0_desc0 
	 * 				--> dma0_desc1 --> dma0_desc_cmd --> ...
	 * 		palette:	dma0_desc_palette --> dma0_desc_cmd 
	 * 				--> dma0_desc0 --> dma0_desc1 
	 * 				--> dma0_desc_palette --> ...
	 * DMA Chan1:
	 *	TO LCD Panel:
	 * 		dma1_desc0 <<==>> dma1_desc0 
	 *	TO TV Encoder:
	 * 		dma1_desc0 <<==>> dma1_desc1
	 */

#if defined(CONFIG_FB_JZ4750_SLCD)
	/* First CMD descriptors, use only once, cmd_num isn't 0 */
	dma0_desc_cmd0->next_desc 	= (unsigned int)virt_to_phys(dma0_desc0);
	dma0_desc_cmd0->databuf 	= (unsigned int)virt_to_phys((void *)lcd_cmdbuf);
	dma0_desc_cmd0->frame_id 	= (unsigned int)0x0da0cad0; /* dma0's cmd0 */
	dma0_desc_cmd0->cmd 		= LCD_CMD_CMD | 3; /* command */
	//dma0_desc_cmd0->cmd 		= LCD_CMD_CMD | 0; /* command */
	dma0_desc_cmd0->offsize 	= 0;
	dma0_desc_cmd0->page_width 	= 0; 
	dma0_desc_cmd0->cmd_num 	= 3;
	//dma0_desc_cmd0->cmd_num 	= 0;


	/* Dummy Command Descriptor, cmd_num is 0 */
	dma0_desc_cmd->next_desc 	= (unsigned int)virt_to_phys(dma0_desc0);
	dma0_desc_cmd->databuf 		= 0; 
	dma0_desc_cmd->frame_id 	= (unsigned int)0x0da000cd; /* dma0's cmd0 */
	dma0_desc_cmd->cmd 		= LCD_CMD_CMD | 0; /* dummy command */
	dma0_desc_cmd->cmd_num 		= 0;
	dma0_desc_cmd->offsize 		= 0; 
	dma0_desc_cmd->page_width 	= 0; 

	/* Palette Descriptor */
	dma0_desc_palette->next_desc 	= (unsigned int)virt_to_phys(dma0_desc_cmd0);
#else
	/* Palette Descriptor */
	dma0_desc_palette->next_desc 	= (unsigned int)virt_to_phys(dma0_desc0);
#endif
	dma0_desc_palette->databuf 	= (unsigned int)virt_to_phys((void *)lcd_palette);
	dma0_desc_palette->frame_id 	= (unsigned int)0xaaaaaaaa;
	dma0_desc_palette->cmd 		= LCD_CMD_PAL | pal_size; /* Palette Descriptor */

	/* DMA0 Descriptor0 */
	if ( lcd_info->panel.cfg & LCD_CFG_TVEN ) /* TVE mode */
		dma0_desc0->next_desc 	= (unsigned int)virt_to_phys(dma0_desc1);
	else{			/* Normal TFT LCD */
#if defined(CONFIG_FB_JZ4750_SLCD)
			dma0_desc0->next_desc = (unsigned int)virt_to_phys(dma0_desc_cmd);
			//dma0_desc0->next_desc = (unsigned int)virt_to_phys(dma0_desc0);
#else
			dma0_desc0->next_desc = (unsigned int)virt_to_phys(dma0_desc0);
#endif
	}

	//maddrone change here
	if(lcd_info->panel.cfg & LCD_CFG_TVEN)
	{
#ifdef TVOUT_2x
                lprint_dbg("tvout_640_480 is %d\n",tvout_640_480);
		if(tvout_640_480)
		dma0_desc0->databuf = virt_to_phys((void *)lcd_frame0);
		else
		dma0_desc0->databuf = virt_to_phys((void *)lcd_frame01);
		
		dma0_desc0->frame_id = (unsigned int)0x0000da00; /* DMA0'0 */
		//maddrone
		unsigned int frame_size0;
		//frame_size0 = (640 * 480 * 32) >> 3;
		frame_size0 = (640 * 480 * 16) >> 3;
		frame_size0 /= 4;
		dma0_desc0->cmd = frame_size0;
		dma0_desc0->desc_size = (480 << 16) | 640;
		dma0_desc0->offsize = 0;
		dma0_desc0->cmd_num = 0;
#else
		dma0_desc0->databuf = virt_to_phys((void *)lcd_frame0);
		dma0_desc0->frame_id = (unsigned int)0x0000da00; /* DMA0'0 */
		//maddrone
		unsigned int frame_size0;
		frame_size0 = (tvout_display_w * tvout_display_h * 16) >> 3;
		frame_size0 /= 4;
		dma0_desc0->cmd = frame_size0;
		dma0_desc0->desc_size = (tvout_display_h << 16) | tvout_display_w;
		dma0_desc0->offsize = 0;
		dma0_desc0->cmd_num = 0;
#endif
	}
	else
	{
		dma0_desc0->databuf = virt_to_phys((void *)lcd_frame0);
		dma0_desc0->frame_id = (unsigned int)0x0000da00; /* DMA0'0 */
		//maddrone
		unsigned int frame_size0;
		frame_size0 = (480 * 272 * 16) >> 3;
		frame_size0 /= 4;
		dma0_desc0->cmd = frame_size0;
		dma0_desc0->desc_size = (272 << 16) | 480;
		dma0_desc0->offsize = 0;
		dma0_desc0->cmd_num = 0;
         	}	

	/* DMA0 Descriptor1 */
	if ( lcd_info->panel.cfg & LCD_CFG_TVEN ) { /* TVE mode */
		
		printk("TV Enable Mode...\n");
		if (lcd_info->osd.fg0.bpp <= 8) /* load palette only once at setup */
			dma0_desc1->next_desc = (unsigned int)virt_to_phys(dma0_desc_palette);
		else
#if defined(CONFIG_FB_JZ4750_SLCD)  /* for smatlcd */
			dma0_desc1->next_desc = (unsigned int)virt_to_phys(dma0_desc_cmd);
#else
			dma0_desc1->next_desc = (unsigned int)virt_to_phys(dma0_desc0);
#endif
		dma0_desc1->frame_id = (unsigned int)0x0000da01; /* DMA0'1 */
	}

	if (lcd_info->osd.fg0.bpp <= 8) /* load palette only once at setup */
		REG_LCD_DA0 = virt_to_phys(dma0_desc_palette);
	else {
#if defined(CONFIG_FB_JZ4750_SLCD)  /* for smartlcd */
		REG_LCD_DA0 = virt_to_phys(dma0_desc_cmd0); //smart lcd
		//printk("SET REG_LCD_DA0 ======== \n");
		//REG_LCD_DA0 = virt_to_phys(dma0_desc_cmd); //smart lcd
#else
		REG_LCD_DA0 = virt_to_phys(dma0_desc0); //tft
#endif
	}

	/* DMA1 Descriptor0 */
	if ( lcd_info->panel.cfg & LCD_CFG_TVEN ) /* TVE mode */
		dma1_desc0->next_desc = (unsigned int)virt_to_phys(dma1_desc1);
	else			/* Normal TFT LCD */
		dma1_desc0->next_desc = (unsigned int)virt_to_phys(dma1_desc0);

	dma1_desc0->databuf = virt_to_phys((void *)lcd_frame1);
	dma1_desc0->frame_id = (unsigned int)0x0000da10; /* DMA1'0 */

	/* DMA1 Descriptor1 */
	if ( lcd_info->panel.cfg & LCD_CFG_TVEN ) { /* TVE mode */
		dma1_desc1->next_desc = (unsigned int)virt_to_phys(dma1_desc0);
		dma1_desc1->frame_id = (unsigned int)0x0000da11; /* DMA1'1 */
	}

	REG_LCD_DA1 = virt_to_phys(dma1_desc0);	/* set Dma-chan1's Descripter Addrress */
	dma_cache_wback_inv((unsigned int)(dma_desc_base), (DMA_DESC_NUM)*sizeof(struct jz4750_lcd_dma_desc));

#if 0
	/* Palette Descriptor */
	if ( lcd_info->panel.cfg & LCD_CFG_LCDPIN_SLCD ) 
//		dma0_desc_palette->next_desc = (unsigned int)virt_to_phys(dma0_desc_cmd);
		dma0_desc_palette->next_desc = (unsigned int)virt_to_phys(dma0_desc_cmd1);
	else
		dma0_desc_palette->next_desc = (unsigned int)virt_to_phys(dma0_desc0);
	dma0_desc_palette->databuf = (unsigned int)virt_to_phys((void *)lcd_palette);
	dma0_desc_palette->frame_id = (unsigned int)0xaaaaaaaa;
	dma0_desc_palette->cmd 	= LCD_CMD_PAL | pal_size; /* Palette Descriptor */

	/* Dummy Command Descriptor, cmd_num is 0 */
	dma0_desc_cmd->next_desc = (unsigned int)virt_to_phys(dma0_desc0);
	dma0_desc_cmd->databuf 	= (unsigned int)virt_to_phys((void *)lcd_cmdbuf);
	dma0_desc_cmd->frame_id = (unsigned int)0x0da0cad0; /* dma0's cmd0 */
	dma0_desc_cmd->cmd 	= LCD_CMD_CMD | 3; /* dummy command */
	dma0_desc_cmd->offsize 	= 0; /* dummy command */
	dma0_desc_cmd->page_width = 0; /* dummy command */
	dma0_desc_cmd->cmd_num 	= 3;

//---------------------------------
	dma0_desc_cmd1->next_desc = (unsigned int)virt_to_phys(dma0_desc0);
	dma0_desc_cmd1->databuf 	= 0; 
	dma0_desc_cmd1->frame_id = (unsigned int)0x0da0cad1; /* dma0's cmd0 */
	dma0_desc_cmd1->cmd 	= LCD_CMD_CMD | 0; /* dummy command */
	dma0_desc_cmd1->cmd_num 	= 0;
	dma0_desc_cmd1->offsize 	= 0; /* dummy command */
	dma0_desc_cmd1->page_width = 0; /* dummy command */
//-----------------------------------
	/* DMA0 Descriptor0 */
	if ( lcd_info->panel.cfg & LCD_CFG_TVEN ) /* TVE mode */
		dma0_desc0->next_desc = (unsigned int)virt_to_phys(dma0_desc1);
	else{			/* Normal TFT LCD */
		if (lcd_info->osd.fg0.bpp <= 8) /* load palette only once at setup?? */
//			dma0_desc0->next_desc = (unsigned int)virt_to_phys(dma0_desc_palette); //tft
			dma0_desc0->next_desc = (unsigned int)virt_to_phys(dma0_desc_cmd); // smart lcd
		else if ( lcd_info->panel.cfg & LCD_CFG_LCDPIN_SLCD ) 
			dma0_desc0->next_desc = (unsigned int)virt_to_phys(dma0_desc_cmd1);
//			dma0_desc0->next_desc = (unsigned int)virt_to_phys(dma0_desc_cmd);
		else 
			dma0_desc0->next_desc = (unsigned int)virt_to_phys(dma0_desc0);
	}

	dma0_desc0->databuf = virt_to_phys((void *)lcd_frame0);
	dma0_desc0->frame_id = (unsigned int)0x0000da00; /* DMA0'0 */

	/* DMA0 Descriptor1 */
	if ( lcd_info->panel.cfg & LCD_CFG_TVEN ) { /* TVE mode */
		if (lcd_info->osd.fg0.bpp <= 8) /* load palette only once at setup?? */
			dma0_desc1->next_desc = (unsigned int)virt_to_phys(dma0_desc_palette);
		
		else if ( lcd_info->panel.cfg & LCD_CFG_LCDPIN_SLCD ) 
			dma0_desc1->next_desc = (unsigned int)virt_to_phys(dma0_desc_cmd);
		else 
			dma0_desc1->next_desc = (unsigned int)virt_to_phys(dma0_desc0);
		dma0_desc1->frame_id = (unsigned int)0x0000da01; /* DMA0'1 */
	}

	/* DMA1 Descriptor0 */
	if ( lcd_info->panel.cfg & LCD_CFG_TVEN ) /* TVE mode */
		dma1_desc0->next_desc = (unsigned int)virt_to_phys(dma1_desc1);
	else			/* Normal TFT LCD */
		dma1_desc0->next_desc = (unsigned int)virt_to_phys(dma1_desc0);

	dma1_desc0->databuf = virt_to_phys((void *)lcd_frame1);
	dma1_desc0->frame_id = (unsigned int)0x0000da10; /* DMA1'0 */

	/* DMA1 Descriptor1 */
	if ( lcd_info->panel.cfg & LCD_CFG_TVEN ) { /* TVE mode */
		dma1_desc1->next_desc = (unsigned int)virt_to_phys(dma1_desc0);
		dma1_desc1->frame_id = (unsigned int)0x0000da11; /* DMA1'1 */
	}

	if (lcd_info->osd.fg0.bpp <= 8) /* load palette only once at setup?? */
		REG_LCD_DA0 = virt_to_phys(dma0_desc_palette);
	else
//		REG_LCD_DA0 = virt_to_phys(dma0_desc_cmd); //smart lcd
		REG_LCD_DA0 = virt_to_phys(dma0_desc0); //tft
	REG_LCD_DA1 = virt_to_phys(dma1_desc0);	/* set Dma-chan1's Descripter Addrress */
	dma_cache_wback_inv((unsigned int)(dma_desc_base), (DMA_DESC_NUM)*sizeof(struct jz4750_lcd_dma_desc));
#endif
}

static void jz4750fb_set_panel_mode( struct jz4750lcd_info * lcd_info )
{
	struct jz4750lcd_panel_t *panel = &lcd_info->panel;
#ifdef CONFIG_JZ4750D_VGA_DISPLAY
	REG_TVE_CTRL |= TVE_CTRL_DAPD;
	REG_TVE_CTRL &= ~( TVE_CTRL_DAPD1 | TVE_CTRL_DAPD2 | TVE_CTRL_DAPD3);
#endif
	/* set bpp */
	lcd_info->panel.ctrl &= ~LCD_CTRL_BPP_MASK;
	if ( lcd_info->osd.fg0.bpp == 1 )
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_1;
	else if ( lcd_info->osd.fg0.bpp == 2 )
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_2;
	else if ( lcd_info->osd.fg0.bpp == 4 )
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_4;
	else if ( lcd_info->osd.fg0.bpp == 8 )
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_8;
	else if ( lcd_info->osd.fg0.bpp == 15 )
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_16 | LCD_CTRL_RGB555;
	else if ( lcd_info->osd.fg0.bpp == 16 )
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_16 | LCD_CTRL_RGB565;
	else if ( lcd_info->osd.fg0.bpp > 16 && lcd_info->osd.fg0.bpp < 32+1 ) {
		lcd_info->osd.fg0.bpp = 32;
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_18_24;
	}
	else {
		printk("The BPP %d is not supported\n", lcd_info->osd.fg0.bpp);
		lcd_info->osd.fg0.bpp = 32;
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_18_24;
	}

	lcd_info->panel.cfg |= LCD_CFG_NEWDES; /* use 8words descriptor always */

	REG_LCD_CTRL = lcd_info->panel.ctrl; /* LCDC Controll Register */
	REG_LCD_CFG = lcd_info->panel.cfg; /* LCDC Configure Register */
	REG_SLCD_CFG = lcd_info->panel.slcd_cfg; /* Smart LCD Configure Register */
	
	if ( lcd_info->panel.cfg & LCD_CFG_LCDPIN_SLCD ) /* enable Smart LCD DMA */
		REG_SLCD_CTRL = SLCD_CTRL_DMA_EN;

	switch ( lcd_info->panel.cfg & LCD_CFG_MODE_MASK ) {
	case LCD_CFG_MODE_GENERIC_TFT:
	case LCD_CFG_MODE_INTER_CCIR656:
	case LCD_CFG_MODE_NONINTER_CCIR656:
	case LCD_CFG_MODE_SLCD:
	default:		/* only support TFT16 TFT32, not support STN and Special TFT by now(10-06-2008)*/
		REG_LCD_VAT = (((panel->blw + panel->w + panel->elw + panel->hsw)) << 16) | (panel->vsw + panel->bfw + panel->h + panel->efw);
		REG_LCD_DAH = ((panel->hsw + panel->blw) << 16) | (panel->hsw + panel->blw + panel->w);
		REG_LCD_DAV = ((panel->vsw + panel->bfw) << 16) | (panel->vsw + panel->bfw + panel->h);
		REG_LCD_HSYNC = (0 << 16) | panel->hsw;
		REG_LCD_VSYNC = (0 << 16) | panel->vsw;
		break;
	}
}


static void jz4750fb_set_osd_mode( struct jz4750lcd_info * lcd_info )
{
	dprintk("%s, %d\n", __FILE__, __LINE__ );
	lcd_info->osd.osd_ctrl &= ~(LCD_OSDCTRL_OSDBPP_MASK);
	if ( lcd_info->osd.fg1.bpp == 15 )
		lcd_info->osd.osd_ctrl |= LCD_OSDCTRL_OSDBPP_15_16|LCD_OSDCTRL_RGB555;
	else if ( lcd_info->osd.fg1.bpp == 16 )
		lcd_info->osd.osd_ctrl |= LCD_OSDCTRL_OSDBPP_15_16|LCD_OSDCTRL_RGB565;
	else {
		lcd_info->osd.fg1.bpp = 32;
		lcd_info->osd.osd_ctrl |= LCD_OSDCTRL_OSDBPP_18_24;
	}

	REG_LCD_OSDC 	= lcd_info->osd.osd_cfg; /* F0, F1, alpha, */

	REG_LCD_OSDCTRL = lcd_info->osd.osd_ctrl; /* IPUEN, bpp */
	REG_LCD_RGBC  	= lcd_info->osd.rgb_ctrl;
	REG_LCD_BGC  	= lcd_info->osd.bgcolor;
	REG_LCD_KEY0 	= lcd_info->osd.colorkey0;
	REG_LCD_KEY1 	= lcd_info->osd.colorkey1;
	REG_LCD_ALPHA 	= lcd_info->osd.alpha;
	REG_LCD_IPUR 	= lcd_info->osd.ipu_restart;
}

static void jz4750fb_foreground_resize( struct jz4750lcd_info * lcd_info )
{
	int fg0_line_size, fg0_frm_size, fg1_line_size, fg1_frm_size;
	/* 
	 * NOTE: 
	 * Foreground change sequence: 
	 * 	1. Change Position Registers -> LCD_OSDCTL.Change;
	 * 	2. LCD_OSDCTRL.Change -> descripter->Size
	 * Foreground, only one of the following can be change at one time:
	 * 	1. F0 size;
	 *	2. F0 position
	 * 	3. F1 size
	 *	4. F1 position
	 */
	
	/* 
	 * The rules of f0, f1's position:
	 * 	f0.x + f0.w <= panel.w;
	 * 	f0.y + f0.h <= panel.h;
	 *
	 * When output is LCD panel, fg.y and fg.h can be odd number or even number.
	 * When output is TVE, as the TVE has odd frame and even frame,
	 * to simplified operation, fg.y and fg.h should be even number always.
	 *
	 */
        lprint_dbg("the fg0x is %d fg0y is %d fg0w is %d fgoh is %d\n",lcd_info->osd.fg0.x,lcd_info->osd.fg0.y,lcd_info->osd.fg0.w,lcd_info->osd.fg0.h);
        lprint_dbg("the fg1x is %d fg1y is %d fg1w is %d fg1h is %d\n",lcd_info->osd.fg1.x,lcd_info->osd.fg1.y,lcd_info->osd.fg1.w,lcd_info->osd.fg1.h);
	/* Foreground 0  */
	if ( lcd_info->osd.fg0.x >= lcd_info->panel.w )
		lcd_info->osd.fg0.x = lcd_info->panel.w;
	if ( lcd_info->osd.fg0.y >= lcd_info->panel.h )
		lcd_info->osd.fg0.y = lcd_info->panel.h;
	if ( lcd_info->osd.fg0.x + lcd_info->osd.fg0.w > lcd_info->panel.w )
		lcd_info->osd.fg0.w = lcd_info->panel.w - lcd_info->osd.fg0.x;
	if ( lcd_info->osd.fg0.y + lcd_info->osd.fg0.h > lcd_info->panel.h ) 
		lcd_info->osd.fg0.h = lcd_info->panel.h - lcd_info->osd.fg0.y;
	/* Foreground 1 */
	/* Case TVE ??? TVE 720x573 or 720x480*/
	if ( lcd_info->osd.fg1.x >= lcd_info->panel.w ) 
		lcd_info->osd.fg1.x = lcd_info->panel.w;
	if ( lcd_info->osd.fg1.y >= lcd_info->panel.h ) 
		lcd_info->osd.fg1.y = lcd_info->panel.h;
	if ( lcd_info->osd.fg1.x + lcd_info->osd.fg1.w > lcd_info->panel.w ) 
		lcd_info->osd.fg1.w = lcd_info->panel.w - lcd_info->osd.fg1.x;
	if ( lcd_info->osd.fg1.y + lcd_info->osd.fg1.h > lcd_info->panel.h ) 
		lcd_info->osd.fg1.h = lcd_info->panel.h - lcd_info->osd.fg1.y;

//	fg0_line_size = lcd_info->osd.fg0.w*((lcd_info->osd.fg0.bpp+7)/8);
	fg0_line_size = (lcd_info->osd.fg0.w*(lcd_info->osd.fg0.bpp)/8);
	fg0_line_size = ((fg0_line_size+3)>>2)<<2; /* word aligned */
	fg0_frm_size = fg0_line_size * lcd_info->osd.fg0.h;
	
	printk("fg0_frm_size = 0x%x\n",fg0_frm_size);

	fg1_line_size = lcd_info->osd.fg1.w*((lcd_info->osd.fg1.bpp+7)/8);
	fg1_line_size = ((fg1_line_size+3)>>2)<<2; /* word aligned */
	fg1_frm_size = fg1_line_size * lcd_info->osd.fg1.h;

	if ( lcd_info->osd.fg_change ) {
		if ( lcd_info->osd.fg_change & FG0_CHANGE_POSITION ) { /* F0 change position */
			REG_LCD_XYP0 = lcd_info->osd.fg0.y << 16 | lcd_info->osd.fg0.x;
		}
		if ( lcd_info->osd.fg_change & FG1_CHANGE_POSITION ) { /* F1 change position */
			REG_LCD_XYP1 = lcd_info->osd.fg1.y << 16 | lcd_info->osd.fg1.x;
		}

		/* set change */
		if ( !(lcd_info->osd.osd_ctrl & LCD_OSDCTRL_IPU) && 
		     (lcd_info->osd.fg_change != FG_CHANGE_ALL) )
			REG_LCD_OSDCTRL |= LCD_OSDCTRL_CHANGES;

		/* wait change ready???  maddrone open*/ 
		while ( REG_LCD_OSDS & LCD_OSDS_READY )	/* fix in the future, Wolfgang, 06-20-2008 */
		print_dbg("wait LCD_OSDS_READY\n");
		
		if ( lcd_info->osd.fg_change & FG0_CHANGE_SIZE ) { /* change FG0 size */
			if ( lcd_info->panel.cfg & LCD_CFG_TVEN ) { /* output to TV */
				dma0_desc0->cmd = dma0_desc1->cmd = (fg0_frm_size/4)/2;
				dma0_desc0->offsize = dma0_desc1->offsize 
					= fg0_line_size/4;
				dma0_desc0->page_width = dma0_desc1->page_width 
					= fg0_line_size/4;
				#ifdef TVOUT_2x
				if(tvout_640_480)
				dma0_desc1->databuf = virt_to_phys((void *)(lcd_frame0 + fg0_line_size));  //maddrone
				else
				dma0_desc1->databuf = virt_to_phys((void *)(lcd_frame01 + fg0_line_size));  //maddrone
				#else
				dma0_desc1->databuf = virt_to_phys((void *)(lcd_frame0 + fg0_line_size));
				#endif
				REG_LCD_DA0 = virt_to_phys(dma0_desc0); //tft
			}
			else {
				dma0_desc0->cmd = dma0_desc1->cmd = fg0_frm_size/4;
				dma0_desc0->offsize = dma0_desc1->offsize =0;
				dma0_desc0->page_width = dma0_desc1->page_width = 0;
			}

			dma0_desc0->desc_size = dma0_desc1->desc_size 
				= lcd_info->osd.fg0.h << 16 | lcd_info->osd.fg0.w;
			REG_LCD_SIZE0 = (lcd_info->osd.fg0.h<<16)|lcd_info->osd.fg0.w;

		}

		if ( lcd_info->osd.fg_change & FG1_CHANGE_SIZE ) { /* change FG1 size*/
			if ( lcd_info->panel.cfg & LCD_CFG_TVEN ) { /* output to TV */
				dma1_desc0->cmd = dma1_desc1->cmd = (fg1_frm_size/4)/2;
				dma1_desc0->offsize = dma1_desc1->offsize = fg1_line_size/4;
				dma1_desc0->page_width = dma1_desc1->page_width = fg1_line_size/4;
				dma1_desc1->databuf = virt_to_phys((void *)(lcd_frame1 + fg1_line_size));  //maddrone
				REG_LCD_DA1 = virt_to_phys(dma0_desc1); //tft

			}
			else {
				dma1_desc0->cmd = dma1_desc1->cmd = fg1_frm_size/4;
				dma1_desc0->offsize = dma1_desc1->offsize = 0;
				dma1_desc0->page_width = dma1_desc1->page_width = 0;
			}
			
			dma1_desc0->desc_size = dma1_desc1->desc_size 
				= lcd_info->osd.fg1.h << 16 | lcd_info->osd.fg1.w;
			REG_LCD_SIZE1 = lcd_info->osd.fg1.h << 16|lcd_info->osd.fg1.w;
		}

		dma_cache_wback((unsigned int)(dma_desc_base), (DMA_DESC_NUM)*sizeof(struct jz4750_lcd_dma_desc));
		lcd_info->osd.fg_change = FG_NOCHANGE; /* clear change flag */
	}
}

static void jz4750fb_change_clock( struct jz4750lcd_info * lcd_info )
{
	unsigned int val = 0;
	unsigned int pclk;
	/* Timing setting */
	__cpm_stop_lcd();

	val = lcd_info->panel.fclk; /* frame clk */

	if ( (lcd_info->panel.cfg & LCD_CFG_MODE_MASK) != LCD_CFG_MODE_SERIAL_TFT) {
		pclk = val * (lcd_info->panel.w + lcd_info->panel.hsw + lcd_info->panel.elw + lcd_info->panel.blw) * (lcd_info->panel.h + lcd_info->panel.vsw + lcd_info->panel.efw + lcd_info->panel.bfw); /* Pixclk */
	}
	else {
		/* serial mode: Hsync period = 3*Width_Pixel */
		pclk = val * (lcd_info->panel.w*3 + lcd_info->panel.hsw + lcd_info->panel.elw + lcd_info->panel.blw) * (lcd_info->panel.h + lcd_info->panel.vsw + lcd_info->panel.efw + lcd_info->panel.bfw); /* Pixclk */
	}

	/********* In TVE mode PCLK = 27MHz ***********/
	if ( lcd_info->panel.cfg & LCD_CFG_TVEN ) { 		/* LCDC output to TVE */
		REG_CPM_LPCDR  &= (~CPM_LPCDR_LSCS);   //maddrone add
		REG_CPM_LPCDR  |= CPM_LPCDR_LTCS;
		pclk = 27000000;
		//val = __cpm_get_pllout2() / pclk; /* pclk */
		val = 432000000 / pclk; /* pclk */
		printk("maddrone tve: pllout2 = 0x%x\n", __cpm_get_pllout2());
		val--;
		__cpm_set_pixdiv(val);

		dprintk("REG_CPM_LPCDR = 0x%08x\n", REG_CPM_LPCDR);
#if defined(CONFIG_SOC_JZ4750) /* Jz4750D don't use LCLK */
		val = pclk * 3 ;	/* LCDClock > 2.5*Pixclock */

		val =(__cpm_get_pllout()) / val;
		if ( val > 0x1f ) {
			printk("lcd clock divide is too large, set it to 0x1f\n");
			val = 0x1f;
		}
		__cpm_set_ldiv( val );
#endif
		__cpm_select_pixclk_tve();

		REG_CPM_LPCDR  |= CPM_LPCDR_LTCS;  //maddrone add
		REG_CPM_CPCCR |= CPM_CPCCR_CE ; /* update divide */
	}
        else { 		/* LCDC output to  LCD panel */
                //pclk = 12000000;
                pclk = 22000000;
		val = __cpm_get_pllout2() / pclk; /* pclk */
                printk("carlos tve: pllout2 = %d plck is %d\n", __cpm_get_pllout2(),pclk);
		val--;
		dprintk("ratio: val = %d\n", val);
		if ( val > 0x7ff ) {
			printk("pixel clock divid is too large, set it to 0x7ff\n");
			val = 0x7ff;
		}
		__cpm_set_pixdiv(val);
		dprintk("REG_CPM_LPCDR = 0x%08x\n", REG_CPM_LPCDR);
#if defined(CONFIG_SOC_JZ4750) /* Jz4750D don't use LCLK */
		val = pclk * 3 ;	/* LCDClock > 2.5*Pixclock */
		val =__cpm_get_pllout2() / val;
		if ( val > 0x1f ) {
			printk("lcd clock divide is too large, set it to 0x1f\n");
			val = 0x1f;
		}
		__cpm_set_ldiv( val );
#endif
		REG_CPM_CPCCR |= CPM_CPCCR_CE ; /* update divide */
		
	}

	dprintk("REG_CPM_LPCDR=0x%08x\n", REG_CPM_LPCDR);
	dprintk("REG_CPM_CPCCR=0x%08x\n", REG_CPM_CPCCR);

	jz_clocks.pixclk = __cpm_get_pixclk();
	printk("LCDC: PixClock:%d\n", jz_clocks.pixclk);

#if defined(CONFIG_SOC_JZ4750) /* Jz4750D don't use LCLK */
	jz_clocks.lcdclk = __cpm_get_lcdclk();
	printk("LCDC: LcdClock:%d\n", jz_clocks.lcdclk);
#endif
	__cpm_start_lcd();
	udelay(1000);
	/* 
	 * set lcd device clock and lcd pixel clock.
	 * what about TVE mode???
	 *
	 */
}

/* 
 * jz4750fb_set_mode(), set osd configure, resize foreground
 *
 */
static void jz4750fb_set_mode( struct jz4750lcd_info * lcd_info )
{
	struct lcd_cfb_info *cfb = jz4750fb_info;

	jz4750fb_set_osd_mode(lcd_info);
	jz4750fb_foreground_resize(lcd_info);
	jz4750fb_set_var(&cfb->fb.var, -1, &cfb->fb);
}

/**
 * @brief jz4750fb_deep_set_mode 
 *
 * @param lcd_info
 */
static void jz4750fb_deep_set_mode( struct jz4750lcd_info * lcd_info )
{
	/* configurate sequence:
	 * 1. disable lcdc.
	 * 2. init frame descriptor.
	 * 3. set panel mode
	 * 4. set osd mode
	 * 5. start lcd clock in CPM
	 * 6. enable lcdc.
	 */

        printk("In jz4750fb_deep_set_mode  \n");
        lprint_dbg("\n");

        __lcd_set_dis(); /* regular disable */
        int cnt = 0xfffe;
#define NORMAL_DISABLE 0
#define  QUICH_DISABLE 0
#if QUICH_DISABLE
        sdf
        while(!__lcd_quick_disable_done() && cnt > 1) {
          cnt--;
        }
#elif  NORMAL_DISABLE
        while(!__lcd_disable_done() && cnt > 1) {
          cnt--;
        }
#endif
        mdelay(50);
        lprint_dbg("cnt is %d\n",cnt);
	//__slcd_disable_dma();   //maddrone add
	lcd_info->osd.fg_change = FG_CHANGE_ALL; /* change FG0, FG1 size, postion??? */
	jz4750fb_descriptor_init(lcd_info);
	jz4750fb_set_panel_mode(lcd_info);
	jz4750fb_set_mode(lcd_info);
	jz4750fb_change_clock(lcd_info);
	//__slcd_enable_dma();   //maddrone add
	//REG_SLCD_CTRL |= SLCD_CTRL_DMA_EN; //maddrone add
        __lcd_clr_dis();
	__lcd_set_ena();	/* enable lcdc */
	printk("Out jz4750fb_deep_set_mode  \n");
	
}

static irqreturn_t jz4750fb_interrupt_handler(int irq, void *dev_id)
{
	unsigned int state;
	static int irqcnt=0;

	state = REG_LCD_STATE;
	dprintk("In the lcd interrupt handler, state=0x%x\n", state);

	if (state & LCD_STATE_EOF) /* End of frame */
		REG_LCD_STATE = state & ~LCD_STATE_EOF;

	if (state & LCD_STATE_IFU0) {
		printk("%s, InFiFo0 underrun\n", __FUNCTION__);
		REG_LCD_STATE = state & ~LCD_STATE_IFU0;
	}

	if (state & LCD_STATE_IFU1) {
		printk("%s, InFiFo1 underrun\n", __FUNCTION__);
		REG_LCD_STATE = state & ~LCD_STATE_IFU1;
	}

	if (state & LCD_STATE_OFU) { /* Out fifo underrun */
		REG_LCD_STATE = state & ~LCD_STATE_OFU;
		if ( irqcnt++ > 100 ) {
			__lcd_disable_ofu_intr();
			printk("disable Out FiFo underrun irq.\n");
		}
		printk("%s, Out FiFo underrun.\n", __FUNCTION__);
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_PM
/*
 * Suspend the LCDC.
 */
static int jzfb_suspend(void)
{
	__lcd_clr_ena(); /* Quick Disable */
	__lcd_display_off();
	__cpm_stop_lcd();

	return 0;
}

/*
 * Resume the LCDC.
 */
static int jzfb_resume(void)
{
	__cpm_start_lcd();
	
	//maddrone 
	//__gpio_set_pin(GPIO_DISP_OFF_N); 
	
    __lcd_special_on();
    //__lcd_slcd_special_on();
	__lcd_set_ena();
	mdelay(200);
	__lcd_set_backlight_level(80);

	return 0;
}

/*
 * Power management hook.  Note that we won't be called from IRQ context,
 * unlike the blank functions above, so we may sleep.
 */
static int jzlcd_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	int ret;
	struct lcd_cfb_info *cfb = pm_dev->data;

	if (!cfb) return -EINVAL;

	switch (req) {
	case PM_SUSPEND:
		ret = jzfb_suspend();
		break;

	case PM_RESUME:
		ret = jzfb_resume();
		break;

	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
#else
#define jzfb_suspend      NULL
#define jzfb_resume       NULL
#endif /* CONFIG_PM */

/* The following routine is only for test */

//#ifdef DEBUG
#if 0
static void test_gpio(int gpio_num, int delay)	{
	__gpio_as_output(gpio_num);
	while(1) {
		__gpio_set_pin(gpio_num);
		udelay(delay);
		__gpio_clear_pin(gpio_num);
		udelay(delay);
	}
}
static void display_v_color_bar(int w, int h, int bpp) {
	int i, j, wpl, data = 0;
	int *ptr;
	ptr = (int *)lcd_frame0;
//	ptr = (int *)lcd_frame1;
	wpl = w*bpp/32;
	if (!(bpp > 8))
		switch(bpp){
		case 1:
			for (j = 0;j < h; j++)
				for (i = 0;i < wpl; i++) {
					*ptr++ = 0x00ff00ff;
				}
			break;
		case 2:
			for (j = 0;j < h; j++)
				for (i = 0;i < wpl; i++) {
					data = (i%4)*0x55555555;
					*ptr++ = data;
				}
			break;
		case 4:
			for (j = 0;j < h; j++)
				for (i = 0;i < wpl; i++) {
					data = (i%16)*0x11111111;
					*ptr++ = data;
				}
			break;
		case 8:
			for (j = 0;j < h; j++)
				for (i = 0;i < wpl; i+=2) {
					data = (i%(256))*0x01010101;
					*ptr++ = data;
					*ptr++ = data;
				}
			break;
		}
	else {
		switch(bpp) {
		case 16:
			for (j = 0;j < h; j++)
				for (i = 0;i < wpl; i++) {
					if((i/4)%8==0)
						*ptr++ = 0xffffffff;
					else if ((i/4)%8==1)
						*ptr++ = 0xf800f800;
					else if ((i/4)%8==2)
						*ptr++ = 0xffe0ffe0;
					else if ((i/4)%8==3)
						*ptr++ = 0x07e007e0;
					else if ((i/4)%8==4)
						*ptr++ = 0x07ff07ff;
					else if ((i/4)%8==5)
						*ptr++ = 0x001f001f;
					else if ((i/4)%8==6)
						*ptr++ = 0xf81ff81f;
					else if ((i/4)%8==7)
						*ptr++ = 0x00000000;
				}
			break;
		case 18:
		case 24:
		case 32:
		default:
#if 1
			for (j = 0;j < h; j++)
				for (i = 0;i < wpl; i++) {
					if((i/8)%8==7) 
						*ptr++ = 0xffffff;
					else if ((i/8)%8==1)
						*ptr++ = 0xff0000;
					else if ((i/8)%8==2)
						*ptr++ = 0xffff00;
					else if ((i/8)%8==3)
						*ptr++ = 0x00ff00;
					else if ((i/8)%8==4)
						*ptr++ = 0x00ffff;
					else if ((i/8)%8==5)
						*ptr++ = 0x0000ff;
					else if ((i/8)%8==6)
						*ptr++ = 0xff00ff;
					else if ((i/8)%8==0)
						*ptr++ = 0x000000;
				}
#else
			for (j = 0;j < h; j++)
				for (i = 0;i < wpl; i++) {
					if((i/8)%8==7) 
						*ptr++ = 0x00ff0000;
					else if ((i/8)%8==1)
						*ptr++ = 0xffff0000;
					else if ((i/8)%8==2)
						*ptr++ = 0x20ff0000;
					else if ((i/8)%8==3)
						*ptr++ = 0x40ff0000;
					else if ((i/8)%8==4)
						*ptr++ = 0x60ff0000;
					else if ((i/8)%8==5)
						*ptr++ = 0x80ff0000;
					else if ((i/8)%8==6)
						*ptr++ = 0xa0ff0000;
					else if ((i/8)%8==0)
						*ptr++ = 0xc0ff0000;
				}
#endif
			break;
		}
	}
}

static void 
display_h_color_bar(int w, int h, int bpp) 
{
  //int i,j, data = 0;
  //int *ptr,*ptr2;
  //int wpl; //word_per_line
  //ptr = (int *)lcd_frame0;
  //printk("========== Test H Color BAR ============\n");
  //ptr2 = (int *)lcd_frame1;
  //wpl = w*bpp/32;

  //memset((unsigned char *)ptr, 320*240*2, 0x0F);
  //dma_cache_wback((unsigned int)(ptr), 320 * 240 * 2);
  //memset((unsigned char *)ptr2, 320*240*4, 0xF0);

  //for(i=0; i<320*240; i++) Mcupanel_Data(0xF0F0);				
  printk("========== Test H Color BAR Over ============\n");
#if 0
  short open_pic0[400][240];
  short open_pic1[400][240];
  short open_pic2[400][240];
  short open_pic3[400][240];
  short open_pic4[400][240];
  short open_pic5[400][240];
  short open_pic6[400][240];
  short open_pic7[400][240];
  short open_pic8[400][240];
  short open_pic9[400][240];
#endif
printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
#if 0 
  memset((char *)open_pic0,0x00,400*240*2);
 printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
  memset(open_pic1,0xf8,400*240*2);
  printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
 memset(open_pic2,0x20,400*240*2);
  printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
 memset(open_pic3,0x30,400*240*2);
  printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
 memset(open_pic4,0x01,400*240*2);
  printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
 memset(open_pic5,0x02,400*240*2);
  printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
 memset(open_pic6,0x03,400*240*2);
  printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
 memset(open_pic7,0x0f,400*240*2);
  printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
 memset(open_pic8,0xf0,400*240*2);
  printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
 memset(open_pic9,0xff,400*240*2);
  printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
#endif
 //while(1);
  int i,j;
  unsigned short *p;
  int k = 0;

  p = (unsigned short *)lcd_frame0;
  //memcpy(p,open_pic0,400*240*2);
  printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
 if(0)
  {
    //red
    for(i=0; i<80; i++)
      for(j=0; j<400; j++)
        p[k++] = 0xF800;
    //p[k++] = 0xFF00;    //green
    //p[k++] = 0xFF;    //purple

#if 1
    //green
    for(i=0; i<80; i++)
      for(j=0; j<400; j++)
        p[k++] = 0x07E0;

    //blue
    for(i=0; i<80; i++)
      for(j=0; j<400; j++)
        p[k++] = 0x001F;
  }
  dma_cache_wback((unsigned int)(lcd_frame0), 400 * 240);
printk("====%d====== Test H Color BAR Over ============\n",__LINE__);
 
#endif

#if 0
  if (!(bpp > 8))
    for (i = 0;i < wpl*h;i++) {
      switch(bpp){
        case 1:
          if(i%(wpl*8)==0)
            data = ((i/(wpl*8))%2)*0xffffffff;
          *ptr++ = data; 
          break;
        case 2:
          if(i%(wpl*8)==0)
            data = ((i/(wpl*8))%4)*0x55555555;
          *ptr++ = data; 
          break;
        case 4:
          if(i%(wpl*8)==0)
            data = ((i/(wpl*8))%16)*0x11111111;
          *ptr++ = data; 
          break;
        case 8:
          if(i%(wpl*8)==0)
            data = ((i/(wpl*8))%256)*0x01010101;
          *ptr++ = data; 
          break;
      }
    }
  else {

    switch(bpp) {
      case 15:
      case 16:
        for (i = 0;i < wpl*h;i++) {
          if (((i/(wpl*8)) % 8) == 0)
            *ptr++ = 0xffffffff;
          else if (((i/(wpl*8)) % 8) == 1)
            *ptr++ = 0xf800f800;
          else if (((i/(wpl*8)) % 8) == 2)
            *ptr++ = 0xffe0ffe0;
          else if (((i/(wpl*8)) % 8) == 3)
            *ptr++ = 0x07e007e0;
          else if (((i/(wpl*8)) % 8) == 4)
            *ptr++ = 0x07ff07ff;
          else if (((i/(wpl*8)) % 8) == 5)
            *ptr++ = 0x001f001f;
          else if (((i/(wpl*8)) % 8) == 6)
            *ptr++ = 0xf81ff81f;
          else if (((i/(wpl*8)) % 8) == 7)
            *ptr++ = 0x00000000;
        }
        break;
      case 18:
      case 24:
      case 32:
      default:
        for (i = 0;i < wpl*h;i++) {
          if (((i/(wpl*8)) % 8) == 7)
            *ptr++ = 0xffffff;
          else if (((i/(wpl*8)) % 8) == 2)
            *ptr++ = 0xff0000;
          else if (((i/(wpl*8)) % 8) == 4)
            *ptr++ = 0xffff00;
          else if (((i/(wpl*8)) % 8) == 6)
            *ptr++ = 0x00ff00;
          else if (((i/(wpl*8)) % 8) == 1)
            *ptr++ = 0x00ffff;
          else if (((i/(wpl*8)) % 8) == 3)
            *ptr++ = 0x0000ff;
          else if (((i/(wpl*8)) % 8) == 5)
            *ptr++ = 0x000000;
          else if (((i/(wpl*8)) % 8) == 0)
            *ptr++ = 0xff00ff;
        }
        break;
    }

  }
#endif

}
#endif	

static unsigned int l009_backlight = 100;
void draw_lock_picture(void) 
{
	int i,j;
	unsigned short *p;
	unsigned short *q;
	p = (unsigned short *)lcd_frame0;
	q = (unsigned short *)lockpic;

	p = p + 100*480 + 215; 
#define PIC_HEIGHT 70
	for(i = 0; i < PIC_HEIGHT-3; i++)
	{
		
		for(j = 0; j < PIC_HEIGHT; j++)
		{
#if 0	
			while( *q == 0xf800 && j < PIC_HEIGHT)
			{  k++;
				q++;
				j++; }
#endif
				if((*q &0xf000) == 0xf000||(*q & 0xe000) == 0xe000||(*q & 0xd000) == 0xd000||(*q & 0xc000) == 0xc000)
				{
					*p++;
					*q++;
				}
				else
				{

					*p++ = *q++;
				}
		}
		p = p + 480 - PIC_HEIGHT;
	}
	
	dma_cache_wback((unsigned int)(lcd_frame0), 480 * 272);
        if(l009_backlight == 0)
          __lcd_set_backlight_level(50);
        mdelay(500);
        if(l009_backlight == 0)
          __lcd_set_backlight_level(0);

}
EXPORT_SYMBOL(draw_lock_picture);

static int proc_lcd_backlight_read_proc(
			char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	return sprintf(page, "%lu\n", l009_backlight);
}

static int proc_lcd_backlight_write_proc(
			struct file *file, const char *buffer,
			unsigned long count, void *data)
{
        l009_backlight =  simple_strtoul(buffer, 0, 10);
	__lcd_set_backlight_level(l009_backlight);	/* We support 8 levels here. */
        if(l009_backlight == 0)
          __gpio_clear_pin(GPIO_LCD_VCC_EN_N);
        else
          __gpio_set_pin(GPIO_LCD_VCC_EN_N);
	return count;
}

static int proc_lcd_flush_read_proc(
			char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	return sprintf(page, "%lu\n", lcd_flush_flag);
}



static int fb_resize_a320_original_thread(void *unused)
{
	printk("kernel frame buffer fb_resize_a320_original_thread start!\n");
	while(1)
	{
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ/20);
		fb_a320_thread(320,240,480,272,A320_ORIGINAL);
		if(resize_a320_go_out)
		break;
	}

}
static void fb_resize_a320_original_start()
{
	resize_a320_go_out= 0;
	resize_a320_original_task= kthread_run(fb_resize_a320_original_thread, NULL, "fb_a320_original");
	if(IS_ERR(resize_a320_original_task))
	{
		printk("Kernel fb_a320_originalstart error!\n");
		return;
	}
}
static int fb_resize_a320_fullscreen_thread(void *unused)
{
  printk("kernel frame buffer resize thread start!\n");
  while(1)
  {
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(HZ/20);
    fb_a320_thread(320,240,480,272,A320_FULLSCREEN);
    if(resize_a320_go_out)
      break;
  }
}
static void fb_resize_a320_fullscreen_start()
{
	resize_a320_go_out = 0;
	resize_a320_full_screen_task = kthread_run(fb_resize_a320_fullscreen_thread, NULL, "fb_a320_fullscreen");
	if(IS_ERR(resize_a320_full_screen_task))
	{
		printk("Kernel fb_a320_fullscreen start error!\n");
		return;
	}
}
static int proc_lcd_a320_read_proc(
			char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	return sprintf(page, "%lu\n", lcd_a320_flag);
}

static int proc_lcd_a320_write_proc(
			struct file *file, const char *buffer,
			unsigned long count, void *data)
{
  int last_lcd_a320_flag = lcd_a320_flag;

  lcd_a320_flag =  simple_strtoul(buffer, 0, 10);
  printk("%s %d lcd_a320_flag is %d\n",__FILE__,__LINE__,lcd_a320_flag);

  if(tvout_flag == 0)
  {
    if(lcd_a320_flag == 0)  	
    {
      resize_a320_go_out = 1;
      dma0_desc0->databuf = virt_to_phys((void *)lcd_frame0);
    }
    else if(lcd_a320_flag == 1)
    {
      resize_a320_go_out = 1;
      memset(lcd_frame0,0x00,480*272*2);
      memset(lcd_frame01,0x00,480*272*2);
      dma0_desc0->databuf = virt_to_phys((void *)lcd_frame01);
      fb_resize_a320_original_start();
    }
    else if(lcd_a320_flag == 2)
    {
      resize_a320_go_out = 1;
      memset(lcd_frame0,0x00,480*272*2);
      memset(lcd_frame01,0x00,480*272*2);
      dma0_desc0->databuf = virt_to_phys((void *)lcd_frame01);
      fb_resize_a320_fullscreen_start();
    }
  }
  else
  {
    lcd_a320_flag = last_lcd_a320_flag;
  }
  return count;
}
static int proc_lcd_flush_write_proc(
			struct file *file, const char *buffer,
			unsigned long count, void *data)
{
  //printk("%s %d  tvout_flah is %d\n",__FILE__,__LINE__,tvout_flag);
  if(tvout_flag == 0)
  {
    lcd_flush_flag =  simple_strtoul(buffer, 0, 10);
    //printk("%s %d  lcd_flush_flag is %d\n",__FILE__,__LINE__,lcd_flush_flag);
#if 0
    if(lcd_flush_flag == 1)  //start flush	
    {
      tvout_640_480 = 0;
    }	
    else  //stop flush
    {
      tvout_640_480 = 1;
    }
#endif
    //memcpy(lcd_frame01,lcd_frame0,320*240*4);
    if(lcd_flush_flag == 1)  //start flush	
    { 
      dma0_desc0->databuf = virt_to_phys((void *)lcd_frame0);
    }
    else
    {
      lprint_dbg("\n");
      memcpy(lcd_frame01,lcd_frame0,480*272*2);
      lprint_dbg("\n");
      //mdelay(500);
      lprint_dbg("\n");
      dma_cache_wback((unsigned int)(lcd_frame01), 480 * 272 * 2);
      dma0_desc0->databuf = virt_to_phys((void *)lcd_frame01);
    }
    //jz4750fb_deep_set_mode(jz4750_lcd_info);
  }
  return count;
}

static int proc_tvout_read_proc(
			char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	return sprintf(page, "%lu\n", tvout_flag);
}

static int proc_tvout_write_proc(
			struct file *file, const char *buffer,
			unsigned long count, void *data)
{
	unsigned long old_flag;
	old_flag = tvout_flag;
	
	tvout_flag =  simple_strtoul(buffer, 0, 10);
	
	printk("buffer XXXXXXXXXXXXXXXXXXXX---%s---\n",buffer);
	printk("************* tvout_flag = %d *************\n",tvout_flag);
	printk("************* old   flag = %d *************\n",old_flag);
	if(old_flag==0  && tvout_flag==1)	//lcd to pal
	{
		jz4750lcd_info_switch_to_TVE(PANEL_MODE_TVE_NTSC);
		jz4750tve_init(PANEL_MODE_TVE_NTSC); /* tve controller init */
		udelay(100);
		jz4750tve_enable_tve();
		/* turn off lcd backlight */
		__lcd_display_off();
                tvout_640_480 = 0;
		jz4750fb_deep_set_mode(jz4750_lcd_info);
		//display_h_color_bar(720, 540, 16);
		fb_resize_start();
	}
	else if(old_flag==1 && tvout_flag==0)  //tvout to lcd
	{
			jz4750tve_disable_tve();
			udelay(100);
			jz4750_lcd_info = &jz4750_lcd_panel;
			/* turn off lcd backlight */
			jz4750fb_deep_set_mode(jz4750_lcd_info);
			//display_h_color_bar(720, 540, 16);
			resize_go_out = 1;
			__lcd_slcd_special_on();
			__lcd_display_on();
	}
	else if(old_flag==1 && tvout_flag==2)  //tvout_2x to tvout_640x480
	{
			resize_go_out = 1;
			tvout_640_480 = 1;
			jz4750fb_deep_set_mode(jz4750_lcd_info);
	}	
	else if(old_flag==2 && tvout_flag==1)  //tvout_640x480 to tvout_2x
	{
			fb_resize_start();
			tvout_640_480 = 0;
			jz4750fb_deep_set_mode(jz4750_lcd_info);
	}
	else
	{}	

	return count;
}
static struct task_struct * logo_task;
#define BOOT_LOGO_PIC_NUM 1
static void logo_display(unsigned long data)
{
  //printk("%s %d \n",__FILE__,__LINE__);
  printk("kernel logo display  thread start!\n");
  char *p_l009 = 0x83000000;
  int i = 0;
  for(i = 0; i < BOOT_LOGO_PIC_NUM; i++)
  {
    memcpy((unsigned char *)lcd_frame0,(unsigned char *)p_l009, 480 * 272 * 2);
    p_l009 += 480*272*2;
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(HZ/10);
  }
  
}

static start_logo_task(void)
{
  logo_task= kthread_run(logo_display, NULL, "logo task");
  if(IS_ERR(logo_task))
  {
    printk("Kernel battery _monitor thread start error!\n");
    return;
  }
}
void jz4750_l009_draw_poweroff(void)
{
  return;
#if 0
  //printk("%s %d\n",__FILE__,__LINE__);
  memcpy((unsigned char *)lcd_frame0,(unsigned char *)l009_bootpic, 400 * 240 * 2);
  __lcd_set_backlight_level(100);
  REG_SLCD_CTRL &= ~(0x04);
  __slcd_enable_dma();   //maddrone add
  msleep(1000);
#endif
  //printk("%s %d\n",__FILE__,__LINE__);
}
EXPORT_SYMBOL(jz4750_l009_draw_poweroff);

static draw_rgb_to_lcd()
{
	int i = 0, j =0;
	unsigned short *fb = (unsigned short *)lcd_frame0;
	for (i=0;i<90;i++)
		for(j=0;j<480;j++){
			*fb = 0xf800;
			fb++;
		}
#if 1
	for (i=0;i<90;i++)
		for(j=0;j<480;j++){
			*fb = 0x07e0;
			fb++;
		}
	for (i=0;i<92;i++)
		for(j=0;j<480;j++){
			*fb = 0x001f;
			fb++;
		}
#endif
        dma_cache_wback((unsigned int)(lcd_frame0), 480 * 272 * 2);
        //while(1);
}
#define BOOTPIC_NUM 1
static void logo_display_(void)
{
  printk("kernel logo display  thread start!\n");
  char *p_l009 = 0x83000000;
  int i = 0;
  for(i = 0; i < BOOTPIC_NUM; i++)
  {
    memcpy((unsigned char *)lcd_frame0,(unsigned char *)p_l009, 480 * 272 * 2);
    dma_cache_wback((unsigned int)(lcd_frame0), 480 * 272 * 2);
    mdelay(70);
  }
}

 
static int __init jz4750fb_init(void)
{
	struct lcd_cfb_info *cfb;
	int err = 0;
	struct proc_dir_entry *res, *res1, *res3, *res4;

#if 1
	/* gpio init __gpio_as_lcd */
	if (jz4750_lcd_info->panel.cfg & LCD_CFG_MODE_TFT_16BIT)
		__gpio_as_lcd_16bit();
	else if (jz4750_lcd_info->panel.cfg & LCD_CFG_MODE_TFT_24BIT)
		__gpio_as_lcd_24bit();
	else
        {
          printk("attention  __gpio_as_lcd_8bit\n\n\n\n");
          __gpio_as_lcd_8bit();

          //__gpio_as_lcd_18bit();
        }	
	/* In special mode, we only need init special pin, 
	 * as general lcd pin has init in uboot */
#if defined(CONFIG_SOC_JZ4750) || defined(CONFIG_SOC_JZ4750D)
	switch (jz4750_lcd_info->panel.cfg & LCD_CFG_MODE_MASK) {
	case LCD_CFG_MODE_SPECIAL_TFT_1:
	case LCD_CFG_MODE_SPECIAL_TFT_2:
	case LCD_CFG_MODE_SPECIAL_TFT_3:
          {
            printk("attention __gpio_as_lcd_special \n\n\n");
            __gpio_as_lcd_special();
            break;
          }
	default:
		;
	}
#endif
#endif
	//maddrone
	//__gpio_as_slcd_8bit();

	if ( jz4750_lcd_info->osd.fg0.bpp > 16 && 
	     jz4750_lcd_info->osd.fg0.bpp < 32 ) {
		jz4750_lcd_info->osd.fg0.bpp = 32;
	}

	switch ( jz4750_lcd_info->osd.fg1.bpp ) {
	case 15:
	case 16:
		break;
	case 17 ... 32:
		jz4750_lcd_info->osd.fg1.bpp = 32;
		break;
	default:
		printk("jz4750fb fg1 not support bpp(%d), force to 32bpp\n", 
		       jz4750_lcd_info->osd.fg1.bpp);
		jz4750_lcd_info->osd.fg1.bpp = 32;
	}
	__lcd_clr_dis();
	__lcd_clr_ena();

	/* Configure SLCD module for setting smart lcd control registers */
#if defined(CONFIG_FB_JZ4750_SLCD)
		__lcd_display_on();
		__lcd_as_smart_lcd();
		__slcd_disable_dma();
		//__init_slcd_bus();	/* Note: modify this depend on you lcd */
#endif
	/* init clk */
	jz4750fb_change_clock(jz4750_lcd_info);
	__lcd_display_pin_init();
	//__lcd_slcd_special_on();
	
	cfb = jz4750fb_alloc_fb_info();
	if (!cfb)
		goto failed;

	err = jz4750fb_map_smem(cfb);
	if (err)
		goto failed;
	char *p_l009 = 0x83000000;
	//maddrone add boot pic here
        //memcpy((unsigned char *)lcd_frame0,(unsigned char *)l009_bootpic, 320 * 240 * 2);
        //memcpy((unsigned char *)lcd_frame0,(unsigned char *)p_l009, 400 * 240 * 2);


	jz4750fb_deep_set_mode( jz4750_lcd_info );

	//move here
	//__lcd_slcd_special_on();
	

	err = register_framebuffer(&cfb->fb);
	if (err < 0) {
		dprintk("jzfb_init(): register framebuffer err.\n");
		goto failed;
	}
	printk("fb%d: %s frame buffer device, using %dK of video memory\n",
	       cfb->fb.node, cfb->fb.fix.id, cfb->fb.fix.smem_len>>10);

	if (request_irq(IRQ_LCD, jz4750fb_interrupt_handler, IRQF_DISABLED,
			"lcd", 0)) {
		err = -EBUSY;
		goto failed;
	}

#ifdef CONFIG_PM
	/*
	 * Note that the console registers this as well, but we want to
	 * power down the display prior to sleeping.
	 */
	cfb->pm = pm_register(PM_SYS_DEV, PM_SYS_VGA, jzlcd_pm_callback);
	if (cfb->pm)
		cfb->pm->data = cfb;
#endif

	__lcd_set_ena();	/* enalbe LCD Controller */
	//__lcd_display_on();
#ifdef DEBUG
	display_h_color_bar(jz4750_lcd_info->osd.fg0.w, jz4750_lcd_info->osd.fg0.h, jz4750_lcd_info->osd.fg0.bpp);
#endif
	//display_h_color_bar(jz4750_lcd_info->osd.fg0.w, jz4750_lcd_info->osd.fg0.h, jz4750_lcd_info->osd.fg0.bpp);
        mdelay(50);
        start_logo_task();
	//draw_rgb_to_lcd();
        //logo_display_();
	__lcd_display_on();
	//mdelay(2000);
	//alterac add
#if 0
	__gpio_as_func0(32*3+26);
	__gpio_as_input(32*3+26);

	__gpio_as_func0(32*3);
	__gpio_as_output(32*3);
	__gpio_clear_pin(32*3);
	__gpio_as_func0(32*3+1);
	__gpio_as_output(32*3+1);
	__gpio_clear_pin(32*3+1);
	__gpio_as_func0(32*3+2);
	__gpio_as_output(32*3+2);
	__gpio_clear_pin(32*3+2);
	__gpio_as_func0(32*3+3);
	__gpio_as_output(32*3+3);
	__gpio_clear_pin(32*3+3);
	__gpio_as_func0(32*4+1);
	__gpio_as_output(32*4+1);
	__gpio_set_pin(32*4+1);
#endif
	//end
	//__lcd_backlight_on();
        //maddrone add
        res = create_proc_entry("jz/lcd_backlight", 0, NULL);
        if(res)
	{
		res->owner = THIS_MODULE;
		res->read_proc = proc_lcd_backlight_read_proc;
		res->write_proc = proc_lcd_backlight_write_proc;	
	}	

	res1 = create_proc_entry("jz/tvout", 0, NULL);
	if(res1)
	{
		res1->owner = THIS_MODULE;
		res1->read_proc = proc_tvout_read_proc;
		res1->write_proc = proc_tvout_write_proc;	
	}


	res3 = create_proc_entry("jz/lcd_flush", 0, NULL);
	if(res3)
	{
		res3->owner = THIS_MODULE;
		res3->read_proc = proc_lcd_flush_read_proc;
		res3->write_proc = proc_lcd_flush_write_proc;	
        }
        res4 = create_proc_entry("jz/lcd_a320", 0, NULL);
        if(res4)
        {
        	res4->owner = THIS_MODULE;
		res4->read_proc = proc_lcd_a320_read_proc;
		res4->write_proc = proc_lcd_a320_write_proc;	
        }
#if 0
	while(1)
	{
		msleep(500);
		print_lcdc_registers();
	}
#endif

	//maddrone add ipu driver init
	//ipu_dirver_register_irq(ipu_priv);

	return 0;

failed:
	print_dbg();
	jz4750fb_unmap_smem(cfb);
	jz4750fb_free_fb_info(cfb);

	return err;
}

#if 0
static int jzfb_remove(struct device *dev)
{
	struct lcd_cfb_info *cfb = dev_get_drvdata(dev);
	jzfb_unmap_smem(cfb);
	jzfb_free_fb_info(cfb);
	return 0;
}
#endif

#if 0
static struct device_driver jzfb_driver = {
	.name		= "jz-lcd",
	.bus 		= &platform_bus_type,
	.probe		= jzfb_probe,
        .remove		= jzfb_remove,
	.suspend	= jzfb_suspend,
        .resume		= jzfb_resume,
};
#endif

static void __exit jz4750fb_cleanup(void)
{
	//driver_unregister(&jzfb_driver);
	//jzfb_remove();
}

module_init(jz4750fb_init);
module_exit(jz4750fb_cleanup);
