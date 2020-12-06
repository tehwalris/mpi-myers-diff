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
  std::vector<std::pair<CellLocation, Side>> sends;

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
    sends.emplace_back(CellLocation(d, k), to);
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

  RoundRobinPartition partition(3, 1);

  ReceiveSideIterator left_receive_begin(partition, Side::Left);
  ReceiveSideIterator left_receive_end(partition);
  ReceiveSideIterator right_receive_begin(partition, Side::Right);
  ReceiveSideIterator right_receive_end(partition);
  PerSide future_receive_begins(left_receive_begin, right_receive_begin);
  PerSide future_receive_ends(left_receive_end, right_receive_end);

  SendSideIterator left_send_begin(partition, Side::Left);
  SendSideIterator left_send_end(partition);
  SendSideIterator right_send_begin(partition, Side::Right);
  SendSideIterator right_send_end(partition);
  PerSide future_send_begins(left_send_begin, right_send_begin);
  PerSide future_send_ends(left_send_end, right_send_end);

  const int d_max = 7;
  SimpleStorage storage(d_max);
  TestStrategyFollower<SimpleStorage> follower(&storage);
  Strategy strategy(&follower, future_receive_begins, future_receive_ends, future_send_begins, future_send_ends, d_max);

  const int dummy = 0;

  REQUIRE(!strategy.is_done());
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 0);
  REQUIRE(follower.sends.size() == 0);

  strategy.receive(Side::Left, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 1);
  REQUIRE(follower.sends.size() == 1);

  strategy.receive(Side::Left, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 2);
  REQUIRE(follower.sends.size() == 2);

  strategy.receive(Side::Left, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 2);
  REQUIRE(follower.sends.size() == 2);

  strategy.receive(Side::Right, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 4);
  REQUIRE(follower.sends.size() == 2);

  strategy.receive(Side::Right, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 5);
  REQUIRE(follower.sends.size() == 3);
  REQUIRE(!strategy.is_blocked_waiting_for_receive());
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 6);
  REQUIRE(follower.sends.size() == 3);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 6);
  REQUIRE(follower.sends.size() == 3);
  REQUIRE(strategy.is_blocked_waiting_for_receive());

  strategy.receive(Side::Right, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 7);
  REQUIRE(follower.sends.size() == 3);

  strategy.receive(Side::Left, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 8);
  REQUIRE(follower.sends.size() == 4);
  REQUIRE(!strategy.is_blocked_waiting_for_receive());
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 10);
  REQUIRE(follower.sends.size() == 4);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 10);
  REQUIRE(follower.sends.size() == 4);
  REQUIRE(strategy.is_blocked_waiting_for_receive());

  strategy.receive(Side::Left, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 11);
  REQUIRE(follower.sends.size() == 4);

  REQUIRE(!strategy.is_done());
  strategy.receive(Side::Right, dummy);
  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 12);
  REQUIRE(follower.sends.size() == 4);
  REQUIRE(strategy.is_done());

  strategy.run();
  REQUIRE(follower.get_num_directly_calculated() == 12);
  REQUIRE(follower.sends.size() == 4);
  REQUIRE(strategy.is_done());
  REQUIRE(!strategy.is_blocked_waiting_for_receive());

  std::vector<std::pair<CellLocation, Side>> expected_sends{
      std::make_pair(CellLocation(1, 1), Side::Right),
      std::make_pair(CellLocation(2, 0), Side::Left),
      std::make_pair(CellLocation(4, 2), Side::Right),
      std::make_pair(CellLocation(5, -1), Side::Left),
  };
  REQUIRE(follower.sends.size() == expected_sends.size());
  for (int i = 0; i < expected_sends.size(); i++)
  {
    REQUIRE(follower.sends.at(i).first == expected_sends.at(i).first);
    REQUIRE(follower.sends.at(i).second == expected_sends.at(i).second);
  }
}

TEST_CASE("Strategy - whole pyramid")
{
  const int d_max = 20;
  PerSide<std::vector<CellLocation>> future_receives(std::vector<CellLocation>{}, std::vector<CellLocation>{});
  PerSide<std::vector<CellLocation>::const_iterator> future_receive_begins(future_receives.at(Side::Left).begin(), future_receives.at(Side::Right).begin());
  PerSide<std::vector<CellLocation>::const_iterator> future_receive_ends(future_receives.at(Side::Left).end(), future_receives.at(Side::Right).end());

  PerSide<std::vector<CellLocation>> future_sends(std::vector<CellLocation>{}, std::vector<CellLocation>{});
  PerSide<std::vector<CellLocation>::const_iterator> future_send_begins(future_sends.at(Side::Left).begin(), future_sends.at(Side::Right).begin());
  PerSide<std::vector<CellLocation>::const_iterator> future_send_ends(future_sends.at(Side::Left).end(), future_sends.at(Side::Right).end());

  SimpleStorage storage(d_max);
  TestStrategyFollower<SimpleStorage> follower(&storage);
  Strategy strategy(&follower, future_receive_begins, future_receive_ends, future_send_begins, future_send_ends, d_max);

  REQUIRE(!strategy.is_done());
  REQUIRE(follower.get_num_directly_calculated() == 0);
  strategy.run();
  REQUIRE(strategy.is_done());
  REQUIRE(follower.get_num_directly_calculated() == ((d_max + 1) * (d_max + 1) + (d_max + 1)) / 2);
}

