#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include <fstream>

const int debug_level = 1;

#ifndef NDEBUG
#define DEBUG(level, x)          \
  if (debug_level >= level)      \
  {                              \
    std::cerr << x << std::endl; \
  }
#define DEBUG_NO_LINE(level, x) \
  if (debug_level >= level)     \
  {                             \
    std::cerr << x;             \
  }
#else
#define DEBUG(level, x)
#define DEBUG_NO_LINE(level, x)
#endif

void print_vector(const std::vector<int> &vec)
{
  for (int i = 0; i < vec.size(); i++)
  {
    if (i != 0)
    {
      DEBUG_NO_LINE(2, " ");
    }
    DEBUG_NO_LINE(2, vec.at(i));
  }
  DEBUG_NO_LINE(2, std::endl);
}

std::vector<int> read_vec_from_file(const std::string path)
{
  std::ifstream file(path);
  if (!file.is_open())
  {
    std::cerr << "Could not open file " << path << std::endl;
    exit(1);
  }

  std::vector<int> output_vec;
  int tmp;
  while (file >> tmp)
  {
    output_vec.push_back(tmp);
  }
  return output_vec;
}
