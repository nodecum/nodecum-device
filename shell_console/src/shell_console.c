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

#define SHELL_WRITE_EVENT 1 
//BIT(1)
#define ALT_POS_CHANGED 2
//(2)
#define TERMINATE_OUTPUT_LOOP 4

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
// *new_data is set to lowest position where data is different
// from the value before
void copy_alt( struct shell_parse_t *p, uint8_t *data, size_t *i, uint32_t max_len, 
                uint32_t *new_data) 
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
      // printk("MATCH i_=%d, j_=%d, match=%d\n", i_, j_, match);
      j += p->alt_eq_part;
    }
  }
  if( p->cmd->size == 0 || p->cmd->buffer[ p->cmd->size - 1] == ' ' || match) {
    while( j < p->alt->size && *i < max_len && 
           p->alt->buffer[ j] != ' ' ) {
      const char c = p->alt->buffer[j];      
      if( data[ *i] != c) {
        data[ *i] = c;
        if( *i < *new_data) *new_data = *i;
      } 
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
      k_mutex_lock( p->mutex, K_FOREVER);
      // printk( "BTN1P tab\n");
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
      k_mutex_unlock( p->mutex);
    }
    if( c == BTN2P ) {
      // printk( "BTN2P\n");
      // choose current alternative
      uint8_t *data;
      uint32_t max_len = ring_buf_put_claim( &rx_ringbuf, &data, rx_ringbuf.size);
      size_t i = 0; 
      uint32_t nd = max_len; // dummy, not used here
      k_mutex_lock( p->mutex, K_FOREVER);
      copy_alt( p, data, &i, max_len, &nd);
      if( i < max_len) data[i++] = ' ';
      if( i < max_len) data[i++] = '\t';
      ring_buf_put_finish( &rx_ringbuf, i);
      p->alt->size = 0;
      k_mutex_unlock( p->mutex);
      // we already have choosed the root 
      sc->shell_handler( SHELL_TRANSPORT_EVT_RX_RDY,
			     sc->shell_context);    
    }
    if( c == BTN3P ) {
      // printk( "BTN3P\n");
      // execute command
      uint8_t *data;
      uint32_t max_len = ring_buf_put_claim( &rx_ringbuf, &data, rx_ringbuf.size);
      size_t i = 0;
      uint32_t nd = max_len; // dummy, not used here
      copy_alt( p, data, &i, max_len, &nd);
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
      k_event_post( &event, ALT_POS_CHANGED);
    }
  }
  printk("leave input loop\n");
}

