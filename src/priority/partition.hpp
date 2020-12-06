#pragma once

#include <vector>
#include <cassert>

class Partition
{
public:
  Partition(
      int num_workers,
      int target_worker) : num_workers(num_workers),
                           target_worker(target_worker)
  {
    assert(num_workers >= 0);
    assert(target_worker >= 0 && target_worker < num_workers);
    next_d_layer();
  };

  void next_d_layer()
  {
    extend(next_worker_to_extend);
    next_worker_to_extend = (next_worker_to_extend + 1) % num_workers;
    d++;
  }

  bool has_work()
  {
    return size_target > 0;
  }

  std::pair<int, int> get_k_range()
  {
    assert(has_work());
    int k_min = -d + 2 * size_before;
    int k_max = k_min + 2 * size_target - 2;
    assert(k_min <= k_max);
    return std::make_pair(k_min, k_max);
  }

private:
  int num_workers;
  int target_worker;
  int d = -1;
  int next_worker_to_extend = 0;
  int size_before = 0;
  int size_target = 0;
  int size_after = 0;

  void extend(int worker)
  {
    assert(worker >= 0 && worker < num_workers);
    if (worker < target_worker)
    {
      size_before++;
    }
    else if (worker > target_worker)
    {
      size_after++;
    }
    else
    {
      size_target++;
    }
  }
};