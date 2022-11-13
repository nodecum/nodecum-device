#ifndef SHELL_CONSOLE_H__
#define SHELL_CONSOLE_H__

#include <zephyr/shell/shell.h>
#include "strbuf.h"
#include "shell_parse.h"
#ifdef __cplusplus
extern "C" {
#endif
  
  extern const struct shell_transport_api shell_console_transport_api;
  
  struct shell_console {
    /** Handler function registered by shell. */
    shell_transport_handler_t shell_handler;
    /** Context registered by shell. */
    void *shell_context;
    struct shell_parse_t *p;
  };

#define SHELL_CONSOLE_DEFINE(_name, _prompt, _cmd_buf_size,		\
			     _alt_buf_size, _out_buf_size)		\
  SHELL_PARSE_DEFINE( _name##_p, _prompt, _cmd_buf_size,		\
		      _alt_buf_size, _out_buf_size);			\
  static struct shell_console _name##_shell_console = {			\
    .p = &_name##_p_shell_parse						\
  };                                                                    \
  struct shell_transport _name = {					\
    .api = &shell_console_transport_api,				\
    .ctx = (struct shell_console *)&_name##_shell_console,		\
  };
  int enable_shell_console( const struct device *arg);
  void shell_console_input_loop();
  void shell_console_output_loop( void*, void*, void*);
#ifdef __cplusplus
}
#endif

#endif /* SHELL_CONSOLE_H__ */

