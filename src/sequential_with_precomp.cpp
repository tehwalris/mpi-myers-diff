#include "common.hpp"
#include <iostream>
#include <fstream>
#include <assert.h>
#include <algorithm>
#include <chrono>   // chrono::high_resolution_clock
#include <optional> // requires C++17
#include <cmath>
#include "snake_computation.hpp"

// Uncomment this line when performance is measured
//#define NDEBUG

const int debug_level = 0;

#ifndef NDEBUG
#define DEBUG(level, x)              \
    if (debug_level >= level)        \
    {                                \
        std::cerr << x << std::endl; \
    }
#define DEBUG_NO_LINE(level, x) \
    if (debug_level >= level)   \
    {                           \
        std::cerr << x;         \
    }
#else
#define DEBUG(level, x)
#define DEBUG_NO_LINE(level, x)
#endif

const int shutdown_sentinel = -1;
const int unknown_len = -1;
const int no_worker_rank = 0;

class Results
{
private:
    int alloc_n_layers = 20;
    std::vector<int *> data_pointers;

    // allocate the data block that begins at layer index d_begin
    int *allocate_block(int d_begin)
    {
        int d_max = d_begin + alloc_n_layers;
        int size = pyramid_size(d_max) - pyramid_size(d_begin);
        DEBUG(3, "PYRAMID: allocate new blocks for layer " << d_begin << " to " << d_max - 1 << " of size " << size);

        return new int[size](); // initialized to 0
    }

    // total number of elements in a pyramid with l layers
    inline int pyramid_size(int l)
    {
        return (l * (l + 1)) / 2;
    }

public:
    int m_d_max;
    int num_blocks;

    Results(int d_max)
    {
        this->m_d_max = d_max;

        // allocate initial block 0
        this->num_blocks = std::ceil(std::max(1.0, d_max / (double)alloc_n_layers));
        data_pointers.resize(num_blocks, nullptr);
        data_pointers[0] = allocate_block(0);
    }

    int &result_at(int d, int k)
    {
        DEBUG(3, "PYRAMID: (" << d << ", " << k << ")");
        int block_idx = d / alloc_n_layers;

        // allocate new block if needed
        if (data_pointers.at(block_idx) == nullptr)
        {
            data_pointers.at(block_idx) = allocate_block(block_idx * alloc_n_layers);
        }

        int start_d = pyramid_size(d) - pyramid_size(block_idx * alloc_n_layers);
        int offset = (k + d) / 2;

        assert(d < this->m_d_max);
        assert(offset >= 0 && offset <= d + 1);

        return data_pointers[block_idx][start_d + offset];
    }

    // returns pointer to first that element.
    // is guaranteed to continue for at least end of layer d.
    // unlike result_at(d,k) does not perform allocation of next layers
    int *get_pointer(int d, int k)
    {
        int block_idx = d / alloc_n_layers;
        int start_d = pyramid_size(d) - pyramid_size(block_idx * alloc_n_layers);
        int offset = (k + d) / 2;

        return data_pointers[block_idx] + start_d + offset;
    }
};

struct Edit_step
{
    /** Position at which to perform the edit step */
    int x;
    /** Value to insert. This value is ignored when in delete mode */
    int insert_val;
    /** Mode of this edit step. True means addition, false deletion */
    bool mode;
};

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

void read_file(const std::string path, std::vector<int> &output_vec)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Could not open file " << path << std::endl;
        exit(1);
    }

    int tmp;
    while (file >> tmp)
    {
        output_vec.push_back(tmp);
    }
}