void shell_console_output_loop( void*, void*, void*) {
  printk("output loop is active now\n");
  struct shell_parse_t *p = sc->p;
  struct str_buf *cmd_buf = sc->cmd_buf;
  struct str_buf *out_buf = sc->out_buf;
  uint32_t ev_ = 0;
  enum ct_state ct_ = Prompt;
  enum ct_state ct = Prompt;
  while ( ! (ev_ & TERMINATE_OUTPUT_LOOP)) {
    //LOG_INF( "alt_pos:%d,alt_size:%d",alt_pos,sh_cfb->alt->size);
    ev_ = k_event_wait( &event, 0xffffffff, true, K_FOREVER);
    //printk( "\nev:%d ", ev_); 
    if ( (ev_ & SHELL_WRITE_EVENT) || (ev_ & ALT_POS_CHANGED) )  {
      const size_t cmd_old_size = cmd_buf->size;
      uint32_t cmd_nd = cmd_buf->max_size;
      uint32_t out_nd = out_buf->max_size;
      k_mutex_lock( p->mutex, K_FOREVER);
      ct_ = ct;  // save the context before the new one 
      ct = p->ct;
      /*
      printk( " ct:");
      switch (ct)
      {
        case Prompt: printk("Prompt"); break;
        case PromptMatched: printk("PromptMatched"); break;
        case Cmd: printk("Cmd"); break;
        case AltOrOut: printk("AltOrOut"); break;
        case Alt: printk("Alt"); break;
        case Out: printk("Out"); break;
      } */
      const bool newPrompt = 
        ( ct == PromptMatched 
          && ! (cmd_buf->size > 0 && cmd_buf->buffer[ cmd_buf->size-1] == ' ') );
      if( ( (ev_ & SHELL_WRITE_EVENT) 
            && (ct == Cmd || newPrompt ) ) 
          || (ev_ & ALT_POS_CHANGED) ) {
        size_t i = 0;  
        while( i < p->cmd->size && i < cmd_buf->max_size) { 
          const char c = p->cmd->buffer[ i];
          if( i < cmd_buf->size) {
            if( cmd_buf->buffer[ i] != c ) {
              cmd_buf->buffer[ i] = c;
              if( i < cmd_nd) cmd_nd = i;
            }
          } else {
            cmd_buf->buffer[i] = c;
            cmd_nd = i;
          } 
          ++i; 
        }
        copy_alt( p, cmd_buf->buffer, &i, cmd_buf->max_size, &cmd_nd);
        if( cmd_nd != cmd_buf->max_size ) {  
          if( i < cmd_buf->max_size) cmd_buf->buffer[ i] = '\0'; 
          else cmd_buf->buffer[ i-1] = '\0'; 
          cmd_buf->size = i;
        }
      } 
      else if( ct == Out) {
        if( ct_ != Out) 
        { 
          // if we had an output we clear the cmd_buf
          // and the out_buf
          cmd_buf->size=0; cmd_buf->buffer[0]= '\0'; 
          cmd_nd = cmd_buf->max_size; 
          out_buf->size=0; out_buf->buffer[0]= '\0';
          out_nd = out_buf->max_size;
        }
        size_t i = 0;
        while( i < p->out->size && i < out_buf->max_size) { 
          const char c = p->out->buffer[ i];
          if( out_buf->buffer[ i] != c ) {
              out_buf->buffer[ i] = c;
              if( i < out_nd) out_nd = i;
            }
          ++i; 
        }
        if( i < out_buf->max_size) out_buf->buffer[ i] = '\0'; 
        else out_buf->buffer[ i-1] = '\0'; 
        out_buf->size = i;
      }
      k_mutex_unlock( p->mutex);
      if( (ev_ & SHELL_WRITE_EVENT ) && newPrompt) {
        printk("\r$");
      }
      if( ((ev_ & SHELL_WRITE_EVENT)
          && (ct == Cmd || ct == PromptMatched) 
          && cmd_nd < cmd_buf->size)
          || (ev_ & ALT_POS_CHANGED) ) {
        // we delete 
        int steps_back = cmd_old_size - cmd_nd;
        int j = 0;
        while( j < steps_back) { printk( "\b"); ++j; }
        //printk("\n");
        printk("%s", cmd_buf->buffer + cmd_nd );
        //printk( " cmd:'%s' cmd_buf:'%s' s:%d os:%d nd:%d sb:%d ev:%d", 
        //p->cmd->buffer, cmd_buf->buffer, cmd_buf->size, cmd_old_size, cmd_nd, 
        //steps_back, ev_ );
        
        //printk("\r%s", cmd_buf->buffer);
        
        j = cmd_buf->size;
        while( j < cmd_old_size) { printk(" "); ++j; }
        j = cmd_buf->size;
        while( j < cmd_old_size) { printk("\b"); ++j; }
        
      }
      else if ( !newPrompt && ct == Out && out_nd < out_buf->size) {
        if( ct_ != Out) printk( "\n");
        printk( "%s", out_buf->buffer + out_nd);
      }
      //}
      //if( p->out->size > 0)
      //  printk( "\n%s\n", p->out->buffer );
      //printk( " eq:%d", p->alt_eq_part);
      //printk( " cmd:'%s' alt:'%s' out:'%s'\n", p->cmd->buffer, p->alt->buffer, p->out->buffer  );     
      //printk( "buf:'%s'\n", buf );
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

