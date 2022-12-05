#include "shell_browser.h"
#include <string.h>

enum endpoint {
  CONSOLE_EP,
  SHELL_EP
};

static void shell_browser_pend_on
( const struct shell_browser *sb, enum shell_browser_signal sig)
{
  struct k_poll_event event;
  k_poll_event_init
    ( &event, K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
      &sb->ctx->signals[ sig]);
  k_poll( &event, 1, K_FOREVER);
  k_poll_signal_reset( &sb->ctx->signals[ sig]);
}

void shell_browser_write_to
( const struct shell_browser *sb, enum endpoint ep, const char *data, size_t length)
{
  __ASSERT_NO_MSG(sb && data);

  size_t offset = 0;
  size_t tmp_cnt;

  while (length) {
    int err;
    switch( ep) {
    case CONSOLE_EP : err = sb->iface->api->write_to_console
	( sb->iface, &data[offset], length, &tmp_cnt);
      break;
    case SHELL_EP :  err = sb->iface->api->write_to_shell
	( sb->iface, &data[offset], length, &tmp_cnt);
      break;
    }
    __ASSERT_NO_MSG( err == 0);
    __ASSERT_NO_MSG( length >= tmp_cnt);
    offset += tmp_cnt;
    length -= tmp_cnt;
    if (tmp_cnt == 0) {
      shell_pend_on( sb, SHELL_BROWSER_SIGNAL_CONSOLE_TX_DONE + ep);
    }
  }
}

static void shell_browser_evt_handler( enum shell_browser_evt evt_type, void *ctx)
{
  struct shell_browser *sb = (struct shell_browser *) ctx;
  // caution, mapping of external event to internal signal enum
  // depend on ordering in enum defenitions
  k_poll_signal_raise
    ( &sb->ctx->signals
      [ // the first corresponding event enum
       SHELL_BROWSER_SIGNAL_CONSOLE_RX_RDY 
       + evt_type], 0);
}

typedef void (*shell_browser_signal_handler_t)(const struct shell_browser*);

static void shell_browser_signal_handle
( const struct shell_browser *sb,
  enum shell_browser_signal sig_idx,
  shell_browser_signal_handler_t handler)
{
  struct k_poll_signal *signal = &sb->ctx->signals[sig_idx];
  int set;
  int res;

  k_poll_signal_check(signal, &set, &res);
  
  if (set) {
    k_poll_signal_reset(signal);
    handler(sb);
  }
}

void shell_browser_handle_console( struct shell_browser* sb)
{
  size_t count = 0;
  char data;
  (void) sb->iface->api->read_from_console
    ( sb->iface, &data, sizeof( data), &count);
  if( count == 0) {
    return;
  }
  (void) sb->iface->api->write_to_shell
    ( sb->iface, &data, sizeof( data), &count);
}

void shell_browser_handle_shell( struct shell_browser* sb)
{
 size_t count = 0;
  char data;
  (void) sb->iface->api->read_from_shell
    ( sb->iface, &data, sizeof( data), &count);
  if( count == 0) {
    return;
  }
  (void) sb->iface->api->write_to_console
    ( sb->iface, &data, sizeof( data), &count);
} 


static void kill_handler( const struct shell_browser *sb)
{
  sb->ctx->tid = NULL;
  k_thread_abort( k_current_get());
}

void shell_browser_process(const struct shell_browser *sb,
			   shell_browser_signal_handler_t handler)
{
  __ASSERT_NO_MSG( sb);
  __ASSERT_NO_MSG( sb->ctx);

  switch (sb->ctx->state) { 
  case SHELL_BROWSER_STATE_UNINITIALIZED:
  case SHELL_BROWSER_STATE_INITIALIZED:
    break; 
  case SHELL_BROWSER_STATE_ACTIVE: 
    handler( sb);
    break; 
  default: 
    break; 
  } 
}

void shell_browser_process_console( struct shell_browser* sb)
{ shell_browser_process( sb, shell_browser_handle_console); }

void shell_browser_process_shell( struct shell_browser* sb)
{ shell_browser_process( sb, shell_browser_handle_shell); }


void shell_browser_thread(void *sb_handle, void *, void *)
{
  struct shell_browser *sb = sb_handle;
  int err;
  //err = shell->iface->api->enable(shell->iface, false);
  //if (err != 0) {
  //  return;
  //}
  /* Enable shell and print prompt. */
  //err = shell_start(shell);
  //if (err != 0) {
  //  return;
  //}
  while (true) {
    /* waiting for all signals except
       SHELL_BROWSER_SIGNAL_CONSOLE_TX_DONE and
       SHELL_BROWSER_SIGNAL_SHELL_TX_DONE */
    err = k_poll(sb->ctx->events, SHELL_BROWSER_SIGNAL_CONSOLE_TX_DONE,
		 K_FOREVER);
    if (err != 0) {
      //k_mutex_lock(&shell_browser->ctx->wr_mtx, K_FOREVER);
      //z_shell_fprintf(shell, SHELL_ERROR,
      //"Shell thread error: %d", err);
      //k_mutex_unlock(&shell_browser->ctx->wr_mtx);
      return;
    }
    k_mutex_lock(&sb->ctx->wr_mtx, K_FOREVER);
    shell_browser_signal_handle( sb, SHELL_BROWSER_SIGNAL_KILL, kill_handler);
    shell_browser_signal_handle( sb, SHELL_BROWSER_SIGNAL_CONSOLE_RX_RDY, shell_browser_process_console);
    shell_browser_signal_handle( sb, SHELL_BROWSER_SIGNAL_SHELL_RX_RDY, shell_browser_process_shell);
    k_mutex_unlock(&sb->ctx->wr_mtx);
  }
}

static int instance_init( const struct shell_browser *sb,
			  const void *config)
{
  memset( sb->ctx, 0, sizeof(*sb->ctx));
  sb->ctx->prompt = sb->default_prompt;

  k_mutex_init( &sb->ctx->wr_mtx);
  
  for (int i = 0; i < SHELL_BROWSER_EVENTS; i++) {
    k_poll_signal_init( &sb->ctx->signals[i]);
    k_poll_event_init( &sb->ctx->events[i],
		       K_POLL_TYPE_SIGNAL,
		       K_POLL_MODE_NOTIFY_ONLY,
		       &sb->ctx->signals[i]);
  }
  int ret = sb->iface->api->init
    ( sb->iface, config, evt_handler, (void *)sb);
  ret = ret || sb->shell_iface->api->init( sb->shell_iface, (void *)sb);
  //if (ret == 0) {
  //  state_set( sb, SHELL_BROWSER_STATE_INITIALIZED);
  //}
  return ret;
}


int shell_browser_init(const struct shell_browser *sb,
		       const void *config)
{
  __ASSERT_NO_MSG( sb);
  __ASSERT_NO_MSG( sb->ctx && sb->iface);
  
  if (sb->ctx->tid) {
    return -EALREADY;
  }
  int err = instance_init( sb, config);
  if (err != 0) { return err; }
  k_tid_t tid = k_thread_create
    ( sb->thread, sb->stack, CONFIG_SHELL_BROWSER_STACK_SIZE,
      shell_browser_thread, (void *)sb, UINT_TO_POINTER( 0), UINT_TO_POINTER( 0),
      CONFIG_SHELL_BROWSER_THREAD_PRIORITY, 0, K_NO_WAIT);
  
  sb->ctx->tid = tid;
  k_thread_name_set(tid, sb->thread_name);
  
  return 0;
}
