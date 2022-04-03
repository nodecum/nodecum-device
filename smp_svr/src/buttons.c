#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include "uievq.h"
#include "buttons.h"

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER( buttons);

#define NR_OF_BUTTONS CONFIG_NUMBER_OF_BUTTONS

static uint8_t default_pin_values[NR_OF_BUTTONS];
static struct gpio_callback callback[NR_OF_BUTTONS];

/*
 * Create gpio_dt_spec structures from the devicetree.
 */
#define BUTTON_SPEC( n, _)					\
  static const struct gpio_dt_spec sw##n =			\
    GPIO_DT_SPEC_GET( DT_ALIAS( sw##n ), gpios);

UTIL_LISTIFY( NR_OF_BUTTONS, BUTTON_SPEC);

/* Devices */

#define BUTTON_CALLBACK( n, _)						\
  void btn##n( const struct device *gpio, struct gpio_callback* cb, uint32_t val) \
  {									\
    uint32_t ev = 0;							\
    ev = FIELD_PREP( EVENT_TYPE_MASK, BTN_EVENT)			\
      | FIELD_PREP( BTN_NR_MASK, (n));					\
    int ret = gpio_pin_get( gpio, sw##n.pin);				\
    if (ret < 0) {							\
      LOG_ERR("Failed to get the state of port %s pin %u, error: %d",	\
	      gpio->name, sw##n.pin, ret);				\
      return;								\
    }									\
    if (default_pin_values[(n)] != (uint8_t)ret) {			\
      ev |= BTN_STATE_BIT;						\
    } else {								\
      ev &= ~BTN_STATE_BIT;						\
    }									\
    ui_evq_put( ev);							\
  }

UTIL_LISTIFY( NR_OF_BUTTONS, BUTTON_CALLBACK);

int callback_configure
( const struct gpio_dt_spec *spec,
  gpio_callback_handler_t handler,
  struct gpio_callback *cb )
{
  if (!spec->port) {
    LOG_ERR("Could not find gpio PORT");
    return -ENXIO;
  }
  gpio_pin_configure
    ( spec->port, spec->pin, GPIO_INPUT | GPIO_INT_DEBOUNCE | spec->dt_flags);
  gpio_init_callback( cb, handler, BIT( spec->pin));
  gpio_add_callback( spec->port, cb);
  gpio_pin_interrupt_configure( spec->port, spec->pin, GPIO_INT_EDGE_BOTH);
  return 0;
}

#define CALLBACK_CONFIGURE(n, _)			\
  callback_configure( &sw##n, &btn##n, &callback[n] );						

void buttons_init()
{
  UTIL_LISTIFY( NR_OF_BUTTONS, CALLBACK_CONFIGURE);
}

 
  
