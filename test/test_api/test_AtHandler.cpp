#include <Arduino.h>
#include <AtHandler.h>
#include <unity.h>

#include "MockLoRaModule.h"
#include "StreamString.h"

MockLoRaModule mock;

const char *VALID_TX_REQUEST = "AT+LORATX=CAFE,433.0,10.4,9,6,18,10,55,0,0\r\n";
const char *INVALID_TX_REQUEST = "AT+LORATX=CA FE,433.0,10.4,9,6,18,10,55,0,0\r\n";
const char *INVALID_TX_DATA_REQUEST = "AT+LORATX=CAXE,433.0,10.4,9,6,18,10,55,0,0\r\n";
const char *UNKNOWN_COMMAND_RESPONSE = "unknown command\r\nERROR\r\n";

void setUp(void) {
  mock.rxCode = ERR_NONE;
  mock.receiving = false;
  mock.txCode = ERR_NONE;
  mock.expectedFrames.clear();
  mock.currentFrameIndex = 0;

  Preferences preferences;
  if (!preferences.begin("lora-at", false)) {
    return;
  }
  preferences.putBool("initialized", false);
  preferences.end();
}

void setupFrame() {
  // should be deleted in the handlePull
  size_t data_len = 2;
  uint8_t *data = (uint8_t *)malloc(sizeof(uint8_t) * data_len);
  data[0] = 0xca;
  data[1] = 0xfe;
  LoRaFrame *frame = new LoRaFrame();
  frame->setData(data);
  frame->setDataLength(data_len);
  frame->setFrequencyError(13.2);
  frame->setRssi(-11.22);
  frame->setSnr(3.2);
  frame->setTimestamp(1605980902);
  mock.expectedFrames.push_back(frame);
}

void assert_request_response(const char *expected, const char *req) {
  AtHandler handler(&mock);
  StreamString request;
  request.print(req);
  StreamString response;
  handler.handle(&request, &response);
  TEST_ASSERT_EQUAL_STRING(expected, response.c_str());
}

void test_at_command() {
  assert_request_response("OK\r\n", "AT\r\n");
}

void test_at_gmr() {
  assert_request_response("1.0\r\nOK\r\n", "AT+GMR\r\n");
}

void test_success_lorarx() {
  assert_request_response("OK\r\n", "AT+LORARX=433.0,10.4,9,6,18,10,55,0,0\r\n");
}

void test_invalidreq_lorarx() {
  assert_request_response(UNKNOWN_COMMAND_RESPONSE, "AT+LORARX=433.0 10.4,9,6,18,10,55,0,0\r\n");
}

void test_unknown_command() {
  assert_request_response(UNKNOWN_COMMAND_RESPONSE, "AT+UNKNOWN\r\n");
}

void test_failed_lorarx() {
  mock.rxCode = -1;
  assert_request_response("Unable to start lora: -1\r\nERROR\r\n", "AT+LORARX=433.0,10.4,9,6,18,10,55,0,0\r\n");
}

void test_success_stop_even_if_not_running(void) {
  assert_request_response("OK\r\n", "AT+STOPRX\r\n");
}

void test_pull(void) {
  setupFrame();
  AtHandler handler(&mock);
  StreamString request;
  request.print("AT+LORARX=433.0,10.4,9,6,18,10,55,0,0\r\n");
  StreamString response;
  handler.handle(&request, &response);
  // force loop on LoraModule
  request.clear();
  response.clear();
  handler.handle(&request, &response);
  request.clear();
  response.clear();
  request.print("AT+PULL\r\n");
  handler.handle(&request, &response);
  TEST_ASSERT_EQUAL_STRING("CAFE,-11.22,3.2,13.2,1605980902\r\nOK\r\n", response.c_str());
}

