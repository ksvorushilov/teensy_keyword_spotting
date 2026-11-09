#ifndef CUSTOM_FIFO_H
#define CUSTOM_FIFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BUFFER_MAX_LENGTH 257  // FFT len + 1 dummy

  typedef int16_t buf_data_t;
  typedef struct
  {
    buf_data_t data[BUFFER_MAX_LENGTH];
    buf_data_t *volatile tail;
    uint16_t len;

  } buffer_t;

  void buffer_init(buffer_t *buffer, uint16_t len);
  void buffer_flush(buffer_t *buffer);
  bool buffer_write(buffer_t *buffer, buf_data_t val);
  bool buffer_full(buffer_t *buffer);

#ifdef __cplusplus
}
#endif

#endif