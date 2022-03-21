#include <sys/util.h>

#define BTN_NR_MASK    GENMASK( 5, 0)
#define BTN_STATE_BIT  BIT( 6)
#define BTN_TIME_MASK  GENMASK( 28, 7)    

void buttons_init();
