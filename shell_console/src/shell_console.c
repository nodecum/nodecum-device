/*
 * Shell backend used for testing
 *
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shell_console.h"
#include "shell_parse.h"
#include "strbuf.h" 
#include <zephyr/kernel.h>
#include <zephyr/console/console.h>
#include <zephyr/sys/util.h>
#include <zephyr/init.h>
//#include <zephyr/logging/log.h>
//#include <zephyr/logging/log_core.h>
#include <zephyr/sys/printk.h>


#define SHELL_CONSOLE_LOG_MESSAGE_QUEUE_SIZE 100
#define SHELL_CONSOLE_LOG_MESSAGE_QUEUE_TIMEOUT 1000
#define SHELL_PROMPT_CONSOLE "c$"

SHELL_CONSOLE_DEFINE(shell_transport_console, SHELL_PROMPT_CONSOLE, 128, 128, 128);
SHELL_DEFINE(shell_console, SHELL_PROMPT_CONSOLE, &shell_transport_console,
	     SHELL_CONSOLE_LOG_MESSAGE_QUEUE_SIZE,
	     SHELL_CONSOLE_LOG_MESSAGE_QUEUE_TIMEOUT,
	     SHELL_FLAG_OLF_CRLF);
LOG_MODULE_REGISTER(shell_console, LOG_LEVEL_DBG);

struct shell_console *sc;

RING_BUF_DECLARE( rx_ringbuf, 64); // size...64

K_EVENT_DEFINE( event);

#define SHELL_WRITE_EVENT BIT(1)
#define TERMINATE_OUTPUT_LOOP BIT(2)

static int init(const struct shell_transport *transport,
		const void *config,
		shell_transport_handler_t evt_handler,
		void *context)
{
  sc = (struct shell_console *) transport->ctx;  
  sc->shell_handler = evt_handler;
  sc->shell_context = context;
  ring_buf_reset( &rx_ringbuf);
  return 0;
}

// copy current alternative to data with ommitting
// the letters in the last snippet of cmdline if present 
// begin to write at position *i in data 
// *i gets incremented as data will be writen
void copy_alt( struct shell_parse_t *p, uint8_t *data, size_t* i, uint32_t max_len) 
{   
  size_t j = p->alt_pos;
  // if cmd exists then copy only if last char is space 
  bool match = false;  
  if( p->cmd->size > 0 && p->alt_eq_part > 0 && p->alt_eq_part < p->alt->size ) {
    int i_ = p->alt_eq_part - 1;
    int j_ = p->cmd->size - 1;
    match = true;
    while( match && j_ >= 0 && p->cmd->buffer[ j_] != ' ') {
      match = i_ >= 0 && p->cmd->buffer[ j_] == p->alt->buffer[ i_];
      --i_; --j_;
    } 
    match = match && i_ < 0;
    if(match) {
      printk("MATCH i_=%d, j_=%d, match=%d\n", i_, j_, match);
      j += p->alt_eq_part;
    }
  }
  if( p->cmd->size == 0 || p->cmd->buffer[ p->cmd->size - 1] == ' ' || match) {
    while( j < p->alt->size && *i < max_len && 
           p->alt->buffer[ j] != ' ' ) {
      data[*i] = p->alt->buffer[j];
      (*i)++; j++;
    }
  }
}

#define BTN1P '1'
#define BTN2P '2'
#define BTN3P '3'
#define BTNQ  'q'

void shell_console_input_loop() {
  /* LOG_INF( "event_loop: vt=%d,ct=%d,alt='%s',alt.size=%d,cmd='%s',cmd.size=%d", */
  /* 	   sh_cfb->vt, sh_cfb->ct, */
  /* 	   sh_cfb->alt->buffer, sh_cfb->alt->size, */
  /* 	   sh_cfb->cmd->buffer, sh_cfb->cmd->size); */
  struct shell_parse_t *p = sc->p;
  char c = '\0';
  while ( c != BTNQ ) {
    //LOG_INF( "p->alt_pos:%d,alt_size:%d",p->alt_pos,sh_cfb->alt->size);
    bool refresh_display = false;
    c = console_getchar();
    // btn1 tab pressed
    if ( c == BTN1P ) {
      printk( "BTN1P tab\n");
      //LOG_INF( "BTN1P tab\n");
      // cycle through alternatives
      size_t alt_pos_ = p->alt_pos; // store where we started
      while( p->alt_pos < p->alt->size && p->alt->buffer[ p->alt_pos] != ' ')
	      ++p->alt_pos;
      if( p->alt_pos < p->alt->size) {
	      ++p->alt_pos; // advance to next entry
	      refresh_display = true;
      }
      else if( p->alt_pos == p->alt->size) { // we are on the end
	      if( alt_pos_ == 0) {
	        // we had only one alternative or no at all
	        // so we sent the tab key to the shell
	        uint8_t *data;
	        ring_buf_put_claim( &rx_ringbuf, &data, rx_ringbuf.size);
	        strcpy( data, "\t");
	        ring_buf_put_finish( &rx_ringbuf, strlen( data));
	        sc->shell_handler( SHELL_TRANSPORT_EVT_RX_RDY,
				  sc->shell_context);    
	      } else {
	        p->alt_pos = 0; // wrap around
	        refresh_display = true;
	      }
      }
    }
    if( c == BTN2P ) {
      printk( "BTN2P\n");
      // choose current alternative
      uint8_t *data;
      uint32_t max_len = ring_buf_put_claim( &rx_ringbuf, &data, rx_ringbuf.size);
      size_t i = 0;
      copy_alt( p, data, &i, max_len);
      if( i < max_len) data[i++] = ' ';
      if( i < max_len) data[i++] = '\t';
      ring_buf_put_finish( &rx_ringbuf, i);
      p->alt->size = 0;
      // we already have choosed the root 
      sc->shell_handler( SHELL_TRANSPORT_EVT_RX_RDY,
			     sc->shell_context);    
    }
    if( c == BTN3P ) {
      printk( "BTN3P\n");
      // execute command
      uint8_t *data;
      uint32_t max_len = ring_buf_put_claim( &rx_ringbuf, &data, rx_ringbuf.size);
      size_t i = 0;
      copy_alt( p, data, &i, max_len);
      if( i < max_len) data[i++] = '\r';
      ring_buf_put_finish( &rx_ringbuf, i);
      // we would like to receive the result
      sc->shell_handler( SHELL_TRANSPORT_EVT_RX_RDY,
			     sc->shell_context);      
    }
    if( c == BTNQ ) {
      printk( "send termination event\n");
      k_event_post( &event, TERMINATE_OUTPUT_LOOP);
      refresh_display = false;
    }
    if ( refresh_display ) {
      k_event_post( &event, SHELL_WRITE_EVENT);
    }
  }
  printk("leave input loop\n");
}

