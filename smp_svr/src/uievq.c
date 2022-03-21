#include <kernel.h>
#include "uievq.h"

#define LOG_LEVEL LOG_LEVEL_ERR
#include <logging/log.h>
LOG_MODULE_REGISTER( uievq);

struct ui_evt_t {
  sys_snode_t node;
  uint32_t    event;
};

static struct ui_evt_t fifo_data[ CONFIG_UI_EVENT_QUEUE_SIZE];

#define FIFO_ELEM_SZ     sizeof( struct ui_evt_t)
#define FIFO_ELEM_ALIGN  sizeof( unsigned int)

static struct k_fifo fifo_free;
static struct k_fifo fifo_used; 

void ui_evq_init( void)
{
  k_fifo_init( &fifo_free);
  k_fifo_init( &fifo_used);
  for( int i = 0; i < ARRAY_SIZE( fifo_data);  ++i) {
    k_fifo_put( &fifo_free, &fifo_data[i]);
  }
}

void ui_evq_put( uint32_t ev) {
  struct ui_evt_t* ev_ = k_fifo_get( &fifo_free, K_NO_WAIT);
  if( ev_ == NULL) {
    LOG_ERR( "ui_evq_put: failed to aquire free event store.");
  } else {
    ev_->event = ev;
    k_fifo_put( &fifo_used, ev_);
  }
}

uint32_t ui_evq_get( k_timeout_t timeout) {
  struct ui_evt_t* ev_ = k_fifo_get( &fifo_used, timeout);
  if( ev_ == NULL) {
    return 0;
  } else {
    uint32_t ev = ev_->event;
    k_fifo_put( &fifo_free, ev_);
    return ev;
  }
}

void ui_evq_flush( void) {
  uint32_t ev;
  do {
    ev = ui_evq_get( K_NO_WAIT);
  } while (ev != 0);
}

