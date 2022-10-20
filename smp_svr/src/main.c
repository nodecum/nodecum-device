/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Prevas A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/stats/stats.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/display/cfb.h>
#include "cfb_font_magic5.h"

#ifdef CONFIG_MCUMGR_CMD_FS_MGMT
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include "fs_mgmt/fs_mgmt.h"
#include <zephyr/fs/littlefs.h>
#endif
#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
#include "os_mgmt/os_mgmt.h"
#endif
#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
#include "img_mgmt/img_mgmt.h"
#endif
#ifdef CONFIG_MCUMGR_CMD_STAT_MGMT
#include "stat_mgmt/stat_mgmt.h"
#endif
#ifdef CONFIG_MCUMGR_CMD_SHELL_MGMT
#include "shell_mgmt/shell_mgmt.h"
#endif
#ifdef CONFIG_MCUMGR_CMD_FS_MGMT
#include "fs_mgmt/fs_mgmt.h"
#endif

#define LOG_LEVEL LOG_LEVEL_DBG
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(smp_sample, LOG_LEVEL_DBG);

#include "common.h"

#include "uievq.h"
#include "buttons.h"
#include "shell_cfb.h"

/* Define an example stats group; approximates seconds since boot. */
STATS_SECT_START(smp_svr_stats)
STATS_SECT_ENTRY(ticks)
STATS_SECT_END;

/* Assign a name to the `ticks` stat. */
STATS_NAME_START(smp_svr_stats)
STATS_NAME(smp_svr_stats, ticks)
STATS_NAME_END(smp_svr_stats);

/* Define an instance of the stats group. */
STATS_SECT_DECL(smp_svr_stats) smp_svr_stats;

/* #ifdef CONFIG_MCUMGR_CMD_FS_MGMT */
/* FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(cstorage); */
/* static struct fs_mount_t littlefs_mnt = { */
/* 	.type = FS_LITTLEFS, */
/* 	.fs_data = &cstorage, */
/* 	.storage_dev = (void *)FLASH_AREA_ID(storage), */
/* 	.mnt_point = "/lfs1" */
/* }; */
/* #endif */


void main(void)
{
  int rc = STATS_INIT_AND_REG(smp_svr_stats, STATS_SIZE_32,
			      "smp_svr_stats");
  
  if (rc < 0) {
    LOG_ERR("Error initializing stats system [%d]", rc);
  }

  /* Register the built-in mcumgr command handlers. */
/* #ifdef CONFIG_MCUMGR_CMD_FS_MGMT */
/*   rc = fs_mount(&littlefs_mnt); */
/*   if (rc < 0) { */
/*     LOG_ERR("Error mounting littlefs [%d]", rc); */
/*   } */
  
/*   fs_mgmt_register_group(); */
/* #endif */
#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
  os_mgmt_register_group();
#endif
#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
  img_mgmt_register_group();
#endif
#ifdef CONFIG_MCUMGR_CMD_STAT_MGMT
  stat_mgmt_register_group();
#endif
#ifdef CONFIG_MCUMGR_CMD_SHELL_MGMT
  shell_mgmt_register_group();
#endif
#ifdef CONFIG_MCUMGR_CMD_FS_MGMT
  fs_mgmt_register_group();
#endif
#ifdef CONFIG_MCUMGR_SMP_BT
  start_smp_bluetooth();
#endif
#ifdef CONFIG_MCUMGR_SMP_UDP
  start_smp_udp();
#endif

  if (IS_ENABLED(CONFIG_USB_DEVICE_STACK)) {
    rc = usb_enable(NULL);
    if (rc) {
      LOG_ERR("Failed to enable USB");
      return;
    }
  }

  ui_evq_init();
  init_buttons();

  const struct device *dev;

  dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
  if (!device_is_ready(dev)) {
    LOG_ERR("Device %s not ready\n", dev->name);
    return;
  }
  
  if (display_set_pixel_format(dev, PIXEL_FORMAT_MONO10) != 0) {
    LOG_ERR("Failed to set required pixel format\n");
    return;
  }
  LOG_INF("Initialized %s\n", dev->name);
  
  if (cfb_framebuffer_init(dev)) {
    LOG_ERR("Framebuffer initialization failed!\n");
    return;
  }
  cfb_framebuffer_set_font(dev, 0);	      
  //cfb_framebuffer_invert(dev);
  cfb_framebuffer_clear(dev, true);
  display_blanking_off(dev);
  cfb_framebuffer_finalize(dev);
  
  LOG_INF("build time: " __DATE__ " " __TIME__);
  
  /* The system work queue handles all incoming mcumgr requests.  Let the
   * main thread idle while the mcumgr server runs.
   */
  // char buf[32];
  enable_shell_cfb(NULL);
  while (1) {
    k_sleep(Z_TIMEOUT_MS(1000));
    shell_cfb_event_loop( dev);
    /* uint32_t ev = ui_evq_get( K_FOREVER); */
    /* int btn = FIELD_GET( BTN_NR_MASK, ev); */
    /* int state = FIELD_GET( BTN_STATE_BIT, ev); */
    
    /* if (btn == 4 && state == 0) { */
    /*   cfb_framebuffer_clear(dev, true); */
    /*   cfb_framebuffer_finalize(dev); */
    /*   gpio_pin_set(led0, PIN, 0); */
    /* } else { */
    /*   cfb_framebuffer_clear(dev, false); */
    /*   snprintf(buf, sizeof(buf), "%d, %d", btn, state); */
    /*   cfb_print(dev, buf, 0, 0); */
    /*   cfb_framebuffer_finalize(dev); */
    /*   gpio_pin_set(led0, PIN, 1);	       */
    /*  } */
  }
}
