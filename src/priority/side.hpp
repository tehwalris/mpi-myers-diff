#pragma once

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

Side other_side(Side side)
{
  switch (side)
  {
  case Side::Left:
    return Side::Right;
  case Side::Right:
    return Side::Left;
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

  inline const T &at(Side side) const
  {
    return ((PerSide *)this)->at(side);
  }

  inline bool operator==(const PerSide<T> &other) const
  {
    return left == other.left && right == other.right;
  }

private:
  T left;
  T right;
};

template <class T>
inline std::ostream &operator<<(std::ostream &os, const PerSide<T> &loc)
{
  os << "{ " << str_for_side(Side::Left) << " = " << loc.at(Side::Left) << ", " << str_for_side(Side::Right) << " = " << loc.at(Side::Right) << " }";
  return os;
}