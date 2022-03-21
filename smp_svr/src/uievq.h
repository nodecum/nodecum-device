/*
  user interface event queue
*/

void ui_evq_put( uint32_t);
struct k_timeout_t;
uint32_t ui_evq_get( k_timeout_t timeout);
// 
void ui_evq_flush( void);

void ui_evq_init();
