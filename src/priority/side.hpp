#include <cassert>

enum Side
{
  Left = 1,
  Right = 2,
};

const char *str_for_side(Side side);

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