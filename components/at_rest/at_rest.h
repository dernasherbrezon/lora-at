#ifndef LORA_AT_AT_REST_H
#define LORA_AT_AT_REST_H

#include <esp_err.h>
#include <sx127x_util.h>

typedef struct at_rest_t at_rest;

esp_err_t at_rest_create(sx127x *device, at_rest **result);

void at_rest_destroy(at_rest *result);

#endif //LORA_AT_AT_REST_H