int main(int argc, char *argv[])
{
    std::ios_base::sync_with_stdio(false);

    std::string path_1, path_2, edit_script_path;
    bool edit_script_to_file = false;

    if (argc < 3)
    {
        std::cerr << "You must provide two paths to files to be compared as arguments" << std::endl;
        exit(1);
    }
    else
    {
        path_1 = argv[1];
        path_2 = argv[2];
        if (argc >= 4)
        {
            edit_script_path = argv[3];
            edit_script_to_file = true;
        }
    }

    // start TIMER
    auto chrono_start = std::chrono::high_resolution_clock::now();

    std::vector<int> in_1, in_2;
    read_file(path_1, in_1);
    read_file(path_2, in_2);

    DEBUG(2, "in_1.size(): " << in_1.size());
    DEBUG(2, "in_2.size(): " << in_2.size());

    size_t real_in_1_size = in_1.size();
    size_t real_in_2_size = in_2.size();

    std::vector<bool> deletions(in_1.size());
    std::vector<std::pair<size_t, int>> trivial_insertions;

    std::vector<size_t> real_indices_1;
    std::vector<size_t> real_indices_2;
    discard_lines_without_match_seq(in_1, in_2, deletions, trivial_insertions, real_indices_1, real_indices_2);

    DEBUG(2, "Remaining in in_1 after discard: " << in_1.size());
    DEBUG(2, "Remaining in in_2 after discard: " << in_2.size());

    int trivial_edits = (real_in_1_size - in_1.size()) + (real_in_2_size - in_2.size());

    int d_max = in_1.size() + in_2.size() + 1;

    int edit_len = unknown_len;
    Results results(d_max);

    for (int d = 0; d < d_max; d++)
    {
        DEBUG(2, "calculating layer " << d);

        for (int k = -d; k <= d; k += 2)
        {
            DEBUG(2, "d " << d << " k " << k);

            int x;
            if (d == 0)
            {
                x = 0;
            }
            else if (k == -d || k != d && results.result_at(d - 1, k - 1) < results.result_at(d - 1, k + 1))
            {
                x = results.result_at(d - 1, k + 1);
            }
            else
            {
                x = results.result_at(d - 1, k - 1) + 1;
            }

            int y = x - k;

            compute_end_of_snake(x, y, in_1, in_2);

            DEBUG(2, "x: " << x);
            DEBUG(2, "y; " << y);
            results.result_at(d, k) = x;

            if (x >= in_1.size() && y >= in_2.size())
            {
                DEBUG(2, "found lcs");
                edit_len = d;

                // stop TIMER
                auto chrono_end = std::chrono::high_resolution_clock::now();
                auto chrono_t = std::chrono::duration_cast<std::chrono::microseconds>(chrono_end - chrono_start).count();
                std::cout << "Solution [μs]: \t" << chrono_t << std::endl
                          << std::endl;

                goto done;
            }
        }
    }

done:
    int actual_edit_len = edit_len == unknown_len ? unknown_len : (edit_len + trivial_edits);
    std::cout << "min edit length " << actual_edit_len << std::endl;
    std::cout << "mpi comm_world: " << 1 << std::endl;


    std::vector<std::pair<size_t, int>> lcs_insertions;

    int k = in_1.size() - in_2.size();
    for (int d = edit_len; d > 0; d--)
    {
        if (k == -d || k != d && results.result_at(d - 1, k - 1) < results.result_at(d - 1, k + 1))
        {
            k = k + 1;
            int x = results.result_at(d - 1, k);
            int y = x - k;
            int val = in_2.at(y);
            size_t real_y = real_indices_2.at(y);
            lcs_insertions.push_back(std::make_pair(real_y, val));
            DEBUG(2, "real_y: " << real_y << " in_2: " << val);
        }
        else
        {
            k = k - 1;
            int x = results.result_at(d - 1, k);
            deletions.at(real_indices_1.at(x)) = true;
        }
    }

    std::ofstream edit_script_file;
    if (edit_script_to_file)
    {
        edit_script_file.open(edit_script_path);
        if (!edit_script_file.is_open())
        {
            std::cerr << "Could not open edit script file " << edit_script_path << std::endl;
            exit(1);
        }

        std::cout.rdbuf(edit_script_file.rdbuf()); //redirect std::cout to file
    }

    size_t x = 0, y = 0;
    size_t trivial_idx = 0;
    int lcs_idx = lcs_insertions.size() - 1; // lcs_insertions are ordered in reverse

    auto get_next_insertion = [&trivial_insertions, &lcs_insertions, &trivial_idx, &lcs_idx]() -> std::optional<std::pair<size_t, int>> {
        if (trivial_idx >= trivial_insertions.size())
        { // no more trivial insertions
            if (lcs_idx >= 0)
                return lcs_insertions[lcs_idx--];
            else // reached the end of both insertion queues
                return std::nullopt;
        }
        else
        {
            if (lcs_idx < 0) // no more lcs insertions
                return trivial_insertions[trivial_idx++];
            else
            {
                // select insertion with smaller y index
                if (trivial_insertions[trivial_idx].first < lcs_insertions[lcs_idx].first)
                    return trivial_insertions[trivial_idx++];
                else
                    return lcs_insertions[lcs_idx--];
            }
        }
    };

    std::optional<std::pair<size_t, int>> next_insertion = get_next_insertion();
    while (x < real_in_1_size || y < real_in_2_size)
    {
        if (!deletions[x] && (!next_insertion.has_value() || next_insertion->first != y))
        {
            // no change
            x++;
            y++;
        }
        else
        {
            // Insertions
            while (next_insertion.has_value() && next_insertion->first == y)
            {
                std::cout << x << " + " << next_insertion->second << std::endl; // after x -> no need to adjust x
                y++;
                next_insertion = get_next_insertion();
            }
            // Deletions
            while (deletions[x])
            {                                            // insertions cannot happen, since y isn't incremented
                std::cout << x + 1 << " -" << std::endl; // 1-based indices in output
                x++;
            }
        }
    }

    if (edit_script_to_file)
    {
        edit_script_file.close();
    }

    return 0;
}
