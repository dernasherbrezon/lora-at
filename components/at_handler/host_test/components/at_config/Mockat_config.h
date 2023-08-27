/* AUTOGENERATED FILE. DO NOT EDIT. */
#ifndef _MOCKAT_CONFIG_H
#define _MOCKAT_CONFIG_H

#include "unity.h"
#include "at_config.h"

/* Ignore the following warnings, since we are copying code */
#if defined(__GNUC__) && !defined(__ICC) && !defined(__TMS470__)
#if __GNUC__ > 4 || (__GNUC__ == 4 && (__GNUC_MINOR__ > 6 || (__GNUC_MINOR__ == 6 && __GNUC_PATCHLEVEL__ > 0)))
#pragma GCC diagnostic push
#endif
#if !defined(__clang__)
#pragma GCC diagnostic ignored "-Wpragmas"
#endif
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wduplicate-decl-specifier"
#endif

void Mockat_config_Init(void);
void Mockat_config_Destroy(void);
void Mockat_config_Verify(void);




#define lora_at_config_create_IgnoreAndReturn(cmock_retval) lora_at_config_create_CMockIgnoreAndReturn(__LINE__, cmock_retval)
void lora_at_config_create_CMockIgnoreAndReturn(UNITY_LINE_TYPE cmock_line, esp_err_t cmock_to_return);
#define lora_at_config_create_StopIgnore() lora_at_config_create_CMockStopIgnore()
void lora_at_config_create_CMockStopIgnore(void);
#define lora_at_config_create_ExpectAnyArgsAndReturn(cmock_retval) lora_at_config_create_CMockExpectAnyArgsAndReturn(__LINE__, cmock_retval)
void lora_at_config_create_CMockExpectAnyArgsAndReturn(UNITY_LINE_TYPE cmock_line, esp_err_t cmock_to_return);
#define lora_at_config_create_ExpectAndReturn(config, cmock_retval) lora_at_config_create_CMockExpectAndReturn(__LINE__, config, cmock_retval)
void lora_at_config_create_CMockExpectAndReturn(UNITY_LINE_TYPE cmock_line, lora_at_config_t** config, esp_err_t cmock_to_return);
typedef esp_err_t (* CMOCK_lora_at_config_create_CALLBACK)(lora_at_config_t** config, int cmock_num_calls);
void lora_at_config_create_AddCallback(CMOCK_lora_at_config_create_CALLBACK Callback);
void lora_at_config_create_Stub(CMOCK_lora_at_config_create_CALLBACK Callback);
#define lora_at_config_create_StubWithCallback lora_at_config_create_Stub
#define lora_at_config_create_ExpectWithArrayAndReturn(config, config_Depth, cmock_retval) lora_at_config_create_CMockExpectWithArrayAndReturn(__LINE__, config, config_Depth, cmock_retval)
void lora_at_config_create_CMockExpectWithArrayAndReturn(UNITY_LINE_TYPE cmock_line, lora_at_config_t** config, int config_Depth, esp_err_t cmock_to_return);
#define lora_at_config_create_ReturnThruPtr_config(config) lora_at_config_create_CMockReturnMemThruPtr_config(__LINE__, config, sizeof(lora_at_config_t*))
#define lora_at_config_create_ReturnArrayThruPtr_config(config, cmock_len) lora_at_config_create_CMockReturnMemThruPtr_config(__LINE__, config, (int)(cmock_len * (int)sizeof(*config)))
#define lora_at_config_create_ReturnMemThruPtr_config(config, cmock_size) lora_at_config_create_CMockReturnMemThruPtr_config(__LINE__, config, cmock_size)
void lora_at_config_create_CMockReturnMemThruPtr_config(UNITY_LINE_TYPE cmock_line, lora_at_config_t** config, int cmock_size);
#define lora_at_config_create_IgnoreArg_config() lora_at_config_create_CMockIgnoreArg_config(__LINE__)
void lora_at_config_create_CMockIgnoreArg_config(UNITY_LINE_TYPE cmock_line);
#define lora_at_config_set_display_IgnoreAndReturn(cmock_retval) lora_at_config_set_display_CMockIgnoreAndReturn(__LINE__, cmock_retval)
void lora_at_config_set_display_CMockIgnoreAndReturn(UNITY_LINE_TYPE cmock_line, esp_err_t cmock_to_return);
#define lora_at_config_set_display_StopIgnore() lora_at_config_set_display_CMockStopIgnore()
void lora_at_config_set_display_CMockStopIgnore(void);
#define lora_at_config_set_display_ExpectAnyArgsAndReturn(cmock_retval) lora_at_config_set_display_CMockExpectAnyArgsAndReturn(__LINE__, cmock_retval)
void lora_at_config_set_display_CMockExpectAnyArgsAndReturn(UNITY_LINE_TYPE cmock_line, esp_err_t cmock_to_return);
#define lora_at_config_set_display_ExpectAndReturn(init_display, config, cmock_retval) lora_at_config_set_display_CMockExpectAndReturn(__LINE__, init_display, config, cmock_retval)
void lora_at_config_set_display_CMockExpectAndReturn(UNITY_LINE_TYPE cmock_line, bool init_display, lora_at_config_t* config, esp_err_t cmock_to_return);
typedef esp_err_t (* CMOCK_lora_at_config_set_display_CALLBACK)(bool init_display, lora_at_config_t* config, int cmock_num_calls);
void lora_at_config_set_display_AddCallback(CMOCK_lora_at_config_set_display_CALLBACK Callback);
void lora_at_config_set_display_Stub(CMOCK_lora_at_config_set_display_CALLBACK Callback);
#define lora_at_config_set_display_StubWithCallback lora_at_config_set_display_Stub
#define lora_at_config_set_display_ExpectWithArrayAndReturn(init_display, config, config_Depth, cmock_retval) lora_at_config_set_display_CMockExpectWithArrayAndReturn(__LINE__, init_display, config, config_Depth, cmock_retval)
void lora_at_config_set_display_CMockExpectWithArrayAndReturn(UNITY_LINE_TYPE cmock_line, bool init_display, lora_at_config_t* config, int config_Depth, esp_err_t cmock_to_return);
#define lora_at_config_set_display_ReturnThruPtr_config(config) lora_at_config_set_display_CMockReturnMemThruPtr_config(__LINE__, config, sizeof(lora_at_config_t))
#define lora_at_config_set_display_ReturnArrayThruPtr_config(config, cmock_len) lora_at_config_set_display_CMockReturnMemThruPtr_config(__LINE__, config, (int)(cmock_len * (int)sizeof(*config)))
#define lora_at_config_set_display_ReturnMemThruPtr_config(config, cmock_size) lora_at_config_set_display_CMockReturnMemThruPtr_config(__LINE__, config, cmock_size)
void lora_at_config_set_display_CMockReturnMemThruPtr_config(UNITY_LINE_TYPE cmock_line, lora_at_config_t* config, int cmock_size);
#define lora_at_config_set_display_IgnoreArg_init_display() lora_at_config_set_display_CMockIgnoreArg_init_display(__LINE__)
void lora_at_config_set_display_CMockIgnoreArg_init_display(UNITY_LINE_TYPE cmock_line);
#define lora_at_config_set_display_IgnoreArg_config() lora_at_config_set_display_CMockIgnoreArg_config(__LINE__)
void lora_at_config_set_display_CMockIgnoreArg_config(UNITY_LINE_TYPE cmock_line);
#define lora_at_config_destroy_Ignore() lora_at_config_destroy_CMockIgnore()
void lora_at_config_destroy_CMockIgnore(void);
#define lora_at_config_destroy_StopIgnore() lora_at_config_destroy_CMockStopIgnore()
void lora_at_config_destroy_CMockStopIgnore(void);
#define lora_at_config_destroy_ExpectAnyArgs() lora_at_config_destroy_CMockExpectAnyArgs(__LINE__)
void lora_at_config_destroy_CMockExpectAnyArgs(UNITY_LINE_TYPE cmock_line);
#define lora_at_config_destroy_Expect(config) lora_at_config_destroy_CMockExpect(__LINE__, config)
void lora_at_config_destroy_CMockExpect(UNITY_LINE_TYPE cmock_line, lora_at_config_t* config);
typedef void (* CMOCK_lora_at_config_destroy_CALLBACK)(lora_at_config_t* config, int cmock_num_calls);
void lora_at_config_destroy_AddCallback(CMOCK_lora_at_config_destroy_CALLBACK Callback);
void lora_at_config_destroy_Stub(CMOCK_lora_at_config_destroy_CALLBACK Callback);
#define lora_at_config_destroy_StubWithCallback lora_at_config_destroy_Stub
#define lora_at_config_destroy_ExpectWithArray(config, config_Depth) lora_at_config_destroy_CMockExpectWithArray(__LINE__, config, config_Depth)
void lora_at_config_destroy_CMockExpectWithArray(UNITY_LINE_TYPE cmock_line, lora_at_config_t* config, int config_Depth);
#define lora_at_config_destroy_ReturnThruPtr_config(config) lora_at_config_destroy_CMockReturnMemThruPtr_config(__LINE__, config, sizeof(lora_at_config_t))
#define lora_at_config_destroy_ReturnArrayThruPtr_config(config, cmock_len) lora_at_config_destroy_CMockReturnMemThruPtr_config(__LINE__, config, (int)(cmock_len * (int)sizeof(*config)))
#define lora_at_config_destroy_ReturnMemThruPtr_config(config, cmock_size) lora_at_config_destroy_CMockReturnMemThruPtr_config(__LINE__, config, cmock_size)
void lora_at_config_destroy_CMockReturnMemThruPtr_config(UNITY_LINE_TYPE cmock_line, lora_at_config_t* config, int cmock_size);
#define lora_at_config_destroy_IgnoreArg_config() lora_at_config_destroy_CMockIgnoreArg_config(__LINE__)
void lora_at_config_destroy_CMockIgnoreArg_config(UNITY_LINE_TYPE cmock_line);

#if defined(__GNUC__) && !defined(__ICC) && !defined(__TMS470__)
#if __GNUC__ > 4 || (__GNUC__ == 4 && (__GNUC_MINOR__ > 6 || (__GNUC_MINOR__ == 6 && __GNUC_PATCHLEVEL__ > 0)))
#pragma GCC diagnostic pop
#endif
#endif

#endif
