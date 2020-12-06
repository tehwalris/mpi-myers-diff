#include "geometry.hpp"

CellLocation triangle_through_points(CellLocation a, CellLocation b)
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