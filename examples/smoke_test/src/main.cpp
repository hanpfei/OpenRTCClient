
#include <stdio.h>

#include "gtest/gtest.h"
#include "rtc_base/ssl_adapter.h"

GTEST_API_ int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);

  rtc::InitializeSSL();

  int result = RUN_ALL_TESTS();
  return result;
}
