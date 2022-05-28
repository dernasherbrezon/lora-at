#include <Arduino.h>
#include <AtHandler.h>
#include <EEPROM.h>
#include <unity.h>

#include "MockLoRaModule.h"
#include "StreamString.h"

MockLoRaModule mock;

void setUp(void) {
  mock.rxCode = ERR_NONE;
  mock.receiving = false;
  mock.txCode = ERR_NONE;
  mock.expectedFrames.clear();
  mock.currentFrameIndex = 0;

  // cleanup EEPROM between tests
  EEPROM.begin(sizeof(uint8_t) + sizeof(size_t));
  uint8_t version = 0;  // invalid version
  size_t chip_index = 0;
  EEPROM.writeAll(0, version);
  EEPROM.writeAll(sizeof(uint8_t), chip_index);
  EEPROM.end();
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

void test_at_lorarx() {
  assert_request_response("OK\r\n", "AT+LORARX=433.0,10.4,9,6,18,10,55,0,0\r\n");
}

void test_at_invalidreq_lorarx() {
  assert_request_response("unknown command\r\nERROR\r\n", "AT+LORARX=433.0 10.4,9,6,18,10,55,0,0\r\n");
}

void test_at_unknown_command() {
  assert_request_response("unknown command\r\nERROR\r\n", "AT+UNKNOWN\r\n");
}

void test_at_failed_lorarx() {
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

void setup() {
  // NOTE!!! Wait for >2 secs
  // if board doesn't support software reset via Serial.DTR/RTS
  delay(2000);

  UNITY_BEGIN();
  RUN_TEST(test_at_command);
  RUN_TEST(test_at_gmr);
  RUN_TEST(test_at_lorarx);
  RUN_TEST(test_at_invalidreq_lorarx);
  RUN_TEST(test_at_unknown_command);
  RUN_TEST(test_at_failed_lorarx);
  RUN_TEST(test_success_stop_even_if_not_running);
  RUN_TEST(test_pull);
  RUN_TEST(test_frames_after_stop);
  RUN_TEST(test_query_chips);
  RUN_TEST(test_set_chip);
  UNITY_END();
}

void loop() {
  delay(5000);
}