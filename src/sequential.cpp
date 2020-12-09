#include <iostream>
#include <fstream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono> // chrono::high_resolution_clock
#include "priority/storage.hpp"
#include "priority/calculate.hpp"

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

#ifdef FRONTIER_STORAGE
typedef FrontierStorage Storage;
#else
#define EDIT_SCRIPT
typedef FastStorage Storage;
#endif

const int shutdown_sentinel = -1;
const int unknown_len = -1;
const int no_worker_rank = 0;

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

    std::string path_1, path_2;

#ifdef EDIT_SCRIPT
    std::string edit_script_path;
    bool edit_script_to_file = false;
#endif

    if (argc < 3)
    {
        std::cerr << "You must provide two paths to files to be compared as arguments" << std::endl;
        exit(1);
    }
    else
    {
        path_1 = argv[1];
        path_2 = argv[2];

#ifdef EDIT_SCRIPT
        if (argc >= 4)
        {
            edit_script_path = argv[3];
            edit_script_to_file = true;
        }
#endif
    }

    // TIMERS
    std::chrono::_V2::system_clock::time_point t_in_start, t_in_end, t_sol_start, t_sol_end, t_script_start, t_script_end;
    t_in_start = std::chrono::high_resolution_clock::now();

    std::vector<int> in_1, in_2;
    read_file(path_1, in_1);
    read_file(path_2, in_2);

    t_in_end = std::chrono::high_resolution_clock::now();

    DEBUG(2, "in_1.size(): " << in_1.size());
    DEBUG(2, "in_2.size(): " << in_2.size());

    t_sol_start = std::chrono::high_resolution_clock::now();

    int d_max = in_1.size() + in_2.size() + 1;

    int edit_len = unknown_len;

    Storage results(d_max);

    const int in_1_size = in_1.size(), in_2_size = in_2.size();

    for (int d = 0; d < d_max; d++)
    {
        DEBUG(2, "calculating layer " << d);

        std::optional<int> lcs_k_opt = calculate_row_shared(results, in_1, in_2, d, -d, d);
        if (lcs_k_opt.has_value())
        {
            DEBUG(2, "found lcs");
            edit_len = d;
            goto done;
        }
    }

done:
    t_sol_end = std::chrono::high_resolution_clock::now();
    t_script_start = std::chrono::high_resolution_clock::now();

#ifdef EDIT_SCRIPT
    std::vector<struct Edit_step> steps(edit_len);
    int k = in_1.size() - in_2.size();
    for (int d = edit_len; d > 0; d--)
    {
        if (k == -d || k != d && results.get(d - 1, k - 1) < results.get(d - 1, k + 1))
        {
            k = k + 1;
            int x = results.get(d - 1, k);
            int y = x - k;
            int val = in_2.at(y);
            DEBUG(2, "y: " << y << " in_2: " << val);
            steps[d - 1] = {x, val, true};
        }
        else
        {
            k = k - 1;
            int x = results.get(d - 1, k) + 1;
            steps[d - 1] = {x, -1, false};
        }
    }

    std::ofstream edit_script_file;
    auto cout_buf = std::cout.rdbuf();
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

    for (int i = 0; i < steps.size(); i++)
    {
        struct Edit_step step = steps.at(i);
        if (step.mode)
        {
            std::cout << step.x << " + " << step.insert_val << std::endl;
        }
        else
        {
            std::cout << step.x << " -" << std::endl;
        }
    }

    if (edit_script_to_file)
    {
        edit_script_file.close();
    }

    std::cout.rdbuf(cout_buf);
#endif

    t_script_end = std::chrono::high_resolution_clock::now();

    std::cout << "\nmin edit length " << edit_len << std::endl
              << std::endl;
    std::cout << "Read Input [μs]: \t" << std::chrono::duration_cast<std::chrono::microseconds>(t_in_end - t_in_start).count() << std::endl;
    std::cout << "Precompute [μs]: \t" << 0 << std::endl;
    std::cout << "Solution [μs]:   \t" << std::chrono::duration_cast<std::chrono::microseconds>(t_sol_end - t_sol_start).count() << std::endl;
    std::cout << "Edit Script [μs]: \t" << std::chrono::duration_cast<std::chrono::microseconds>(t_script_end - t_script_start).count() << std::endl;

    return 0;
}
