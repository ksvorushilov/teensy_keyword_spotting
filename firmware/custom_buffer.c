#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "custom_buffer.h"

void buffer_init(buffer_t *buffer, uint16_t len) {
  buffer->tail = buffer->data;
  buffer->len = len;
}

void buffer_flush(buffer_t *buffer) {
  memset(buffer->data, 0, sizeof(buf_data_t) * buffer->len);
}

bool buffer_full(buffer_t *buffer) {
  buf_data_t *next = buffer->tail + 1;
  return next == buffer->data + buffer->len + 1;
}

bool buffer_write(buffer_t *buffer, buf_data_t val) {
  // potentially unsafe - will write continuously if not reset
  *(buffer->tail) = val;
  buffer->tail += 1;

  return buffer_full(buffer);
}
