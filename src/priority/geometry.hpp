#pragma once

#include <iostream>
#include <cassert>

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

inline bool point_is_on_inside_of_triangle(CellLocation query_point, CellLocation triangle_bottom)
{
  return query_point.d < triangle_bottom.d && abs(query_point.k - triangle_bottom.k) < (triangle_bottom.d - query_point.d);
}

inline bool point_is_outside_of_triangle(CellLocation query_point, CellLocation triangle_bottom)
{
  return query_point.d > triangle_bottom.d || abs(query_point.k - triangle_bottom.k) > (triangle_bottom.d - query_point.d);
}

// Intersect the diagonal in (+d, +k) direction through a, with the diagonal in (-d, +k) direction through b
CellLocation intersect_diagonals(CellLocation a, CellLocation b)
{
  int temp = b.k - a.k + a.d - b.d;
  assert(temp % 2 == 0);
  temp /= 2;
  return CellLocation(b.d + temp, b.k - temp);
}

CellLocation triangle_through_points(CellLocation a, CellLocation b)
{
  assert(!point_is_on_inside_of_triangle(a, b));
  assert(!point_is_on_inside_of_triangle(b, a));
  if (a.k > b.k)
  {
    std::swap(a, b);
  }
  return intersect_diagonals(a, b);
}

CellLocation intersect_triangles(CellLocation bottom_a, CellLocation bottom_b)
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

// CellDiamond is a pair of top point (inclusive) and bottom point (inclusive)
typedef std::pair<CellLocation, CellLocation> CellDiamond;

bool is_valid_diamond(const CellDiamond &diamond)
{
  return !point_is_outside_of_triangle(diamond.first, diamond.second);
}

// limit_diamond_height returns a diamond with size at most limit in d dimension. The new diamond can be inscribed in the original and has the same top point.
// If there are multiple ways to shrink the diamond, it is shrunk so it is as square as possible.
// If the diamond would be invalid by moving the bottom point in (-d, 0) direction, it will (initially) be moved in (-d, -k) direction.
CellDiamond limit_diamond_height(CellDiamond target, int limit)
{
  assert(is_valid_diamond(target));
  assert(limit >= 1);
  int old_height = target.second.d - target.first.d + 1;
  assert(old_height >= 1);
  if (old_height <= limit)
  {
    return target;
  }

  int shrink_by_total = old_height - limit;
  assert(shrink_by_total > 0);
  int shrink_k_dir = target.first.k - target.second.k;
  if (shrink_k_dir < 0)
  {
    shrink_k_dir = -1;
  }
  else if (shrink_k_dir > 0)
  {
    shrink_k_dir = 1;
  }
  int shrink_by_this_step;
  bool was_square = false;
  if (shrink_k_dir == 0)
  {
    was_square = true;
    shrink_by_this_step = shrink_by_total;
    if (shrink_by_total % 2 == 1)
    {
      shrink_k_dir = -1;
    }
  }
  else
  {
    shrink_by_this_step = std::min(shrink_by_total, abs(target.first.k - target.second.k));
  }

  int new_k = target.second.k + shrink_by_this_step * shrink_k_dir;
  if (was_square)
  {
    new_k = target.second.k;
    if (shrink_by_this_step % 2 == 1)
    {
      new_k += shrink_k_dir;
    }
  }
  CellDiamond result(target.first, CellLocation(target.second.d - shrink_by_this_step, new_k));
  if (shrink_by_this_step < shrink_by_total)
  {
    return limit_diamond_height(result, limit);
  }

  assert(is_valid_diamond(result));
  assert(result.second.d - result.first.d + 1 == limit);
  return result;
}