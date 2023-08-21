#ifndef at_util_h
#define at_util_h

#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>

esp_err_t at_util_string2hex(const char *str, uint8_t **output, size_t *output_len);

esp_err_t at_util_hex2string(const uint8_t *input, size_t input_len, char **output);

#endif