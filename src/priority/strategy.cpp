#include <iostream>
#include <vector>
#include <optional>
#include <cassert>
#include <algorithm>
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
      exposed_top.d += 2;
      assert(!point_is_outside_of_triangle(exposed_top, query_triangle_bottom));

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
};

template <class F, class IR>
class Strategy
{
public:
  Strategy(
      F *follower,
      PerSide<IR> future_receives,
      PerSide<IR> future_receive_ends,
      int d_max) : follower(follower),
                   future_receives(future_receives),
                   future_receive_ends(future_receive_ends),
                   d_max(d_max),
                   last_receive(never_received, never_received),
                   frontier(CellLocation(d_max, -d_max - 2), CellLocation(d_max, d_max + 2)){};

  void receive(Side from, int v)
  {
    assert(future_receives.at(from) != future_receive_ends.at(from));
    CellLocation loc = *future_receives.at(from);
    future_receives.at(from)++;

    assert(last_receive.at(from) < loc.d);
    last_receive.at(from) = loc.d;

    follower->set(loc.d, loc.k, v);
    frontier.cover_triangle(loc);
  }

  void run()
  {
    PerSide<CellLocation> limiters(CellLocation(d_max + 1, -(d_max + 1)), CellLocation(d_max + 1, d_max + 1));
    for (Side s : {Side::Left, Side::Right})
    {
      if (future_receives.at(s) != future_receive_ends.at(s))
      {
        limiters.at(s) = *future_receives.at(s);
      }
    }
    CellLocation target = triangle_through_points(limiters.at(Side::Left), limiters.at(Side::Right));
    target.d -= 2;

    while (true)
    {
      std::optional<CellDiamond> exposed_diamond_opt = frontier.get_next_exposed_diamond(target);
      if (!exposed_diamond_opt.has_value())
      {
        break;
      }
      CellDiamond &exposed_diamond = exposed_diamond_opt.value();
      std::cerr << "DEBUG got exposed diamond " << exposed_diamond.first << " " << exposed_diamond.second << std::endl;
      calculate_all_in_diamond(exposed_diamond);
      frontier.cover_triangle(exposed_diamond.second);
    }
  }

private:
  PerSide<IR> future_receives;
  PerSide<IR> future_receive_ends;
  PerSide<int> last_receive;
  Frontier frontier;
  F *follower;
  int d_max;
  inline static const int never_received = -1;

  void calculate_all_in_diamond(CellDiamond diamond)
  {
    int last_d = std::min(diamond.second.d, d_max);
    for (int d = diamond.first.d; d <= last_d; d++)
    {
      int k_min = std::max(diamond.first.k - (d - diamond.first.d), diamond.second.k - (diamond.second.d - d));
      int k_max = std::min(diamond.first.k + (d - diamond.first.d), diamond.second.k + (diamond.second.d - d));
      assert(k_max >= k_min && (k_max - k_min) % 2 == 0);
      for (int k = k_min; k <= k_max; k += 2)
      {
        follower->calculate(d, k);
      }
    }
  }
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