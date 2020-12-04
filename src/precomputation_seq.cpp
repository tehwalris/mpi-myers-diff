#include "common.hpp"
#include <algorithm>
#include <assert.h>
#include <stdlib.h> 
#include <cmath> 


void discard_lines_without_match_seq(std::vector<int> &in_1, std::vector<int> &in_2, 
                                    std::vector<bool> &deletions, std::vector<std::pair<size_t, int>> &insertions,
                                    std::vector<size_t> &real_indices_1, std::vector<size_t> &real_indices_2) {

    std::vector<char> matches;      // contains a tristate for each value of in_1: 0 => neither occurs in in_1 nor in_2; 1 => occurs in in_1, but not in_2, 2 => occurs in both files

    assert(in_1.size() == deletions.size());

    // insert false entry for each value that occurs in in_1
    for (const int value : in_1) {
        if (value >= matches.size()) {
            // need to resize matches vector
            matches.resize(value + 1);
        }
        matches[value] = 1;
    }

    
    for (size_t j = 0; j < in_2.size(); ++j) {
        int value = in_2[j];
        if (value >= matches.size() || matches[value] == 0) {
            // value not present in in_1 => clearly an insertion
            insertions.push_back(std::make_pair(j, value));
            in_2[j] = -1;        // mark as to be deleted
        }
        else {
            // value present in in_1 -> found a match
            matches[value] = 2;
            // add real_index to reconstruct index after deletion
            real_indices_2.push_back(j);
        }
    }

    for (size_t i = 0; i < in_1.size(); ++i) {
        int value = in_1[i];
        if (matches[value] == 1) {
            // no match found in in_2 => clearly a deletion
            deletions[i] = true;
            in_1[i] = -1;         // mark as to be deleted
        }
        else {  // matches[value] == 2
            // match found => add real_index reconstruct index after deletion
            real_indices_1.push_back(i);
        }
    }

    // remove elements from in_1 in O(n)
    auto to_be_deleted_1 = std::remove(in_1.begin(), in_1.end(), -1);
    in_1.erase(to_be_deleted_1, in_1.end());

    // remove elements from in_2 in O(n)
    auto to_be_deleted_2 = std::remove(in_2.begin(), in_2.end(), -1);
    in_2.erase(to_be_deleted_2, in_2.end());
}



//TODO:? find_identical_ends()