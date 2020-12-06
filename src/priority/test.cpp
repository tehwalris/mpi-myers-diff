#include <iostream>
#include <vector>
#include <optional>
#include <cassert>
#include <algorithm>
#include "strategy.hpp"
#include "geometry.hpp"
#include "side.hpp"

template <class S>
class DebugStrategyFollower
{
public:
  DebugStrategyFollower(S *storage) : storage(storage){};

  inline void set(int d, int k, int v)
  {
    int &stored = storage->at(d, k);
    assert(stored == S::undefined);
    stored = v;
  }

  void calculate(int d, int k)
  {
    std::cerr << "calculating d " << d << " k " << k << std::endl;
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
  }

  void send(int d, int k, Side to)
  {
    std::cerr << "sending d " << d << " k " << k << " to " << str_for_side(to) << std::endl;
    get(d, k);
  }

private:
  S *storage;

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

int main()
{
  int d_max = 7;
  PerSide<std::vector<CellLocation>> future_receives(std::vector<CellLocation>{}, std::vector<CellLocation>());
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
  DebugStrategyFollower<SimpleStorage> follower(&storage);
  Strategy strategy(&follower, future_receive_begins, future_receive_ends, d_max);

  strategy.run();
  std::cerr << std::endl;

  strategy.receive(Side::Left, 12);
  strategy.run();
  std::cerr << std::endl;

  strategy.receive(Side::Left, 12);
  strategy.run();
  std::cerr << std::endl;

  strategy.receive(Side::Left, 12);
  strategy.run();
  std::cerr << std::endl;

  strategy.receive(Side::Right, 12);
  strategy.run();
  std::cerr << std::endl;

  // TODO This might try to calculate invalid cells when all receives are done
}