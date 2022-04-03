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
#include <logging/log_core.h>
#include <display/cfb.h>

#include "uievq.h"
#include "buttons.h"

#define SHELL_PROMPT_CFB "$"
#define SHELL_CFB_LOG_MESSAGE_QUEUE_SIZE 10
#define SHELL_CFB_LOG_MESSAGE_QUEUE_TIMEOUT 1000
SHELL_CFB_DEFINE(shell_transport_cfb);
SHELL_DEFINE(shell_cfb, SHELL_PROMPT_CFB, &shell_transport_cfb,
	     SHELL_CFB_LOG_MESSAGE_QUEUE_SIZE,
	     SHELL_CFB_LOG_MESSAGE_QUEUE_TIMEOUT,
	     SHELL_FLAG_OLF_CRLF);
LOG_MODULE_REGISTER(shell_cfb, LOG_LEVEL_DBG);

struct shell_cfb *sh_cfb;

RING_BUF_DECLARE( rx_ringbuf, 64); // size...64


#define ALT_CMD 20
#define CMD_LEN 32
#define WRITE_BUF_LEN 256

static char cmd_alt[ALT_CMD][CMD_LEN];
static char write_buf[WRITE_BUF_LEN];
static int write_buf_pos = 0;
static int cmd_pos = 0;
static int cmd_nr  = 0;

static enum {
  Character,
  Escape,
  EscapeBracket,
  EscapeNumArg,
  Separator
} parseState = Character;

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

void resetParsing()
{
  write_buf_pos=0;
  cmd_nr=0;
  cmd_pos=0;
  parseState=Character;
}

#define SHELL_ALT_EVENT 2

static enum {
  Idle,
  ChooseArg
} shellMode=Idle;

static int choosen_cmd=0;

void shell_cfb_event_loop( const struct device *cfb) {
  while (1) {
    uint32_t ev = ui_evq_get( K_FOREVER);
    int evtype = FIELD_GET( EVENT_TYPE_MASK, ev);
    LOG_INF("evtype=%d",evtype);
    if( evtype == BTN_EVENT) {
      int btn = FIELD_GET( BTN_NR_MASK, ev);
      int state = FIELD_GET( BTN_STATE_BIT, ev);
      if( shellMode==ChooseArg) {
	if (btn == 1 && state == 1) {
	  ++choosen_cmd;
	  if( choosen_cmd == cmd_nr) choosen_cmd=0;
	  cfb_framebuffer_clear(cfb, false);
	  cfb_print( cfb, cmd_alt[choosen_cmd] , 0, 0); 
	  cfb_framebuffer_finalize( cfb);      
	}
      } else {
	char c='x';
	if (btn == 1 && state == 1) c='\t';
	else if(btn == 2 && state == 1) c='\r';
	if( c !='x') {
	  uint8_t *data;
	  ring_buf_put_claim( &rx_ringbuf, &data, rx_ringbuf.size);
	  char s[2]; s[0]=c; s[1]=0;
	  bytecpy( data, s, 2);
	  ring_buf_put_finish( &rx_ringbuf, 1);
	  resetParsing();
	  sh_cfb->shell_handler( SHELL_TRANSPORT_EVT_RX_RDY,
				 sh_cfb->shell_context);      
	}
      }
    } else if (evtype == SHELL_ALT_EVENT) {
      shellMode=ChooseArg;
      choosen_cmd=0;
      cfb_framebuffer_clear(cfb, false);
      cfb_print( cfb, cmd_alt[choosen_cmd] , 0, 0); 
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
  int i = 0;
  for(; i < length && write_buf_pos < WRITE_BUF_LEN; i++, write_buf_pos++) {
    write_buf[write_buf_pos] = data_[i];
  }
  LOG_HEXDUMP_INF( (const char *) write_buf, write_buf_pos,"data:");
  *cnt = i;

  int j = 0;
  for(; j < length; ++j) {
    char c = data_[j];
    if( c == 0x1b) { parseState=Escape; continue; }
    if( parseState == Escape && c == '[') {
      parseState=EscapeBracket; continue;
    }
    if( parseState == EscapeBracket ) {
      parseState=Character;
      if( c == 'm' ) continue; // MODESOFF 
      if( c >= '0' && c <= '9') {
	parseState=EscapeNumArg; continue;
      }
      continue;
    }
    if( parseState==EscapeNumArg ) {
      if( c >= '0' && c <= '9')	continue;
      parseState=Character;
      if( c == 'C' || c == 'D' ) { // DIRECTION
	parseState=Separator; continue;
      }
      continue;
    }
    if( c == '\r' || c == '\n' || c == ' ') {
      parseState = Separator;
      continue;
    }
    if( parseState==Separator) {
      parseState=Character;
      if( cmd_pos > 0 ) {
	cmd_alt[cmd_nr][cmd_pos]='\0';
	cmd_pos = 0;
	if( cmd_nr < ALT_CMD-1) ++cmd_nr;
      }
    }
    if( cmd_pos == 0 && c == SHELL_PROMPT_CFB[0]) {
      if(cmd_nr>0 || cmd_pos>0)
	ui_evq_put( FIELD_PREP( EVENT_TYPE_MASK, SHELL_ALT_EVENT));
    } else {
      cmd_alt[cmd_nr][cmd_pos]=c;
      if(cmd_pos < CMD_LEN-1) ++cmd_pos;
    }
  }
  *cnt = j;
  
  for( int i = 0; i <= cmd_nr; ++i) {
    LOG_INF( "cmd_alt[%d]='%s'",i,cmd_alt[i]);
  }
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

