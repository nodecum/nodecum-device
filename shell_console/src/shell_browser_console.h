#ifndef SHELL_CONSOLE_H__
#define SHELL_CONSOLE_H__

#include <zephyr/shell/shell.h>
#include "strbuf.h"
#include "shell_browser.h"
#ifdef __cplusplus
extern "C" {
#endif
  
  extern const struct shell_transport_api
  shell_browser_console_shell_transport_api;

  extern const struct shell_browser_transport_api
  shell_browser_console_shell_browser_transport_api;
  
  struct shell_browser_console {
    /** Handler function registered by shell. */
    shell_transport_handler_t shell_handler;
    shell_browser_handler_t shell_browser_handler;
    /** Context registered by shell. */
    void *shell_context;
    struct shell_browser *sb;
    struct str_buf *read_buf;
    struct str_buf *write_buf;
  };

#define SHELL_BROWSER_CONSOLE_DEFINE(_name, _prompt, _cmd_buf_size,	\
				     _alt_buf_size, _out_buf_size)	\
  SHELL_BROWSER_DEFINE( _name##_sb, _prompt, _cmd_buf_size,		\
			_alt_buf_size, _out_buf_size);			\
  STR_BUF_DECLARE( _name##_read_buf, 16);				\
  STR_BUF_DECLARE( _name##_write_buf, 16);				\
  static struct shell_browser_console _name##_shell_browser_console = {	\
    .sb =       &_name##_sb,						\
    .read_buf = &_name##_read_buf,					\
    .write_buf = &_name##_write_buf					\
  };									\
  struct shell_transport _name##_shell_transport = {			\
    .api = &shell_browser_console_shell_transport_api,			\
    .ctx = (struct shell_browser_console *)&_name##_shell_browser_console \
  };									\
  struct shell_browser_transport _name##_shell_browser_transport = {	\
    .api = &shell_browser_console_shell_browser_transport_api,		\
    .ctx =(struct shell_browser_console *)&_name##_shell_browser_console \
  };
  
  int enable_shell_browser_console( const struct device *arg);
#ifdef __cplusplus
}
#endif

#endif /* SHELL_CONSOLE_H__ */

