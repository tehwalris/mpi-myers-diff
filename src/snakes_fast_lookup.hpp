#include <vector>
#include <assert.h>


struct SnakesOnDiag {

    std::vector<int> m_data;
    int m_k;

    SnakesOnDiag(int k) {
        m_k = k;
    }

    SnakesOnDiag(int k, size_t length) {
        m_k = k;
        m_data.reserve(length);
    }

    // all x in [m_data.size() (+k), x_end] will be mapped to x_end
    void append_snake(int x_end) {
        if (m_k < 0) {      // diagonal starts at x = 0
            for (int i = m_data.size(); i <= x_end; i++) {
                m_data.push_back(x_end);
            }
        }
        else {              // diagonal starts at x = k
            for (int i = m_data.size() ; i <= x_end - m_k; i++) {
                m_data.push_back(x_end);
            }
        }
    }

    int get_end_of_snake(int x_start) {
        if (m_k < 0) {      // diagonal starts at x = 0
            return m_data.at(x_start);
        }
        else {              // diagonal starts at x = k
            return m_data.at(x_start - m_k);
        }
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
    Snakes result(k_min, k_max);

    // for each diagonal
    int k = k_min;
    for (; k < 0; ++k) {
        int diag_len = std::min(in_1.size(), in_2.size() + k);       // could be optimized
        SnakesOnDiag curr_snakes(k, diag_len);
        // k = x - y, k negative => first element of diagonal has x = 0
        for (int x = 0, y = - k; x < diag_len; x++, y++) {
            if (in_1.at(x) != in_2.at(y)) {       // found the end of the snake
                // insert the end of the snake for all previous x on the snake
                curr_snakes.append_snake(x);
            }
        }
        // add last snake, which runs till the end
        curr_snakes.append_snake(diag_len);

        result.append_snakes_on_diag(curr_snakes);
    }
    // k == 0
    for (; k <= k_max; ++k) {
        int diag_len = std::min(in_1.size() - k, in_2.size());       // could be optimized
        SnakesOnDiag curr_snakes(k, diag_len);
        // k = x - y, k positive => first element of diagonal has y = 0
        for (int x = k, y = 0; y < diag_len; x++, y++) {
            if (in_1.at(x) != in_2.at(y)) {       // found the end of the snake
                // insert the end of the snake for all previous x on the snake
                curr_snakes.append_snake(x);
            }
        }
        // add last snake, which runs till the end
        curr_snakes.append_snake(k + diag_len);

        result.append_snakes_on_diag(curr_snakes);
    }

    return result;
}