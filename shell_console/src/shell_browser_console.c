#include "shell_browser_console.h"
#include "shell_browser.h"

struct shell_browser_console *sbc = NULL;

static int init( const struct shell_transport *transport,
		 const void *config,
		 shell_transport_handler_t handler,
		 void *context)
{
  sbc = (struct shell_browser_console *) transport->ctx;  
  sbc->shell_handler = handler;
  sbc->shell_context = context;
  return 0;
}

static int uninit(const struct shell_transport *transport)
{ if ( sbc == NULL) { return -ENODEV; } return 0; }

static int enable(const struct shell_transport *transport, bool blocking)
{ if ( sbc == NULL) { return -ENODEV; } return 0; }

static void *write_data = NULL;
static size_t *write_cnt = 0;

static size_t write_to_buf( const char *data, size_t length, struct sect_str_buf *b)
{
  b->begin = 0;
  if ( length > b->max_size ) lenght = b->max_size;
  b->end = length;
  size_t i = 0;
  while ( i < length ) b->buffer[i] = data[i++];
  return length;
}

static size_t read_from_buf( struct sect_str_buf *b, char *data, size_t length)
{
  size_t i = b->begin;
  size_t j = 0;
  while( i < b->end && j < length) data[j++] = b->buffer[i++];
  b->begin += j;
  return j;
}

static int write(const struct shell_transport *transport,
		 const void *data, size_t length, size_t *cnt)
{
  if ( sbc == NULL) { *cnt = 0; return -ENODEV; }
  *cnt = write_to_buf( (const char* ) data, length, sbc->shell_write_buf);
  sbc->shell_browser_handler( SHELL_BROWSER_EVENT_SHELL_RX_RDY,
			      sbc->shell_browser_context);
  return 0;
}

static int read_from_shell( const struct shell_browser_transport *transport,
			    char *data, size_t length, size_t *cnt)
{
  if ( sbc == NULL) { *cnt = 0; return -ENODEV; }
  const struct sect_str_buf *b = sbc->shell_write_buf;
  *cnt = read_from_buf( b, data, length);
  if( b->begin == b->end) {
    // all data was read
    sbc->shell_handler( SHELL_TRANSPORT_EVT_TX_RDY, sbc->shell_context);
  }
  return 0;
}

static int write_to_shell( const struct shell_browser_transport *transport,
			   const char *data, size_t length, size_t *cnt)
{
  if ( sbc == NULL) { *cnt = 0; return -ENODEV; }
  *cnt = write_to_buf( data, length, sbc->shell_read_buf);
  sbc->shell_handler( SHELL_TRANSPORT_EVT_RX_RDY,
		      sbc->shell_context);
  return 0;
}

static int read( const struct shell_transport *transport,
		 void *data, size_t length, size_t *cnt)
{
  if ( sbc == NULL) { *cnt=0; return -ENODEV; }
  const struct sect_str_buf *b = sbc->shell_read_buf;
  *cnt = read_from_buf( b, data, length);
  if( b->begin == b->end) {
    // all data was read
    sbc->shell_browser_handler( SHELL_BROWSER_EVENT_SHELL_TX_DONE,
				sbc->shell_browser_context);
  }
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



