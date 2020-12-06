#include <cassert>
#include "side.hpp"

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