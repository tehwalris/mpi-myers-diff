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

  inline int *get_raw_row(int d)
  {
    return &at(d, 0);
  }

private:
  inline static const int undefined = -1;
  int d_max;
  std::vector<std::vector<int>> data;

  inline int &at(int d, int k)
  {
    assert(d >= 0 && d <= d_max && abs(k) <= d);
    assert((d - abs(k)) % 2 == 0);
    return data.at(d).at(k + d);
  }
};

template <typename BlockInitializer>
class _FastStorageBase : BlockInitializer
{
public:
  _FastStorageBase(int d_max) : d_max(d_max)
  {
    num_blocks = std::ceil(std::max(1.0, (d_max + 1) / (double)alloc_n_layers));
    data_pointers.resize(num_blocks, nullptr);
  }

  inline int get(int d, int k)
  {
    int v = at(d, k);
    assert(v >= 0);
    return v;
  }

  inline void set(int d, int k, int v)
  {
    assert(v >= 0);
    at(d, k) = v;
  }

  inline int *get_raw_row(int d)
  {
    assert(d >= 0 && d <= d_max);
    return &at(d, 0);
  }

protected:
  inline int &at(int d, int k)
  {
    assert(d >= 0 && d <= d_max && abs(k) <= d);

    int block_idx = d / alloc_n_layers;

    // allocate new block if needed
    if (data_pointers.at(block_idx) == nullptr)
    {
      data_pointers.at(block_idx) = allocate_block(block_idx * alloc_n_layers);
    }

    int start_d = 2 * (pyramid_size(d) - pyramid_size(block_idx * alloc_n_layers));
    int offset = k + d;

    assert(d <= d_max);
    assert(offset >= 0 && offset <= 2 * d + 1);

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
    int size = 2 * (pyramid_size(d_max) - pyramid_size(d_begin));
    int *block = new int[size]; // not initialized to 0
    BlockInitializer::initialize_block(block, size);
    return block;
  }

  // total number of elements in a pyramid with l layers
  inline int pyramid_size(int l)
  {
    return (l * (l + 1)) / 2;
  }
};

class _NoopBlockInitializer
{
protected:
  static void initialize_block(int *block, int size) {}
};

class _MinusOneBlockInitializer
{
protected:
  static void initialize_block(int *block, int size)
  {
    std::fill(block, block + size, -1);
  }
};

class FastStorageWithHasValue : public _FastStorageBase<_MinusOneBlockInitializer>
{
public:
  FastStorageWithHasValue(int d_max) : _FastStorageBase(d_max) {}

  inline bool has_value(int d, int k)
  {
    int v = _FastStorageBase::at(d, k);
    assert(v >= -1);
    return v != -1;
  }
};

#ifndef NDEBUG
typedef _FastStorageBase<_MinusOneBlockInitializer> FastStorage;
#else
typedef _FastStorageBase<_NoopBlockInitializer> FastStorage;
#endif

class _FrontierStorageBase
{
public:
  _FrontierStorageBase(int d_max) : d_max(d_max), values_by_column(2 * d_max + 1) {}

  inline int get(int d, int k)
  {
    return values_by_column.at(i_from_d_k(d, k));
  }

  inline void set(int d, int k, int v)
  {
    values_by_column.at(i_from_d_k(d, k)) = v;
  }

  inline int *get_raw_row(int d)
  {
    assert(d >= 0 && d <= d_max);
    return values_by_column.data() + d_max;
  }

protected:
  inline int i_from_d_k(int d, int k)
  {
    assert(d >= 0 && d <= d_max && abs(k) <= d);
    return d_max + k;
  }

private:
  int d_max;
  std::vector<int> values_by_column;
};

class FrontierStorageWithHasValue : public _FrontierStorageBase
{
public:
  FrontierStorageWithHasValue(int d_max) : _FrontierStorageBase(d_max), last_ds_by_column(2 * d_max + 1, column_never_used) {}

  inline int get(int d, int k)
  {
    assert(last_ds_by_column.at(_FrontierStorageBase::i_from_d_k(d, k)) == d);
    return _FrontierStorageBase::get(d, k);
  }

  inline void set(int d, int k, int v)
  {
    int &last_d = last_ds_by_column.at(_FrontierStorageBase::i_from_d_k(d, k));
    assert(last_d == column_never_used || last_d < d);
    last_d = d;
    _FrontierStorageBase::set(d, k, v);
  }

  inline bool has_value(int d, int k)
  {
    int last_d = last_ds_by_column.at(_FrontierStorageBase::i_from_d_k(d, k));
    return last_d == d;
  }

private:
  std::vector<int> last_ds_by_column;
  inline static const int column_never_used = -1;
};

#ifndef NDEBUG
typedef FrontierStorageWithHasValue FrontierStorage;
#else
typedef _FrontierStorageBase FrontierStorage;
#endif