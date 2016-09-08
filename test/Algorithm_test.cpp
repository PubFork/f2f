#include <gtest/gtest.h>
#include <vector>
#include "util/Algorithm.hpp"

TEST(Algoritm, InsertAndCopyBackward)
{
  std::vector<int> values{7, 22, 1, 40, 13, 4, 12, 3};
  std::vector<int> ins{9, 3, 5};

  auto func = [&](unsigned ins_position) -> std::vector<int>
  {
    std::vector<int> values_copy = values;
    values_copy.resize(values_copy.size() + ins.size());
    f2f::util::InsertAndCopyBackward(
      values_copy.begin(), values_copy.begin() + values.size(),
      values_copy.begin() + ins_position, 
      ins.begin(), ins.end(),
      values_copy.end()
    );
    return values_copy;
  };
  {
    std::vector<int> expected{ 9, 3, 5, 7, 22, 1, 40, 13, 4, 12, 3 };
    EXPECT_EQ(expected, func(0));
  }
  {
    std::vector<int> expected{ 7, 9, 3, 5, 22, 1, 40, 13, 4, 12, 3 };
    EXPECT_EQ(expected, func(1));
  }
  {
    std::vector<int> expected{ 7, 22, 1, 40, 13, 4, 12, 3, 9, 3, 5 };
    EXPECT_EQ(expected, func(values.size()));
  }
}