#include <iostream>
#include <vector>
#include <cassert>

enum Side
{
  Left = 1,
  Right = 2,
};

const char *str_for_side(Side side)
{
  switch (side)
  {
  case Side::Left:
    return "left";
  case Side::Right:
    return "right";
  default:
    assert(false);
    __builtin_unreachable();
  }
}

template <class T>
class PerSide
{
public:
  PerSide(T left, T right) : left(left), right(right){};

  inline T &at(Side side)
  {
    switch (side)
    {
    case Side::Left:
      return left;
    case Side::Right:
      return right;
    default:
      assert(false);
      __builtin_unreachable();
    }
  }

private:
  T left;
  T right;
};

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
    assert(abs(k) >= d);
    assert((d - abs(k)) % 2 == 0);
    return data.at(d).at(k + d);
  }

private:
  int d_max;
  std::vector<std::vector<int>> data;
};

template <class F>
class Strategy
{
public:
  Strategy(F *follower, int d_max) : follower(follower), d_max(d_max), last_receive(never_received, never_received){};

  void receive(int d, int k, int v, Side from)
  {
    assert(last_receive.at(from) < d);
    last_receive.at(from) = d;
    follower->set(d, k, v);
  }

  void run()
  {
  }

private:
  PerSide<int> last_receive;
  F *follower;
  int d_max;
  inline static const int never_received = -1;
};

int main()
{
  int d_max = 10;
  SimpleStorage storage(d_max);
  DebugStrategyFollower<SimpleStorage> follower(&storage);
  Strategy<DebugStrategyFollower<SimpleStorage>> strategy(&follower, d_max);

  strategy.receive(0, 0, 12, Side::Left);
}