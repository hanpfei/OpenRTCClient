
#import <Foundation/Foundation.h>
#include <cstdio>
#include <thread>

#include "media_test_utils/test_base.h"

GTEST_API_ int main(int argc, char** argv) {
  int ret = 0;
  std::thread test_thread([&argc, &argv, &ret] {
    testing::InitGoogleTest(&argc, argv);
    ret = RUN_ALL_TESTS();
    exit(ret);
  });

  @autoreleasepool {
    NSRunLoop* runLoop = [NSRunLoop currentRunLoop];
    [runLoop addPort:[NSMachPort port] forMode:NSDefaultRunLoopMode];
    [runLoop run];
  }

  return ret;
}
