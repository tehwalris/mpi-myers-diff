#pragma once

#include "geometry.hpp"
#include "side.hpp"

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
                   frontier(CellLocation(d_max, -d_max - 2), CellLocation(d_max, d_max + 2)),
                   final_known_limiters(CellLocation(d_max + 1, -(d_max + 1)), CellLocation(d_max + 1, d_max + 1)),
                   done(false)
  {
    assert(d_max >= 0);
  };

  void receive(Side from, int v)
  {
    assert(future_receives.at(from) != future_receive_ends.at(from));
    CellLocation loc = *future_receives.at(from);
    future_receives.at(from)++;

    final_known_limiters.at(from) = CellLocation(loc.d + 2, loc.k);

    follower->set(loc.d, loc.k, v);
    frontier.cover_triangle(loc);
  }

  void run()
  {
    PerSide<CellLocation> limiters = final_known_limiters;
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

      assert(!done);
      calculate_all_in_diamond(exposed_diamond);
      frontier.cover_triangle(exposed_diamond.second);
    }

    if (future_receives.at(Side::Left) == future_receive_ends.at(Side::Left) && future_receives.at(Side::Right) == future_receive_ends.at(Side::Right))
    {
      done = true;
    }
  }

  bool is_done()
  {
    return done;
  }

private:
  PerSide<IR> future_receives;
  PerSide<IR> future_receive_ends;
  PerSide<CellLocation> final_known_limiters;
  Frontier frontier;
  F *follower;
  int d_max;
  inline static const int never_received = -1;
  bool done;

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