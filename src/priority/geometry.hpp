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