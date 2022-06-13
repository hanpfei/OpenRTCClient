
#include <stdio.h>

#include "media_test_utils/test_base.h"
#include "rtc_base/ssl_adapter.h"

GTEST_API_ int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);

  rtc::InitializeSSL();

  int result = RUN_ALL_TESTS();
  return result;
}
