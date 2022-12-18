#ifndef STRBUF_H__
#define STRBUF_H__

#include <zephyr/types.h>

struct str_buf {
  char  *buffer;
  size_t size; // current size of string
  size_t max_size;
};

#define STR_BUF_DECLARE(name, size_)			\
  static char __noinit _str_buf_data_##name[size_];	\
  struct str_buf name = {				\
    .buffer = _str_buf_data_##name,			\
    .size = 0,						\
    .max_size = size_					\
  }

#define STR_BUF_INC( s) if( s->size < (s->max_size-1)) ++(s->size); 

struct sect_str_buf {
  char *buffer;
  size_t begin;
  size_t end;
  size_t max_size;
};

#define SECT_STR_BUF_DECLARE(name, size_)			\
  static char __noinit _sect_str_buf_data_##name[size_ + 1];	\
  struct sect_str_buf name = {					\
    .buffer = _sect_str_buf_data_##name,				\
    .begin = 0,							\
    .end = 0,							\
    .max_size = size_						\
  }

#define SECT_STR_BUF_TERM_ZERO( b)  b->buffer[b->end] = '\0'	



#endif // STRBUF_H__
