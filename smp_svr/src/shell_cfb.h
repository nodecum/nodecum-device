#ifndef SHELL_CFB_H__
#define SHELL_CFB_H__

#include <shell/shell.h>

#ifdef __cplusplus
extern "C" {
#endif
  
  extern const struct shell_transport_api shell_cfb_transport_api;
  
  /** @brief Shell CFB transport instance control block (RW data). */
  /* struct shell_cfb { */
  /*   const struct device *dev; */
  /*   shell_transport_handler_t handler; */
  /*   void *context; */
  /* }; */
  struct shell_cfb {
    /** Handler function registered by shell. */
    shell_transport_handler_t shell_handler;
    /** Context registered by shell. */
    void *shell_context;
  };

#define SHELL_CFB_DEFINE(_name)						\
  static struct shell_cfb _name##_shell_cfb;				\
  struct shell_transport _name = {					\
    .api = &shell_cfb_transport_api,					\
    .ctx = (struct shell_cfb *)&_name##_shell_cfb			\
  }
  int enable_shell_cfb( const struct device *arg);
  void shell_cfb_event_loop( const struct device *cfb);
#ifdef __cplusplus
}
#endif

#endif /* SHELL_CFB_H__ */

