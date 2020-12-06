#pragma once

#include <vector>
#include <cassert>

class SimpleStorage
{
public:
  inline static const int undefined = -1;

  SimpleStorage(int d_max) : d_max(d_max)
  {
    for (int i = 0; i <= d_max; i++)
    {
      data.emplace_back(2 * i + 1, undefined);
    }
  }

  inline int &at(int d, int k)
  {
    assert(d >= 0 && d <= d_max);
    assert(abs(k) <= d);
    assert((d - abs(k)) % 2 == 0);
    return data.at(d).at(k + d);
  }

private:
  int d_max;
  std::vector<std::vector<int>> data;
};