#ifndef at_util_h
#define at_util_h

#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>

esp_err_t at_util_string2hex(const char *str, uint8_t *output, size_t *output_len);

esp_err_t at_util_hex2string(const uint8_t *input, size_t input_len, char *output);

typedef struct at_util_vector {
  void *data;
  struct at_util_vector *next;
} at_util_vector_t;

esp_err_t at_util_vector_create(at_util_vector_t **vector);

esp_err_t at_util_vector_add(void *data, at_util_vector_t *vector);

uint16_t at_util_vector_size(at_util_vector_t *vector);

void at_util_vector_get(uint16_t index, void **output, at_util_vector_t *vector);

void at_util_vector_clear(at_util_vector_t *vector);

void at_util_vector_destroy(at_util_vector_t *vector);

#endif