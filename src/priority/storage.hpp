#pragma once

#include <vector>
#include <cassert>
#include <cmath>

class SimpleStorage
{
public:
  inline static const int undefined = -1;

  SimpleStorage(int d_max) : d_max(d_max)
  {
    for (int i = 0; i <= d_max; i++)
    {
      data.emplace_back(2 * i + 1, undefined);
    }
  }

  inline int &at(int d, int k)
  {
    assert(d >= 0 && d <= d_max);
    assert(abs(k) <= d);
    assert((d - abs(k)) % 2 == 0);
    return data.at(d).at(k + d);
  }

private:
  int d_max;
  std::vector<std::vector<int>> data;
};

class FastStorage
{
public:
  FastStorage(int d_max) : d_max(d_max)
  {
    // allocate initial block 0
    num_blocks = std::ceil(std::max(1.0, d_max / (double)alloc_n_layers));
    data_pointers.resize(num_blocks, nullptr);
    data_pointers[0] = allocate_block(0);
  }

  int &at(int d, int k)
  {
    int block_idx = d / alloc_n_layers;

    // allocate new block if needed
    if (data_pointers.at(block_idx) == nullptr)
    {
      data_pointers.at(block_idx) = allocate_block(block_idx * alloc_n_layers);
    }

    int start_d = pyramid_size(d) - pyramid_size(block_idx * alloc_n_layers);
    int offset = (k + d) / 2;

    assert(d < d_max);
    assert(offset >= 0 && offset <= d + 1);

    return data_pointers[block_idx][start_d + offset];
  }

private:
  int d_max;
  int num_blocks;
  int alloc_n_layers = 20;
  std::vector<int *> data_pointers;

  // allocate the data block that begins at layer index d_begin
  int *allocate_block(int d_begin)
  {
    int d_max = d_begin + alloc_n_layers;
    int size = pyramid_size(d_max) - pyramid_size(d_begin);
    return new int[size]; // not initialized to 0
  }

  // total number of elements in a pyramid with l layers
  inline int pyramid_size(int l)
  {
    return (l * (l + 1)) / 2;
  }
};