#pragma once

#include <vector>
#include <algorithm>
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

    int best_index = -1, lowest_abs_k = 0;
    for (int i = 1; i < covered_triangle_bottoms.size(); i++)
    {
      CellLocation prev_bottom = covered_triangle_bottoms.at(i - 1);
      CellLocation next_bottom = covered_triangle_bottoms.at(i);

      CellLocation exposed_top = intersect_triangles(prev_bottom, next_bottom);
      if (point_is_on_inside_of_triangle(exposed_top, query_triangle_bottom) && (best_index == -1 || abs(exposed_top.k) < lowest_abs_k))
      {
        best_index = i;
        lowest_abs_k = abs(exposed_top.k);
      }
    }

    if (best_index == -1)
    {
      return std::nullopt;
    }

    CellLocation prev_bottom = covered_triangle_bottoms.at(best_index - 1);
    CellLocation next_bottom = covered_triangle_bottoms.at(best_index);
    CellLocation exposed_top = intersect_triangles(prev_bottom, next_bottom);
    exposed_top.d += 2;
    assert(!point_is_outside_of_triangle(exposed_top, query_triangle_bottom));
    CellLocation exposed_bottom = intersect_triangles(query_triangle_bottom, triangle_through_points(prev_bottom, next_bottom));
    return std::optional{std::make_pair(exposed_top, exposed_bottom)};
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

template <class F, class IR, class IS>
class Strategy
{
public:
  static const int no_diamond_height_limit = -1;

  Strategy(
      F *follower,
      PerSide<IR> future_receives,
      PerSide<IR> future_receive_ends,
      PerSide<IS> future_sends,
      PerSide<IS> future_send_ends,
      int d_max,
      int diamond_height_limit) : follower(follower),
                                  future_receives(future_receives),
                                  future_receive_ends(future_receive_ends),
                                  future_sends(future_sends),
                                  future_send_ends(future_send_ends),
                                  d_max(d_max),
                                  diamond_height_limit(diamond_height_limit),
                                  frontier(CellLocation(d_max, -d_max - 2), CellLocation(d_max, d_max + 2)),
                                  final_known_limiters(CellLocation(d_max + 1, -(d_max + 1)), CellLocation(d_max + 1, d_max + 1))
  {
    assert(d_max >= 0);
    assert(diamond_height_limit == no_diamond_height_limit || diamond_height_limit > 0);
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
    assert(!done);

    PerSide<CellLocation> limiters = final_known_limiters;
    bool limited_by_receives = false;
    for (Side s : {Side::Left, Side::Right})
    {
      if (future_receives.at(s) == future_receive_ends.at(s))
      {
        continue;
      }
      limiters.at(s) = *future_receives.at(s);
      if (limiters.at(s).d < d_max)
      {
        limited_by_receives = true;
      }
    }

    CellLocation target = intersect_diagonals(limiters.at(Side::Left), limiters.at(Side::Right));
    target.d -= 2;

    bool limited_by_sends = false;
    CellLocation target_from_send_limit(std::numeric_limits<int>::max(), 0);
    for (Side s : {Side::Left, Side::Right})
    {
      if (future_sends.at(s) == future_send_ends.at(s))
      {
        continue;
      }
      CellLocation send_loc = *future_sends.at(s);
      if (send_loc.d < d_max && !point_is_outside_of_triangle(send_loc, target))
      {
        // If we can send both left and right, we have to choose which one we prioritize.
        // Unblock the worker that has made less progress. If both have made equal progress, make a "pseudorandom" choice (parity of d).
        if (send_loc.d < target_from_send_limit.d || (send_loc.d == target_from_send_limit.d && send_loc.d % 2 == 0))
        {
          target_from_send_limit = send_loc;
          limited_by_sends = true;
        }
      }
    }
    if (limited_by_sends)
    {
      target = target_from_send_limit;
    }

    std::optional<CellDiamond> exposed_diamond_opt = frontier.get_next_exposed_diamond(target);
    if (exposed_diamond_opt.has_value())
    {
      CellDiamond &exposed_diamond = exposed_diamond_opt.value();
      if (!limited_by_sends && diamond_height_limit != no_diamond_height_limit)
      {
        exposed_diamond = limit_diamond_height(exposed_diamond, diamond_height_limit);
      }

      calculate_all_in_diamond(exposed_diamond);
      if (done)
      {
        return;
      }
      frontier.cover_triangle(exposed_diamond.second);
    }

    for (Side s : {Side::Left, Side::Right})
    {
      while (future_sends.at(s) != future_send_ends.at(s))
      {
        CellLocation send_loc = *future_sends.at(s);
        if (send_loc.d >= d_max || point_is_outside_of_triangle(send_loc, target))
        {
          break;
        }
        follower->send(send_loc.d, send_loc.k, s);
        future_sends.at(s)++;
      }
    }

    if (!exposed_diamond_opt.has_value() && !limited_by_receives && !limited_by_sends)
    {
      done = true;
    }
    blocked_waiting_for_receive = !exposed_diamond_opt.has_value() && limited_by_receives && !limited_by_sends;
  }

  bool is_done()
  {
    return done;
  }

  bool is_blocked_waiting_for_receive()
  {
    return !done && blocked_waiting_for_receive;
  }

  std::optional<CellLocation> get_final_result_location()
  {
    return final_result_location;
  }

private:
  PerSide<IR> future_receives;
  PerSide<IR> future_receive_ends;
  PerSide<IS> future_sends;
  PerSide<IS> future_send_ends;
  PerSide<CellLocation> final_known_limiters;
  Frontier frontier;
  F *follower;
  int d_max;
  int diamond_height_limit;
  inline static const int never_received = -1;
  bool done = false;
  bool blocked_waiting_for_receive = false;
  std::optional<CellLocation> final_result_location = std::nullopt;

  void calculate_all_in_diamond(CellDiamond diamond)
  {
    assert(is_valid_diamond(diamond));
    int d_local_max = std::min(diamond.second.d, d_max);
    for (int d = diamond.first.d; d <= d_local_max; d++)
    {
      int k_min = std::max(diamond.first.k - (d - diamond.first.d), diamond.second.k - (diamond.second.d - d));
      int k_max = std::min(diamond.first.k + (d - diamond.first.d), diamond.second.k + (diamond.second.d - d));
      assert(k_max >= k_min && (k_max - k_min) % 2 == 0);
      for (int k = k_min; k <= k_max; k += 2)
      {
        bool just_found_final_result = follower->calculate(d, k);
        if (just_found_final_result)
        {
          assert(!final_result_location.has_value());
          final_result_location = std::optional{CellLocation(d, k)};
          done = true;
          return;
        }
      }
    }
  }
};