void shell_console_output_loop( void*, void*, void*) {
  printk("output loop is active now\n");
  struct shell_parse_t *p = sc->p;
  uint32_t ev_ = 0;
  while ( ! (ev_ & TERMINATE_OUTPUT_LOOP)) {
    //LOG_INF( "alt_pos:%d,alt_size:%d",alt_pos,sh_cfb->alt->size);
    ev_ = k_event_wait( &event, 0xffffffff, true, K_FOREVER);
    if ( ev_ & SHELL_WRITE_EVENT )  {
      // printk( "SHELL_WRITE_EVENT\n" );
      char buf[128];
      size_t i = 0;
      while( i < p->cmd->size && i < 128) {  
        buf[i] = p->cmd->buffer[ i];
        ++i; 
      }
      copy_alt( p, buf, &i, 128);  
      if( i < 128) buf[i]='\0'; else buf[127]='\0'; 
      printk( "ct:");
      switch (p->ct)
      {
      case Prompt: printk("Prompt"); break;
      case PromptMatched: printk("PromptMatched"); break;
      case Cmd: printk("Cmd"); break;
      case AltOrOut: printk("AltOrOut"); break;
      case Alt: printk("Alt"); break;
      case Out: printk("Out"); break;
      }
      printk( " eq:%d", p->alt_eq_part);
      printk( " cmd:'%s' alt:'%s' out:'%s'\n", p->cmd->buffer, p->alt->buffer, p->out->buffer  ); 
      printk( "buf:'%s'\n", buf );
    }
    else if ( ev_ & TERMINATE_OUTPUT_LOOP) {
      printk( "TERMINATE_OUTPUT_LOOP\n");
    }
  }
  printk( "leave output loop\n");
}

static int uninit(const struct shell_transport *transport)
{
  if ( sc == NULL) {
    return -ENODEV;
  }
  return 0;
}

static int enable(const struct shell_transport *transport, bool blocking)
{
  if (sc == NULL) {
    return -ENODEV;
  }
  
  return 0;
}

static int write(const struct shell_transport *transport,
		 const void *data, size_t length, size_t *cnt)
{
  if ( sc == NULL) {
    *cnt = 0;
    return -ENODEV;
  }
  const char* data_ = (const char *) data;
  //LOG_HEXDUMP_INF( data_, length,"write data:");
  *cnt = shell_parse(  data_, length, sc->p);
  /* printk( "ct=%d,cmd='%s'\n alt='%s'\n out='%s'", */
  /* 	   sc->p->ct, */
  /* 	   sc->p->cmd->buffer, */
  /* 	   sc->p->alt->buffer, */
  /* 	   sc->p->out->buffer); */
  
  k_event_post( &event, SHELL_WRITE_EVENT);  
  sc->shell_handler( SHELL_TRANSPORT_EVT_TX_RDY,
			 sc->shell_context);
  return 0;
}

static int read(const struct shell_transport *transport,
		void *data, size_t length, size_t *cnt)
{
  if ( sc == NULL) {
    return -ENODEV;
  }
  *cnt = ring_buf_get( &rx_ringbuf, data, length);
  return 0;
}

const struct shell_transport_api shell_console_transport_api = {
  .init = init,
  .uninit = uninit,
  .enable = enable,
  .write = write,
  .read = read
};

int enable_shell_console(const struct device *arg)
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
  shell_init(&shell_console, NULL, cfg_flags, false, LOG_LEVEL_ERR);
  return 0;
}

//SYS_INIT(enable_shell_console, POST_KERNEL, 0);

const struct shell *shell_backend_console_get_ptr(void)
{
  return &shell_console;
}

