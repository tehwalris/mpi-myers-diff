#define CATCH_CONFIG_MAIN

#include <iostream>
#include <vector>
#include <optional>
#include <cassert>
#include <algorithm>
#include "../../lib/catch/catch_amalgamated.hpp"
#include "strategy.hpp"
#include "geometry.hpp"
#include "side.hpp"
#include "partition.hpp"

template <class S>
class TestStrategyFollower
{
public:
  TestStrategyFollower(S *storage) : storage(storage), num_calculated(0){};

  inline void set(int d, int k, int v)
  {
    int &stored = storage->at(d, k);
    assert(stored == S::undefined);
    stored = v;
  }

  void calculate(int d, int k)
  {
    assert(d >= 0 && abs(k) <= d);
    if (k > -d)
    {
      get(d - 1, k - 1);
    }
    if (k < d)
    {
      get(d - 1, k + 1);
    }
    set(d, k, 0); // fake value
    num_calculated++;
  }

  void send(int d, int k, Side to)
  {
    get(d, k);
  }

  int get_num_directly_calculated()
  {
    return num_calculated;
  }

private:
  S *storage;
  int num_calculated;

  inline int get(int d, int k)
  {
    int stored = storage->at(d, k);
    assert(stored != S::undefined);
    return stored;
  }
};

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

TEST_CASE("Strategy - concrete example (green)")
{
  // This tests the strategy with the tasks of the green worker from the custom figure in our first (progress) presentation

  const int d_max = 7;
  PerSide<std::vector<CellLocation>> future_receives(std::vector<CellLocation>{}, std::vector<CellLocation>{});
  future_receives.at(Side::Left).emplace_back(0, 0);
  future_receives.at(Side::Left).emplace_back(1, -1);
  future_receives.at(Side::Left).emplace_back(3, -1);
  future_receives.at(Side::Left).emplace_back(4, -2);
  future_receives.at(Side::Left).emplace_back(6, -2);
  future_receives.at(Side::Right).emplace_back(2, 2);
  future_receives.at(Side::Right).emplace_back(3, 3);
  future_receives.at(Side::Right).emplace_back(5, 3);
  future_receives.at(Side::Right).emplace_back(6, 4);
  PerSide<std::vector<CellLocation>::const_iterator> future_receive_begins(future_receives.at(Side::Left).begin(), future_receives.at(Side::Right).begin());
  PerSide<std::vector<CellLocation>::const_iterator> future_receive_ends(future_receives.at(Side::Left).end(), future_receives.at(Side::Right).end());

  SimpleStorage storage(d_max);
  TestStrategyFollower<SimpleStorage> follower(&storage);
  Strategy strategy(&follower, future_receive_begins, future_receive_ends, d_max);

  const int dummy = 0;

  REQUIRE(!strategy.is_done());
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 0);

  strategy.receive(Side::Left, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 1);

  strategy.receive(Side::Left, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 2);

  strategy.receive(Side::Left, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 2);

  strategy.receive(Side::Right, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 4);

  strategy.receive(Side::Right, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 6);

  strategy.receive(Side::Right, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 7);

  strategy.receive(Side::Left, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 10);

  strategy.receive(Side::Left, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 11);

  REQUIRE(!strategy.is_done());
  strategy.receive(Side::Right, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 12);
  REQUIRE(strategy.is_done());

  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 12);
  REQUIRE(strategy.is_done());
}

TEST_CASE("Strategy - whole pyramid")
{
  const int d_max = 20;
  PerSide<std::vector<CellLocation>> future_receives(std::vector<CellLocation>{}, std::vector<CellLocation>{});
  PerSide<std::vector<CellLocation>::const_iterator> future_receive_begins(future_receives.at(Side::Left).begin(), future_receives.at(Side::Right).begin());
  PerSide<std::vector<CellLocation>::const_iterator> future_receive_ends(future_receives.at(Side::Left).end(), future_receives.at(Side::Right).end());

  SimpleStorage storage(d_max);
  TestStrategyFollower<SimpleStorage> follower(&storage);
  Strategy strategy(&follower, future_receive_begins, future_receive_ends, d_max);

  REQUIRE(!strategy.is_done());
  REQUIRE(follower.get_num_directly_calculated() == 0);
  strategy.run();
  REQUIRE(strategy.is_done());
  REQUIRE(follower.get_num_directly_calculated() == ((d_max + 1) * (d_max + 1) + (d_max + 1)) / 2);
}

TEST_CASE("Partition - concrete example (green)")
{
  // This tests the partition calculator with the tasks of the green worker from the custom figure in our first (progress) presentation

  Partition partition(3, 1);

  // d 0
  REQUIRE(!partition.has_work());
  partition.next_d_layer();

  // starting from d 1
  std::vector<std::pair<int, int>> k_ranges{
      std::make_pair(1, 1),
      std::make_pair(0, 0),
      std::make_pair(1, 1),
      std::make_pair(0, 2),
      std::make_pair(-1, 1),
      std::make_pair(0, 2),
      std::make_pair(-1, 3),
  };

  for (auto expected_k_range : k_ranges)
  {
    REQUIRE(partition.has_work());
    REQUIRE(partition.get_k_range().first == expected_k_range.first);
    REQUIRE(partition.get_k_range().second == expected_k_range.second);
    partition.next_d_layer();
  }
}

TEST_CASE("Partition - concrete example (red)")
{
  // This tests the partition calculator with the tasks of the red worker from the custom figure in our first (progress) presentation

  Partition partition(3, 0);

  // starting from d 0
  std::vector<std::pair<int, int>> k_ranges{
      std::make_pair(0, 0),
      std::make_pair(-1, -1),
      std::make_pair(-2, -2),
      std::make_pair(-3, -1),
      std::make_pair(-4, -2),
      std::make_pair(-5, -3),
      std::make_pair(-6, -2),
      std::make_pair(-7, -3),
  };

  for (auto expected_k_range : k_ranges)
  {
    REQUIRE(partition.has_work());
    REQUIRE(partition.get_k_range().first == expected_k_range.first);
    REQUIRE(partition.get_k_range().second == expected_k_range.second);
    partition.next_d_layer();
  }
}