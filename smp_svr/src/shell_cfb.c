/*
 * Shell backend used for testing
 *
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shell_cfb.h"
#include <init.h>
#include <logging/log.h>
#include "uievq.h"
#include "buttons.h"

#define LOG_MODULE_NAME shell_cfb
LOG_MODULE_REGISTER(shell_cfb);
#define SHELL_PROMPT "$"

SHELL_CFB_DEFINE(shell_transport_cfb);
SHELL_DEFINE(shell_cfb, SHELL_PROMPT, &shell_transport_cfb, 1,
	     0, SHELL_FLAG_OLF_CRLF);

struct shell_cfb *sh_cfb;

RING_BUF_DECLARE( rx_ringbuf, 64); // size...64

static int init(const struct shell_transport *transport,
		const void *config,
		shell_transport_handler_t evt_handler,
		void *context)
{
  sh_cfb = (struct shell_cfb *) transport->ctx;
    
  memset(sh_cfb, 0, sizeof(struct shell_cfb));
  
  sh_cfb->shell_handler = evt_handler;
  sh_cfb->shell_context = context;
  
  ring_buf_reset( &rx_ringbuf);
  return 0;
}

const char* help="help\r";

void shell_cfb_event_loop( void) {
  while (1) {
    uint32_t ev = ui_evq_get( K_FOREVER);
    int btn = FIELD_GET( BTN_NR_MASK, ev);
    int state = FIELD_GET( BTN_STATE_BIT, ev);
    if (btn == 1 && state == 1) {
      uint8_t *data;
      uint32_t len,rd_len;
      len=ring_buf_put_claim( &rx_ringbuf, &data, rx_ringbuf.size);
      rd_len=5;
      int err;
      rd_len = z_user_string_nlen( help, len, &err);
      bytecpy( data, help, rd_len);
      ring_buf_put_finish( &rx_ringbuf, rd_len);
      
      sh_cfb->shell_handler( SHELL_TRANSPORT_EVT_RX_RDY,
			     sh_cfb->shell_context);
    }
  }
}

static int uninit(const struct shell_transport *transport)
{
  if ( sh_cfb == NULL) {
    return -ENODEV;
  }

  return 0;
}

static int enable(const struct shell_transport *transport, bool blocking)
{
  if (sh_cfb == NULL) {
    return -ENODEV;
  }
  
  return 0;
}

static int write(const struct shell_transport *transport,
		 const void *data, size_t length, size_t *cnt)
{
  size_t store_cnt;
  
  if ( sh_cfb == NULL) {
    *cnt = 0;
    return -ENODEV;
  }
  store_cnt = length;
  if (sh_cfb->len + store_cnt >= sizeof(sh_cfb->buf)) {
    store_cnt = sizeof(sh_cfb->buf) - sh_cfb->len - 1;
  }
  memcpy(sh_cfb->buf + sh_cfb->len, data, store_cnt);
  sh_cfb->len += store_cnt;
  
  *cnt = length;
  
  sh_cfb->shell_handler( SHELL_TRANSPORT_EVT_TX_RDY,
			 sh_cfb->shell_context);
  
  return 0;
}

static int read(const struct shell_transport *transport,
		void *data, size_t length, size_t *cnt)
{
  if ( sh_cfb == NULL) {
    return -ENODEV;
  }

  *cnt = ring_buf_get( &rx_ringbuf, data, length);


  return 0;
}

const struct shell_transport_api shell_cfb_transport_api = {
  .init = init,
  .uninit = uninit,
  .enable = enable,
  .write = write,
  .read = read
};

static int enable_shell_cfb(const struct device *arg)
{
  ARG_UNUSED(arg);
  static const struct shell_backend_config_flags cfg_flags =
    {						
      .insert_mode	= 0,
      .echo		= 0,
      .obscure	        = 0,				
      .mode_delete	= 0,			
      .use_colors	= 0,			
      .use_vt100	= 0,			
    };
  shell_init(&shell_cfb, NULL, cfg_flags, true, LOG_LEVEL_INF);
  return 0;
}

SYS_INIT(enable_shell_cfb, POST_KERNEL, 0);

const struct shell *shell_backend_cfb_get_ptr(void)
{
  return &shell_cfb;
}

