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

esp_err_t at_util_vector_create(at_util_vector_t **vector) {
  at_util_vector_t *result = malloc(sizeof(at_util_vector_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->data = NULL;
  result->next = NULL;
  *vector = result;
  return ESP_OK;
}

esp_err_t at_util_vector_add(void *data, at_util_vector_t *vector) {
  at_util_vector_t *next = malloc(sizeof(at_util_vector_t));
  if (next == NULL) {
    return ESP_ERR_NO_MEM;
  }
  next->data = data;
  next->next = NULL;
  at_util_vector_t *last_node = vector;
  while (last_node->next != NULL) {
    last_node = last_node->next;
  }
  last_node->next = next;
  return ESP_OK;
}

uint16_t at_util_vector_size(at_util_vector_t *vector) {
  uint16_t result = 0;
  at_util_vector_t *last_node = vector;
  while (last_node->next != NULL) {
    last_node = last_node->next;
    result++;
  }
  return result;
}

void at_util_vector_get(uint16_t index, void **output, at_util_vector_t *vector) {
  uint16_t current_index = 0;
  at_util_vector_t *last_node = vector;
  while (last_node->next != NULL) {
    if (current_index == index) {
      *output = last_node->next->data;
      return;
    }
    current_index++;
  }
  *output = NULL;
}

void at_util_vector_clear(at_util_vector_t *vector) {
  if (vector->next == NULL) {
    return;
  }
  at_util_vector_t *last_node = vector->next;
  vector->next = NULL;
  while (last_node != NULL) {
    at_util_vector_t *next = last_node->next;
    free(last_node);
    last_node = next;
  }
}

void at_util_vector_destroy(at_util_vector_t *vector) {
  if (vector == NULL) {
    return;
  }
  at_util_vector_clear(vector);
  free(vector);
}