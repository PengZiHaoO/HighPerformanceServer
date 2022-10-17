#include <gtest/gtest.h>

/* TEST(TestSuiteName, TestName) */

/* TEST_F(TestFixtureName, TestName) */
/**/

TEST(AddTest, HandleTwoPositive) {
	EXPECT_EQ(add(40, 2), 42) << "40 add 2 is equal to 42 \n";
}

TEST(AddTest, HandleTwoNegative) {
	EXPECT_EQ(add(-2, -2), -4) << "40 add 2 is equal to 42\n";
}

TEST(MinTest, NumWithAlpha) {
	EXPECT_EQ(min(2, 5), 5) << "The ans is wrong \n";
}