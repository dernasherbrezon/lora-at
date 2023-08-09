#ifndef util_h
#define util_h

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int util_string2hex(const char *str, uint8_t **output, size_t *output_len);
int util_hex2string(const uint8_t *input, size_t input_len, char **output);

#ifdef __cplusplus
}
#endif

#endif