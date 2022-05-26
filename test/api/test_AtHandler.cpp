#include <AtHandler.h>
#include <unity.h>

#include "MockLoRaModule.h"
#include "StreamString.h"

MockLoRaModule mock;

void test_at_command() {
  AtHandler handler(&mock);
  StreamString request;
  request.print("AT\r\n");
  StreamString response;
  handler.handle(&request, &response);
  TEST_ASSERT_EQUAL_STRING("OK\r\n", response.c_str());
}