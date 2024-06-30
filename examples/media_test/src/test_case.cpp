
#include <memory>
#include <stdio.h>

#include "test_base.h"

class ClassA {
public:
	int funcA() {
		return 1;
	}

	long funcB(long a, long b) {
		return a + b;
	}
};

class ClassATest : public testing::Test {
 public:
  void SetUp() override {
	  object_a = std::make_unique<ClassA>();
  }

  void TearDown() override {}

 protected:
  std::unique_ptr<ClassA> object_a;

};

TEST_F(ClassATest, func_a) {
	int result = object_a->funcA();
	EXPECT_EQ(1, result);
	EXPECT_NE(0, result);
}

TEST_F(ClassATest, func_b) {
	int result = object_a->funcA();
	ASSERT_EQ(1, result);
	EXPECT_EQ(3, object_a->funcB(1, 2));
	EXPECT_EQ(7, object_a->funcB(3, 4));
}

TEST(ClassATestSuite, func_a) {
	std::unique_ptr<ClassA> object_a = std::make_unique<ClassA>();
	int result = object_a->funcA();
	EXPECT_EQ(1, result);
	EXPECT_NE(0, result);
}

TEST(ClassATestSuite, DISABLED_donot_run) {
	std::unique_ptr<ClassA> object_a = std::make_unique<ClassA>();
	int result = object_a->funcA();
	ASSERT_EQ(1, result);
	EXPECT_EQ(3, object_a->funcB(1, 2));
	EXPECT_EQ(7, object_a->funcB(3, 4));
}