void test_frames_after_stop(void) {
  setupFrame();
  AtHandler handler(&mock);
  StreamString request;
  request.print("AT+LORARX=433.0,10.4,9,6,18,10,55,0,0\r\n");
  StreamString response;
  handler.handle(&request, &response);
  // force loop on LoraModule
  request.clear();
  response.clear();
  handler.handle(&request, &response);
  request.clear();
  response.clear();
  request.print("AT+STOPRX\r\n");
  handler.handle(&request, &response);
  TEST_ASSERT_EQUAL_STRING("CAFE,-11.22,3.2,13.2,1605980902\r\nOK\r\n", response.c_str());
}

void test_query_chips(void) {
  assert_request_response("0,TTGO - 433/470Mhz\r\n1,TTGO - 868/915Mhz\r\nOK\r\n", "AT+CHIPS?\r\n");
}

void test_set_chip(void) {
  assert_request_response("OK\r\n", "AT+CHIP=1\r\n");
}

void test_set_unknown_chip(void) {
  assert_request_response("Unable to find chip index: 2\r\nERROR\r\n", "AT+CHIP=2\r\n");
}

void test_get_chip(void) {
  AtHandler handler(&mock);
  StreamString request;
  request.print("AT+CHIP?\r\n");
  StreamString response;
  handler.handle(&request, &response);
  TEST_ASSERT_EQUAL_STRING("OK\r\n", response.c_str());

  request.clear();
  response.clear();
  request.print("AT+CHIP=1\r\n");
  handler.handle(&request, &response);

  request.clear();
  response.clear();
  request.print("AT+CHIP?\r\n");
  handler.handle(&request, &response);
  TEST_ASSERT_EQUAL_STRING("TTGO - 868/915Mhz\r\nLORA,863,928\r\nFSK,863,928\r\nOK\r\n", response.c_str());

  // test new instance will load chip from EEPROM
  assert_request_response("TTGO - 868/915Mhz\r\nLORA,863,928\r\nFSK,863,928\r\nOK\r\n", "AT+CHIP?\r\n");
}

void test_cant_tx_during_receive(void) {
  mock.receiving = true;
  assert_request_response("cannot transmit during receive\r\nERROR\r\n", VALID_TX_REQUEST);
}

void test_invalid_tx_request(void) {
  mock.receiving = false;
  assert_request_response(UNKNOWN_COMMAND_RESPONSE, INVALID_TX_REQUEST);
}

void test_invalid_lora_tx_code(void) {
  mock.receiving = false;
  mock.txCode = -1;
  assert_request_response("unable to send data: -1\r\nERROR\r\n", VALID_TX_REQUEST);
}

void test_invalid_tx_data_request(void) {
  mock.receiving = false;
  assert_request_response("unable to convert HEX to byte array: -1\r\nERROR\r\n", INVALID_TX_DATA_REQUEST);
}

void test_success_tx(void) {
  mock.receiving = false;
  assert_request_response("OK\r\n", VALID_TX_REQUEST);
}

void setup() {
  // NOTE!!! Wait for >2 secs
  // if board doesn't support software reset via Serial.DTR/RTS
  delay(2000);

  UNITY_BEGIN();
  RUN_TEST(test_at_command);
  RUN_TEST(test_at_gmr);
  RUN_TEST(test_success_lorarx);
  RUN_TEST(test_invalidreq_lorarx);
  RUN_TEST(test_unknown_command);
  RUN_TEST(test_failed_lorarx);
  RUN_TEST(test_success_stop_even_if_not_running);
  RUN_TEST(test_pull);
  RUN_TEST(test_frames_after_stop);
  RUN_TEST(test_query_chips);
  RUN_TEST(test_set_chip);
  RUN_TEST(test_set_unknown_chip);
  RUN_TEST(test_get_chip);
  RUN_TEST(test_cant_tx_during_receive);
  RUN_TEST(test_invalid_tx_request);
  RUN_TEST(test_invalid_lora_tx_code);
  RUN_TEST(test_invalid_tx_data_request);
  RUN_TEST(test_success_tx);
  UNITY_END();
}

void loop() {
  delay(5000);
}