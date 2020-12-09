#include <vector>
#include <optional>
#include <cmath>
#include <cassert>

template <class S>
inline std::optional<int> calculate_row_shared(
    S &storage,
    const std::vector<int> &in_1,
    const std::vector<int> &in_2,
    int d,
    int k_min,
    int k_max)
{
  assert(d >= 0 && abs(k_min) <= d && abs(k_max) <= d);
  assert(k_min <= k_max);

  int in_1_size = in_1.size(), in_2_size = in_2.size();
  int *last_row = storage.get_raw_row(d - 1);
  int *this_row = storage.get_raw_row(d);

  for (int k = k_min; k <= k_max; k += 2)
  {
    int x;
    if (d == 0)
    {
      x = 0;
    }
    else if (k == -d || k != d && last_row[k - 1] < last_row[k + 1])
    {
      x = last_row[k + 1];
    }
    else
    {
      x = last_row[k - 1] + 1;
    }

    int y = x - k;

    while (x < in_1_size && y < in_2_size && in_1[x] == in_2[y])
    {
      x++;
      y++;
    }

    this_row[k] = x;

    if (x >= in_1_size && y >= in_2_size && k == in_1_size - in_2_size)
    {
      return std::optional{k};
    }
  }

  return std::nullopt;
}