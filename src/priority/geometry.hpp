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

CellLocation triangle_through_points(CellLocation a, CellLocation b);

CellLocation intersect_triangles(CellLocation bottom_a, CellLocation bottom_b);

// CellDiamond is a pair of top point (inclusive) and bottom point (inclusive)
typedef std::pair<CellLocation, CellLocation> CellDiamond;