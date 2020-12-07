#pragma once

#include <vector>
#include <cassert>
#include <cmath>

class SimpleStorage
{
public:
  SimpleStorage(int d_max) : d_max(d_max)
  {
    for (int i = 0; i <= d_max; i++)
    {
      data.emplace_back(2 * i + 1, undefined);
    }
  }

  inline int get(int d, int k)
  {
    int v = at(d, k);
    assert(v != undefined);
    return v;
  }

  inline void set(int d, int k, int v)
  {
    assert(v >= 0);
    int &stored_v = at(d, k);
    assert(stored_v == undefined);
    stored_v = v;
  }

private:
  inline static const int undefined = -1;
  int d_max;
  std::vector<std::vector<int>> data;

  inline int &at(int d, int k)
  {
    assert(d >= 0 && d <= d_max);
    assert(abs(k) <= d);
    assert((d - abs(k)) % 2 == 0);
    return data.at(d).at(k + d);
  }
};

class FastStorage
{
public:
  FastStorage(int d_max) : d_max(d_max)
  {
    // allocate initial block 0
    num_blocks = std::ceil(std::max(1.0, (d_max + 1) / (double)alloc_n_layers));
    data_pointers.resize(num_blocks, nullptr);
    data_pointers[0] = allocate_block(0);
  }

  inline int get(int d, int k)
  {
    return at(d, k);
  }

  inline void set(int d, int k, int v)
  {
    at(d, k) = v;
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

  inline int &at(int d, int k)
  {
    int block_idx = d / alloc_n_layers;

    // allocate new block if needed
    if (data_pointers.at(block_idx) == nullptr)
    {
      data_pointers.at(block_idx) = allocate_block(block_idx * alloc_n_layers);
    }

    int start_d = pyramid_size(d) - pyramid_size(block_idx * alloc_n_layers);
    int offset = (k + d) / 2;

    assert(d <= d_max);
    assert(offset >= 0 && offset <= d + 1);

    return data_pointers[block_idx][start_d + offset];
  }
};

class FrontierStorage
{
public:
  FrontierStorage(int d_max) : d_max(d_max), values_by_column(2 * d_max + 1)
  {
#ifndef NDEBUG
    last_ds_by_column = std::vector<int>(2 * d_max + 1, column_never_used);
#endif
  }

  inline int get(int d, int k)
  {
    assert(d >= 0 && d <= d_max && abs(k) <= d);
    int i = d_max + k;

#ifndef NDEBUG
    assert(last_ds_by_column.at(i) == d);
#endif

    return values_by_column.at(i);
  }

  inline void set(int d, int k, int v)
  {
    assert(d >= 0 && d <= d_max && abs(k) <= d);
    int i = d_max + k;

#ifndef NDEBUG
    int last_d = last_ds_by_column.at(i);
    assert(last_d == column_never_used || last_d < d);
    last_ds_by_column.at(i) = d;
#endif

    values_by_column.at(i) = v;
  }

private:
  int d_max;
  std::vector<int> values_by_column;
#ifndef NDEBUG
  std::vector<int> last_ds_by_column;
  inline static const int column_never_used = -1;
#endif
};