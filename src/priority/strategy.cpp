#include <iostream>
#include <vector>
#include <optional>
#include <cassert>
#include <algorithm>

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

class CellLocation
{
public:
  int d;
  int k;

  CellLocation() : d(0), k(0){};
  CellLocation(int d, int k) : d(d), k(k){};
};

inline std::ostream &operator<<(std::ostream &os, const CellLocation &loc)
{
  os << "{ d = " << loc.d << ", k = " << loc.k << " }";
  return os;
}

inline bool operator==(const CellLocation &lhs, const CellLocation &rhs)
{
  return lhs.d == rhs.d && lhs.k == rhs.k;
}

// CellDiamond is a pair of top point (inclusive) and bottom point (inclusive)
typedef std::pair<CellLocation, CellLocation> CellDiamond;

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

class Frontier
{
public:
  Frontier(CellLocation left, CellLocation right)
  {
    assert(left.k < right.k);
    covered_triangle_bottoms.push_back(left);
    covered_triangle_bottoms.push_back(right);
  }

  std::optional<CellDiamond> get_next_exposed_diamond(CellLocation query_triangle_bottom)
  {
    if (covered_triangle_bottoms.size() == 1)
    {
      assert(!point_is_on_inside_of_triangle(covered_triangle_bottoms.front(), query_triangle_bottom));
      return std::nullopt;
    }

    assert(covered_triangle_bottoms.size() >= 2);
    assert(!point_is_on_inside_of_triangle(covered_triangle_bottoms.front(), query_triangle_bottom));
    assert(!point_is_on_inside_of_triangle(covered_triangle_bottoms.back(), query_triangle_bottom));

    for (int i = 1; i < covered_triangle_bottoms.size(); i++)
    {
      CellLocation prev_bottom = covered_triangle_bottoms.at(i - 1);
      CellLocation next_bottom = covered_triangle_bottoms.at(i);

      CellLocation exposed_top = intersect_triangles(prev_bottom, next_bottom);
      if (!point_is_on_inside_of_triangle(exposed_top, query_triangle_bottom))
      {
        continue;
      }

      CellLocation exposed_bottom = intersect_triangles(query_triangle_bottom, triangle_through_points(prev_bottom, next_bottom));
      return std::optional{std::make_pair(exposed_top, exposed_bottom)};
    }

    return std::nullopt;
  }

  void cover_triangle(CellLocation triangle_bottom)
  {
    // TODO What if the passed triangle is already inside the frontier?
    auto rem_it = std::remove_if(covered_triangle_bottoms.begin(), covered_triangle_bottoms.end(), [triangle_bottom](CellLocation &existing) {
      return !point_is_outside_of_triangle(existing, triangle_bottom);
    });
    covered_triangle_bottoms.erase(rem_it, covered_triangle_bottoms.end());
    covered_triangle_bottoms.push_back(triangle_bottom);
    std::sort(covered_triangle_bottoms.begin(), covered_triangle_bottoms.end(), [](CellLocation &a, CellLocation &b) { return a.k < b.k; });
  }

private:
  std::vector<CellLocation> covered_triangle_bottoms;

  static bool point_is_on_inside_of_triangle(CellLocation query_point, CellLocation triangle_bottom)
  {
    return query_point.d < triangle_bottom.d && abs(query_point.k - triangle_bottom.k) < (triangle_bottom.d - query_point.d);
  }

  static bool point_is_outside_of_triangle(CellLocation query_point, CellLocation triangle_bottom)
  {
    return query_point.d > triangle_bottom.d || abs(query_point.k - triangle_bottom.k) > (triangle_bottom.d - query_point.d);
  }

  static CellLocation triangle_through_points(CellLocation a, CellLocation b)
  {
    if (a.k > b.k)
    {
      std::swap(a, b);
    }
    int temp = b.k - a.k + a.d - b.d;
    assert(temp >= 0 && temp % 2 == 0);
    temp /= 2;
    return CellLocation(b.d + temp, b.k - temp);
  }

  static CellLocation intersect_triangles(CellLocation bottom_a, CellLocation bottom_b)
  {
    if (bottom_a == bottom_b)
    {
      return bottom_a;
    }
    if (!point_is_outside_of_triangle(bottom_a, bottom_b))
    {
      return bottom_a;
    }
    if (!point_is_outside_of_triangle(bottom_b, bottom_a))
    {
      return bottom_b;
    }
    if (bottom_a.k > bottom_b.k)
    {
      std::swap(bottom_a, bottom_b);
    }
    int temp = bottom_b.k - bottom_a.k + bottom_b.d - bottom_a.d;
    assert(temp > 0 && temp % 2 == 0);
    temp /= 2;
    return CellLocation(bottom_b.d - temp, bottom_b.k - temp);
  }
};

template <class F>
class Strategy
{
public:
  Strategy(F *follower, int d_max) : follower(follower), d_max(d_max), last_receive(never_received, never_received), frontier(CellLocation(d_max, -d_max - 2), CellLocation(d_max, d_max + 2)){};

  void receive(int d, int k, int v, Side from)
  {
    assert(last_receive.at(from) < d);
    last_receive.at(from) = d;
    follower->set(d, k, v);
    frontier.cover_triangle(CellLocation(d, k));
  }

  void run()
  {
    CellLocation target(2 * d_max, 0); // TODO determine dynamically
    while (true)
    {
      std::optional<CellDiamond> exposed_diamond_opt = frontier.get_next_exposed_diamond(target);
      if (!exposed_diamond_opt.has_value())
      {
        break;
      }
      CellDiamond &exposed_diamond = exposed_diamond_opt.value();

      std::cerr << "DEBUG got exposed diamond " << exposed_diamond.first << " " << exposed_diamond.second << std::endl;

      frontier.cover_triangle(exposed_diamond.second);
    }
  }

private:
  PerSide<int> last_receive;
  Frontier frontier;
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
  // strategy.receive(1, 1, 12, Side::Left);
  // strategy.receive(2, 2, 12, Side::Left);
  strategy.run();
}