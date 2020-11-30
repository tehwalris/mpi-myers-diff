#include <vector>
#include <assert.h>


struct SnakesOnDiag {

    std::vector<int> m_data;
    int m_k;
    int m_curr_idx;       // for sequential lookup

    SnakesOnDiag(int k) {
        m_k = k;
        m_curr_idx = 0;
    }

    // 
    void append_snake(int x_end) {
        m_data.push_back(x_end);
    }

    // expects sequential lookup
    int get_end_of_snake(int x_start) {
        assert(x_start < m_data[m_data.size() - 1]);
        // search for snake from x_start:
        while (m_data.at(m_curr_idx) < x_start) {
            m_curr_idx++;
        }
        return m_data[m_curr_idx];
    }
};

struct Snakes {

    std::vector<SnakesOnDiag> m_data;
    int m_k_min;        // inclusive
    int m_k_max;

    Snakes() {}

    Snakes(int k_min, int k_max) {
        m_k_min = k_min;
        m_k_max = k_max;
        m_data.reserve(k_max - k_min + 1);
    }

    void append_snakes_on_diag(SnakesOnDiag snakes) {
        m_data.push_back(snakes);
    }

    int get_end_of_snake(int k, int x_start) {
        assert(m_k_min <= k && k <= m_k_max);
        return m_data.at(k - m_k_min).get_end_of_snake(x_start);
    }
};


Snakes compute_all_snakes_seq(const std::vector<int> &in_1, const std::vector<int> &in_2, int k_min, int k_max) {    // inclusive
    if (k_min > k_max) {
        // return empty Snakes
        return Snakes();
    }
    Snakes result(k_min, k_max);       // #diagonals

    // for each diagonal
    int k = k_min;
    for (; k < 0; ++k) {
        SnakesOnDiag curr_snakes(k);
        // k = x - y, k negative => first element of diagonal has x = 0
        for (int x = 0, y = - k; x < in_1.size() && y < in_2.size(); x++, y++)
        {
            if (in_1.at(x) != in_2.at(y))
                curr_snakes.append_snake(x);
        }
        // add sentinel entry = 1 over end of diagonal for last snake
        int diag_len = std::min(in_1.size(), in_2.size() + k);       // could be optimized
        curr_snakes.append_snake(diag_len);

        result.append_snakes_on_diag(curr_snakes);
    }
    // k == 0
    for (; k <= k_max; ++k) {
       SnakesOnDiag curr_snakes(k);
        // k = x - y, k positive => first element of diagonal has y = 0
        for (int x = k, y = 0; x < in_1.size() && y < in_2.size(); x++, y++)
        {
            if (in_1.at(x) != in_2.at(y))
                curr_snakes.append_snake(x);
        }
        // add sentinel entry = 1 over end of diagonal for last snake
        int diag_len = std::min(in_1.size() - k, in_2.size());       // could be optimized
        curr_snakes.append_snake(k + diag_len);

        result.append_snakes_on_diag(curr_snakes);
    }

    return result;
}
