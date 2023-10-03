#include "at_util.h"

#include <esp_log.h>
#include <string.h>

const char SYMBOLS[] = "0123456789ABCDEF";
static const char *TAG = "lora-at";

esp_err_t at_util_string2hex(const char *str, uint8_t **output, size_t *output_len) {
  size_t len = 0;
  size_t str_len = strlen(str);
  for (size_t i = 0; i < str_len; i++) {
    if (str[i] == ' ') {
      continue;
    }
    len++;
  }
  if (len % 2 != 0) {
    ESP_LOGE(TAG, "invalid str length");
    *output = NULL;
    *output_len = 0;
    return ESP_ERR_INVALID_SIZE;
  }
  size_t bytes = len / 2;
  uint8_t *result = (uint8_t *) malloc(sizeof(uint8_t) * bytes);
  uint8_t curByte = 0;
  for (size_t i = 0, j = 0; i < str_len; i++) {
    char curChar = str[i];
    if (curChar == ' ') {
      continue;
    }
    curByte *= 16;
    if (curChar > '0' && curChar < '9') {
      curByte += curChar - '0';
    } else if (curChar >= 'A' && curChar <= 'F') {
      curByte += (curChar - 'A') + 10;
    } else if (curChar >= 'a' && curChar <= 'f') {
      curByte += (curChar - 'a') + 10;
    } else {
      ESP_LOGE(TAG, "invalid char: %c", curChar);
      *output = NULL;
      *output_len = 0;
      return ESP_ERR_INVALID_ARG;
    }
    j++;
    if (j % 2 == 0) {
      result[j / 2 - 1] = curByte;
      curByte = 0;
    }
  }
  *output = result;
  *output_len = bytes;
  return ESP_OK;
}

esp_err_t at_util_hex2string(const uint8_t *input, size_t input_len, char **output) {
  char *result = (char *) malloc(sizeof(char) * (input_len * 2 + 1));
  for (size_t i = 0; i < input_len; i++) {
    uint8_t cur = input[i];
    result[2 * i] = SYMBOLS[cur >> 4];
    result[2 * i + 1] = SYMBOLS[cur & 0x0F];
  }
  result[input_len * 2] = '\0';
  *output = result;
  return ESP_OK;
}

esp_err_t at_util_vector_create(size_t elem_size, at_util_vector_t **vector) {
  at_util_vector_t *result = malloc(sizeof(at_util_vector_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->elem_size = elem_size;
  result->elems = 0;
  result->data = NULL;
  *vector = result;
  return ESP_OK;
}

esp_err_t at_util_vector_add(void *data, at_util_vector_t *vector) {
  void *new_data = realloc(vector->data, (vector->elems + 1) * vector->elem_size);
  if (new_data == NULL) {
    return ESP_ERR_NO_MEM;
  }
  vector->data = new_data;
  memcpy(vector->data + vector->elems * vector->elem_size, data, vector->elem_size);
  vector->elems++;
  return ESP_OK;
}

uint16_t at_util_vector_size(at_util_vector_t *vector) {
  return vector->elems;
}

void at_util_vector_get(uint16_t index, void **output, at_util_vector_t *vector) {
  *output = vector->data + (vector->elem_size * index);
}

void at_util_vector_clear(at_util_vector_t *vector) {
  if (vector->data != NULL) {
    free(vector->data);
    vector->data = NULL;
  }
  vector->elems = 0;
}

void at_util_vector_destroy(at_util_vector_t *vector) {
  if (vector == NULL) {
    return;
  }
  if (vector->data != NULL) {
    free(vector->data);
  }
  free(vector);
}