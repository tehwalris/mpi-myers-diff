#include "snakes_compact.hpp"
#include <iostream>
#include <fstream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <chrono>                   // chrono::high_resolution_clock

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

struct Results{

    std::vector<int> m_data;
    int m_d_max;

    Results(int d_max){
        m_d_max = d_max;
        int size = (d_max*d_max+3*d_max+2)/2;
        m_data = std::vector<int>(size);
    }

    int &result_at(int d, int k){
        assert(d < m_d_max);
        int start = (d*(d+1))/2;
        int access = (k+d)/2;
        DEBUG(3, "PYRAMID: d_max: " << m_d_max << " d:" << d << " k:" << k << " start:" << start << " access:" << access);
        assert(access >= 0 && access <= d+1);
        assert(start+access < m_data.size());

        return m_data.at(start+access);
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

    // start TIMER
    auto chrono_start = std::chrono::high_resolution_clock::now();


    std::vector<int> in_1, in_2;
    read_file(path_1, in_1);
    read_file(path_2, in_2);

    DEBUG(2, "in_1.size(): " << in_1.size());
    DEBUG(2, "in_2.size(): " << in_2.size());

    int d_max = in_1.size() + in_2.size() + 1;

    int k_min = -(std::min((size_t) d_max, in_2.size()) - 1);
    int k_max = std::min((size_t) d_max, in_1.size()) - 1;
    Snakes snakes = compute_all_snakes_seq(in_1, in_2, k_min, k_max);

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

            if (x < in_1.size() && y < in_2.size()) {       //TODO: needed?
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

                // stop TIMER
                auto chrono_end = std::chrono::high_resolution_clock::now();
                auto chrono_t = std::chrono::duration_cast<std::chrono::microseconds>(chrono_end - chrono_start).count();
                std::cout << "chrono Time [μs]: \t" << chrono_t << std::endl << std::endl;

                goto done;
            }
        }
    }

done:
    std::cout << "min edit length " << edit_len << std::endl;

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
    
    return 0;
}
