#include "buttons.h"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER( buttons);

#define NR_OF_BUTTONS CONFIG_NUMBER_OF_BUTTONS

static uint8_t default_pin_values[NR_OF_BUTTONS];
static struct gpio_callback callback[NR_OF_BUTTONS];

K_EVENT_DEFINE( bt_events);

extern struct k_event* button_events()
{ return &bt_events; }

/*
 * Create gpio_dt_spec structures from the devicetree.
 */
#define BUTTON_SPEC( n, _)					\
  static const struct gpio_dt_spec sw##n =			\
    GPIO_DT_SPEC_GET_OR( DT_ALIAS( sw##n ), gpios, {0});

LISTIFY( NR_OF_BUTTONS, BUTTON_SPEC, (;));

/* Devices */

#define BUTTON_CALLBACK( n, _)						\
  void btn##n( const struct device *gpio, struct gpio_callback* cb, uint32_t val) \
  {									\
    const int ret = gpio_pin_get( gpio, sw##n.pin);			\
    if (ret < 0) {							\
      LOG_ERR("Failed to get the state of port %s pin %u, error: %d",	\
	      gpio->name, sw##n.pin, ret);				\
      return;								\
    }									\
    k_event_set_masked							\
      ( &bt_events,							\
	(default_pin_values[(n)] != (uint8_t)ret) ?			\
      BIT( 2*n+1) : BIT( 2*n), GENMASK( 2*n+1, 2*n));			\
  }

LISTIFY( NR_OF_BUTTONS, BUTTON_CALLBACK, (;));

int callback_configure
( const struct gpio_dt_spec *spec,
  gpio_callback_handler_t handler,
  struct gpio_callback *cb )
{
  if (!spec->port) {
    LOG_ERR("Could not find gpio PORT");
    return -ENXIO;
  }
  gpio_pin_configure_dt( spec, GPIO_INPUT /* | GPIO_INT_DEBOUNCE */ );
  gpio_init_callback( cb, handler, BIT( spec->pin));
  gpio_add_callback( spec->port, cb);
  gpio_pin_interrupt_configure( spec->port, spec->pin, GPIO_INT_EDGE_BOTH);
  return 0;
}

#define CALLBACK_CONFIGURE(n, _)			\
  callback_configure( &sw##n, &btn##n, &callback[n] );						

void init_buttons()
{
  LISTIFY( NR_OF_BUTTONS, CALLBACK_CONFIGURE, (;));
}

