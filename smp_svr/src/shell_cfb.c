/*
 * Shell backend used for testing
 *
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shell_cfb.h"
#include "shell_parse.h"
#include "strbuf.h" 
#include <zephyr/sys/util.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/display/cfb.h>

#include "buttons.h"

#define SHELL_CFB_LOG_MESSAGE_QUEUE_SIZE 100
#define SHELL_CFB_LOG_MESSAGE_QUEUE_TIMEOUT 1000
//#define CMD_ALT_SIZE 256
//#define CMD_CHS_SIZE 256
#define SHELL_PROMPT_CFB "cfb$"

SHELL_CFB_DEFINE(shell_transport_cfb, SHELL_PROMPT_CFB, 128, 128, 128);
SHELL_DEFINE(shell_cfb, SHELL_PROMPT_CFB, &shell_transport_cfb,
	     SHELL_CFB_LOG_MESSAGE_QUEUE_SIZE,
	     SHELL_CFB_LOG_MESSAGE_QUEUE_TIMEOUT,
	     SHELL_FLAG_OLF_CRLF);
LOG_MODULE_REGISTER(shell_cfb, LOG_LEVEL_DBG);

struct shell_cfb *sh_cfb;

RING_BUF_DECLARE( rx_ringbuf, 64); // size...64

#define SHELL_WRITE_EVENT BIT(31)

static int init(const struct shell_transport *transport,
		const void *config,
		shell_transport_handler_t evt_handler,
		void *context)
{
  sh_cfb = (struct shell_cfb *) transport->ctx;  
  sh_cfb->shell_handler = evt_handler;
  sh_cfb->shell_context = context;
  ring_buf_reset( &rx_ringbuf);
  return 0;
}

#define BTN1P 0x8
#define BTN2P 0x20
#define BTN3P 0x80

void shell_cfb_event_loop( const struct device *cfb) {
  /* LOG_INF( "event_loop: vt=%d,ct=%d,alt='%s',alt.size=%d,cmd='%s',cmd.size=%d", */
  /* 	   sh_cfb->vt, sh_cfb->ct, */
  /* 	   sh_cfb->alt->buffer, sh_cfb->alt->size, */
  /* 	   sh_cfb->cmd->buffer, sh_cfb->cmd->size); */
  
  size_t alt_pos = 0;
  while (1) {
    //LOG_INF( "alt_pos:%d,alt_size:%d",alt_pos,sh_cfb->alt->size);
    bool refresh_display = false; 
    uint32_t ev_ = k_event_wait( button_events(), 0xffffffff, true, K_FOREVER);
    // btn1 tab pressed
    if ( ev_ & BTN1P ) {
      LOG_INF( "BTN1P tab button");
      // cycle through alternatives
      size_t alt_pos_ = alt_pos; // store where we started
      while( alt_pos < sh_cfb->p->alt->size && sh_cfb->p->alt->buffer[ alt_pos] != ' ')
	++alt_pos;
      if( alt_pos < sh_cfb->p->alt->size) {
	++alt_pos; // advance to next entry
	refresh_display = true;
      }
      else if( alt_pos == sh_cfb->p->alt->size) { // we are on the end
	if( alt_pos_ == 0) {
	  // we had only one alternative or no at all
	  // so we sent the tab key to the shell
	  uint8_t *data;
	  ring_buf_put_claim( &rx_ringbuf, &data, rx_ringbuf.size);
	  strcpy( data, "\t");
	  ring_buf_put_finish( &rx_ringbuf, strlen( data));
	  sh_cfb->shell_handler( SHELL_TRANSPORT_EVT_RX_RDY,
				 sh_cfb->shell_context);    
	} else {
	  alt_pos = 0; // wrap around
	  refresh_display = true;
	}
      }
    }
    if( ev_ & BTN2P ) {
      LOG_INF( "BTN2P");
      // choose current alternative
      uint8_t *data;
      ring_buf_put_claim( &rx_ringbuf, &data, rx_ringbuf.size);
      strcpy( data, sh_cfb->p->alt->buffer + alt_pos);
      strcat( data, " \t");
      ring_buf_put_finish( &rx_ringbuf, strlen( data));
      // we already have choosed the root 
      sh_cfb->shell_handler( SHELL_TRANSPORT_EVT_RX_RDY,
			     sh_cfb->shell_context);    
    }
    if( ev_ & BTN3P ) {
      LOG_INF( "BTN3P");
      // execute command
      uint8_t *data;
      ring_buf_put_claim( &rx_ringbuf, &data, rx_ringbuf.size);
      strcpy( data, "\r");
      ring_buf_put_finish( &rx_ringbuf, strlen( data));
      // we would like to receive the result
      sh_cfb->shell_handler( SHELL_TRANSPORT_EVT_RX_RDY,
			     sh_cfb->shell_context);      
    }
    if ( (ev_ & SHELL_WRITE_EVENT) || refresh_display ) {
      LOG_INF( "Display" );
      char buf[128];
      size_t i = 0, j = 0;
      while( j < sh_cfb->p->cmd->size && i < 128) 
	buf[i++] = sh_cfb->p->cmd->buffer[ j++];
      j = alt_pos;
      while( j < sh_cfb->p->alt->size && i < 128) 
	buf[i++] = sh_cfb->p->alt->buffer[ j++];
      if( i < 128) buf[i]='\0'; else buf[127]='\0';
      cfb_framebuffer_clear(cfb, false);
      cfb_print( cfb, buf, 0, 0); 
      cfb_framebuffer_finalize( cfb);      
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
  if ( sh_cfb == NULL) {
    *cnt = 0;
    return -ENODEV;
  }
  const char* data_ = (const char *) data;
  //LOG_HEXDUMP_INF( data_, length,"write data:");
  *cnt = shell_parse(  data_, length, sh_cfb->p);
  /* LOG_INF( "ct=%d,cmd='%s',alt='%s',out='%s'", */
  /* 	   sh_cfb->p->ct, */
  /* 	   sh_cfb->p->cmd->buffer, */
  /* 	   sh_cfb->p->alt->buffer, */
  /* 	   sh_cfb->p->out->buffer); */
  
  k_event_post( button_events(), SHELL_WRITE_EVENT);  
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

int enable_shell_cfb(const struct device *arg)
{
  ARG_UNUSED(arg);
  static const struct shell_backend_config_flags cfg_flags =
    {						
      .insert_mode	= 0,
      .echo		= 1,
      .obscure	        = 0,				
      .mode_delete	= 0,			
      .use_colors	= 0,			
      .use_vt100	= 1,			
    };
  shell_init(&shell_cfb, NULL, cfg_flags, false, LOG_LEVEL_ERR);
  return 0;
}

//SYS_INIT(enable_shell_cfb, POST_KERNEL, 0);

const struct shell *shell_backend_cfb_get_ptr(void)
{
  return &shell_cfb;
}

