
#include <stdio.h>

#include "test_base.h"

GTEST_API_ int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);

  int result = RUN_ALL_TESTS();
  return result;
}
