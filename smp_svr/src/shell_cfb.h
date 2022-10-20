#ifndef SHELL_CFB_H__
#define SHELL_CFB_H__

#include <zephyr/shell/shell.h>
#include "strbuf.h"
#include "shell_parse.h"
#ifdef __cplusplus
extern "C" {
#endif
  
  extern const struct shell_transport_api shell_cfb_transport_api;
  
  struct shell_cfb {
    /** Handler function registered by shell. */
    shell_transport_handler_t shell_handler;
    /** Context registered by shell. */
    void *shell_context;
    struct shell_parse_t *p;
  };

#define SHELL_CFB_DEFINE(_name, _prompt, _cmd_buf_size,			\
			 _alt_buf_size, _out_buf_size)			\
  SHELL_PARSE_DEFINE( _name##_p, _prompt, _cmd_buf_size,		\
		      _alt_buf_size, _out_buf_size);			\
  static struct shell_cfb _name##_shell_cfb = {				\
    .p = &_name##_p_shell_parse						\
  };                                                                    \
  struct shell_transport _name = {					\
    .api = &shell_cfb_transport_api,                                    \
    .ctx = (struct shell_cfb *)&_name##_shell_cfb,                      \
  };
  int enable_shell_cfb( const struct device *arg);
  void shell_cfb_event_loop( const struct device *cfb);
#ifdef __cplusplus
}
#endif

#endif /* SHELL_CFB_H__ */

