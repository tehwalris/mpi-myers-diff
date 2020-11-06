#include <iostream>
#include <fstream>
#include <assert.h>
#include <vector>
#include <algorithm>

const int debug_level = 2;

#define DEBUG(level, x)              \
    if (debug_level >= level)        \
    {                                \
        std::cerr << x << std::endl; \
    }

const int shutdown_sentinel = -1;
const int unknown_len = -1;
const int no_worker_rank = 0;

typedef std::vector<std::vector<int>> Results;

int &result_at(int d, int k, Results &results)
{
    auto &row = results.at(d);
    return row.at(row.size() / 2 + k);
}

void print_vector(const std::vector<int> &vec)
{
    for (int i = 0; i < vec.size(); i++)
    {
        if (i != 0)
        {
            DEBUG(2, " ");
        }
        DEBUG(2, vec.at(i));
    }
    DEBUG(2, std::endl);
}

void read_file(const std::string path, std::vector<int> &output_vec)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Could not open file " << path;
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

    if (argc < 3)
    {
        std::cerr << "You must provide two paths to files to be compared as arguments";
        exit(1);
    }
    else
    {
        path_1 = argv[1];
        path_2 = argv[2];
    }

    // main_master
    std::vector<int> in_1, in_2;
    read_file(path_1, in_1);
    read_file(path_2, in_2);

    int d_max = in_1.size() + in_2.size() + 1;

    int edit_len = unknown_len;
    Results results(d_max, std::vector<int>(2 * d_max + 1));

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
            else if (k == -d || k != d && result_at(d - 1, k - 1, results) < result_at(d - 1, k + 1, results))
            {
                x = result_at(d - 1, k + 1, results);
            }
            else
            {
                x = result_at(d - 1, k - 1, results) + 1;
            }

            int y = x - k;

            while (x < in_1.size() && y < in_2.size() && in_1.at(x) == in_2.at(y))
            {
                x++;
                y++;
            }

            DEBUG(2, "x: " << x);
            result_at(d, k, results) = x;

            if (x >= in_1.size() && y >= in_2.size())
            {
                DEBUG(2, "found lcs");
                edit_len = d;
                goto done;
            }
        }
    }

done:
    std::cout << "min edit length " << edit_len << std::endl;

    return 0;
}