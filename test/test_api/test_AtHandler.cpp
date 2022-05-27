#include <Arduino.h>
#include <AtHandler.h>
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
  assert_request_response("OK\r\n", "AT+LORARX=433.0,10.4,9,6,18,10,55,0,0\r\n");
  assert_request_response("CAFE,-11.22,3.2,13.2,1605980902\r\nOK\r\n", "AT+PULL\r\n");
}

void test_frames_after_stop(void) {
  setupFrame();
  assert_request_response("OK\r\n", "AT+LORARX=433.0,10.4,9,6,18,10,55,0,0\r\n");
  assert_request_response("CAFE,-11.22,3.2,13.2,1605980902\r\nOK\r\n", "AT+STOPRX\r\n");
}

void test_query_chips(void) {
  assert_request_response("SX1278 - 433/470Mhz\r\nSX1276 - 433/868/915Mhz\r\nOK\r\n", "AT+CHIP?\r\n");
}

void test_set_chip(void) {
  assert_request_response("OK\r\n", "AT+CHIP=SX1278 - 433/470Mhz\r\n");
}

void test_get_chip(void) {
  //FIXME
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