TEST_CASE("RoundRobinPartition - concrete example (green)")
{
  // This tests the partition calculator with the tasks of the green worker from the custom figure in our first (progress) presentation

  RoundRobinPartition partition(3, 1);

  // d 0
  REQUIRE(!partition.has_work());
  REQUIRE(partition.should_send() == PerSide(false, false));
  REQUIRE(partition.should_receive() == PerSide(false, false));
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
  std::vector<PerSide<bool>> should_sends{
      PerSide(false, true),
      PerSide(true, false),
      PerSide(false, false),
      PerSide(false, true),
      PerSide(true, false),
      PerSide(false, false),
      PerSide(false, true),
  };
  std::vector<PerSide<bool>> should_receives{
      PerSide(true, false),
      PerSide(true, false),
      PerSide(false, true),
      PerSide(true, true),
      PerSide(true, false),
      PerSide(false, true),
      PerSide(true, true),
  };
  assert(k_ranges.size() == should_sends.size() && should_sends.size() == should_receives.size());

  for (int i = 0; i < k_ranges.size(); i++)
  {
    REQUIRE(partition.has_work());
    REQUIRE(partition.get_k_range().first == k_ranges.at(i).first);
    REQUIRE(partition.get_k_range().second == k_ranges.at(i).second);
    REQUIRE(partition.should_send() == should_sends.at(i));
    REQUIRE(partition.should_receive() == should_receives.at(i));
    partition.next_d_layer();
  }
}

TEST_CASE("RoundRobinPartition - concrete example (red)")
{
  // This tests the partition calculator with the tasks of the red worker from the custom figure in our first (progress) presentation

  RoundRobinPartition partition(3, 0);

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
  std::vector<PerSide<bool>> should_sends{
      PerSide(false, true),
      PerSide(false, true),
      PerSide(false, false),
      PerSide(false, true),
      PerSide(false, true),
      PerSide(false, false),
      PerSide(false, true),
      PerSide(false, true),
  };
  std::vector<PerSide<bool>> should_receives{
      PerSide(false, false),
      PerSide(false, false),
      PerSide(false, false),
      PerSide(false, true),
      PerSide(false, false),
      PerSide(false, false),
      PerSide(false, true),
      PerSide(false, false),
  };
  assert(k_ranges.size() == should_sends.size() && should_sends.size() == should_receives.size());

  for (int i = 0; i < k_ranges.size(); i++)
  {
    REQUIRE(partition.has_work());
    REQUIRE(partition.get_k_range().first == k_ranges.at(i).first);
    REQUIRE(partition.get_k_range().second == k_ranges.at(i).second);
    REQUIRE(partition.should_send() == should_sends.at(i));
    REQUIRE(partition.should_receive() == should_receives.at(i));
    partition.next_d_layer();
  }
}

TEST_CASE("ReceiveSideIterator - concrete example (green)")
{
  // This test uses tasks of the green worker from the custom figure in our first (progress) presentation

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

  for (Side side : {Side::Left, Side::Right})
  {
    ReceiveSideIterator it(RoundRobinPartition(3, 1), side);
    for (CellLocation expected_receive : future_receives.at(side))
    {
      REQUIRE(*it == expected_receive);
      it++;
    }
  }
}

TEST_CASE("SendSideIterator - concrete example (green)")
{
  // This test uses tasks of the green worker from the custom figure in our first (progress) presentation

  PerSide<std::vector<CellLocation>> future_sends(std::vector<CellLocation>{}, std::vector<CellLocation>{});
  future_sends.at(Side::Left).emplace_back(2, 0);
  future_sends.at(Side::Left).emplace_back(5, -1);
  future_sends.at(Side::Right).emplace_back(1, 1);
  future_sends.at(Side::Right).emplace_back(4, 2);

  for (Side side : {Side::Left, Side::Right})
  {
    SendSideIterator it(RoundRobinPartition(3, 1), side);
    for (CellLocation expected_send : future_sends.at(side))
    {
      REQUIRE(*it == expected_send);
      it++;
    }
  }
}

TEST_CASE("SendSideIterator - concrete example (blue)")
{
  // This test uses tasks of the blue worker from the custom figure in our first (progress) presentation

  RoundRobinPartition partition(3, 2);
  SendSideIterator end_it(partition);

  SendSideIterator left_it(partition, Side::Left);
  assert(left_it != end_it);
  REQUIRE(*left_it == CellLocation(2, 2));
  left_it++;
  REQUIRE(*left_it == CellLocation(3, 3));
  left_it++;
  REQUIRE(*left_it == CellLocation(5, 3));
  left_it++;
  REQUIRE(*left_it == CellLocation(6, 4));
  left_it++;

  SendSideIterator right_it(partition, Side::Right);
  assert(right_it == end_it);
}