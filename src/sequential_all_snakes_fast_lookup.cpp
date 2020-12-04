#include "snakes_fast_lookup.hpp"
#include <iostream>
#include <fstream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>                   // chrono::high_resolution_clock

// all_snakes_fast_lookup_2_vec_eq version


// Uncomment this line when performance is measured
//#define NDEBUG

const int debug_level = 0;

#ifndef NDEBUG
#define DEBUG(level, x)          \
  if (debug_level >= level)      \
  {                              \
    std::cerr << x << std::endl; \
  }
#define DEBUG_NO_LINE(level, x)  \
  if (debug_level >= level)      \
  {                              \
    std::cerr << x; \
  }
#else
#define DEBUG(level, x)
#define DEBUG_NO_LINE(level, x)
#endif

const int shutdown_sentinel = -1;
const int unknown_len = -1;
const int no_worker_rank = 0;


class Results{
private:
    int alloc_n_layers = 20;
    std::vector<int*> data_pointers;

    // allocate the data block that begins at layer index d_begin
    int* allocate_block(int d_begin){
      int d_max = d_begin + alloc_n_layers;
      int size = pyramid_size(d_max) - pyramid_size(d_begin);
      DEBUG(3, "PYRAMID: allocate new blocks for layer "<<d_begin<<" to "<<d_max-1<<" of size "<< size);
      
      return new int[size](); // initialized to 0
    }

    // total number of elements in a pyramid with l layers
    inline int pyramid_size(int l){
      return (l*(l+1))/2;
    }

public:
    int m_d_max;
    int num_blocks;

    Results(int d_max){
        this->m_d_max = d_max;

        // allocate initial block 0
        this->num_blocks = std::ceil(std::max(1.0, d_max / (double) alloc_n_layers));
        data_pointers.resize(num_blocks, nullptr);
        data_pointers[0] = allocate_block(0); 
    }

    int &result_at(int d, int k){
        DEBUG(3, "PYRAMID: ("<<d<<", "<<k<<")");
        int block_idx = d / alloc_n_layers;

        // allocate new block if needed
        if (data_pointers.at(block_idx) == nullptr){
          data_pointers.at(block_idx) = allocate_block(block_idx*alloc_n_layers);
        }

        int start_d = pyramid_size(d) - pyramid_size(block_idx*alloc_n_layers);
        int offset = (k+d)/2;

        assert(d < this->m_d_max);
        assert(offset >= 0 && offset <= d+1);

        return data_pointers[block_idx][start_d+offset];
    }

    // returns pointer to first that element.
    // is guaranteed to continue for at least end of layer d.
    // unlike result_at(d,k) does not perform allocation of next layers
    int* get_pointer(int d, int k){
        int block_idx = d / alloc_n_layers;
        int start_d = pyramid_size(d) - pyramid_size(block_idx*alloc_n_layers);
        int offset = (k+d) / 2;

        return data_pointers[block_idx] + start_d + offset;
    }

};

struct Edit_step{
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
        if (argc >= 4) {
            edit_script_path = argv[3];
            edit_script_to_file = true;
        }
    }

    // Init Timers
    std::chrono::_V2::system_clock::time_point t_in_start, t_in_end, t_pre_start, t_pre_end, t_sol_start, t_sol_end, t_script_start, t_script_end;


    // Input Timer
    t_in_start = std::chrono::high_resolution_clock::now();

    std::vector<int> in_1, in_2;
    read_file(path_1, in_1);
    read_file(path_2, in_2);

    t_in_end = std::chrono::high_resolution_clock::now();

    DEBUG(2, "in_1.size(): " << in_1.size());
    DEBUG(2, "in_2.size(): " << in_2.size());

    // Precomputation Timer
    t_pre_start = std::chrono::high_resolution_clock::now();

    int d_max = in_1.size() + in_2.size() + 1;

    int k_min = -(std::min((size_t) d_max, in_2.size()) - 1);
    int k_max = std::min((size_t) d_max, in_1.size()) - 1;
    Snakes snakes = compute_all_snakes_seq(in_1, in_2, k_min, k_max);

    t_pre_end = std::chrono::high_resolution_clock::now();

    t_sol_start = std::chrono::high_resolution_clock::now();
    int edit_len = unknown_len;
    Results results(d_max);     //TODO: k_min, k_max

    for (int d = 0; d < d_max; d++)
    {
        DEBUG(2, "calculating layer " << d);

        for (int k = -d; k <= d; k += 2)            //TODO: k_min, k_max
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

            if (x < in_1.size() && y < in_2.size() && in_1.at(x) == in_2.at(y)) {
                x = snakes.get_end_of_snake(k, x);
                y = x - k;
            }

            DEBUG(2, "x: " << x);
            DEBUG(2, "y: " << y);
            results.result_at(d, k) = x;

            if (x >= in_1.size() && y >= in_2.size())
            {
                DEBUG(2, "found lcs");
                edit_len = d;
                t_sol_end = std::chrono::high_resolution_clock::now();

                goto done;
            }
        }
    }

done:
    // Edit Script Timer
    t_script_start = std::chrono::high_resolution_clock::now();

    std::vector<struct Edit_step> steps(edit_len);
    int k = in_1.size() - in_2.size();
    for(int d = edit_len; d > 0; d--){
        if (k == -d || k != d && results.result_at(d - 1, k - 1) < results.result_at(d - 1, k + 1))
        {
            k = k + 1;
            int x = results.result_at(d - 1, k);
            int y = x - k;
            int val = in_2.at(y);
            DEBUG(2, "y: " << y << " in_2: " << val);
            steps[d-1] = {x, val, true};
        } else {
            k = k - 1;
            int x = results.result_at(d - 1, k) + 1;
            steps[d-1] = {x, -1, false};
        }
    }

    std::ofstream edit_script_file;
    auto cout_buf = std::cout.rdbuf();
    if (edit_script_to_file) {
        edit_script_file.open(edit_script_path);
        if (!edit_script_file.is_open())
        {
            std::cerr << "Could not open edit script file " << edit_script_path << std::endl;
            exit(1);
        }

        std::cout.rdbuf(edit_script_file.rdbuf()); //redirect std::cout to file
    }

    for(int i=0; i < steps.size(); i++){
        struct Edit_step step = steps.at(i);
        if(step.mode){
            std::cout << step.x << " + " << step.insert_val << std::endl;
        } else  {
            std::cout << step.x << " -" << std::endl;
        }
    }

    if (edit_script_to_file) {
        edit_script_file.close();
    }


    t_script_end = std::chrono::high_resolution_clock::now();

    std::cout.rdbuf(cout_buf);
    std::cout << "\nmin edit length " << edit_len << std::endl << std::endl;
    std::cout << "Read Input [μs]: \t" << std::chrono::duration_cast<std::chrono::microseconds>(t_in_end - t_in_start).count() << std::endl;
    std::cout << "Precompute [μs]: \t" << std::chrono::duration_cast<std::chrono::microseconds>(t_pre_end - t_pre_start).count() << std::endl;
    std::cout << "Solution [μs]:   \t" << std::chrono::duration_cast<std::chrono::microseconds>(t_sol_end - t_sol_start).count() << std::endl;
    std::cout << "Edit Script [μs]: \t" << std::chrono::duration_cast<std::chrono::microseconds>(t_script_end - t_script_start).count() << std::endl;
    
    
    return 0;
}
