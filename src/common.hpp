#include <vector>

/* Sequential version: 
   Discard lines from one file that have no matches in the other file.

   A line which is discarded will not be considered by the actual
   comparison algorithm; it will be as if that line were not in the file.
   The file's 'realindexes' table maps virtual line numbers
   (which don't count the discarded lines) into real line numbers;
   this is how the actual comparison algorithm produces results
   that are comprehensible when the discarded lines are counted.

   When we discard a line, we also mark it as a deletion or insertion
   so that it will be printed in the output.  
   
   cf. diffutils: analyze.c::discard_confusing_lines
   */
void discard_lines_without_match_seq(std::vector<int> &in_1, std::vector<int> &in_2, 
                                    std::vector<bool> &deletions, std::vector<std::pair<size_t, int>> &insertions,
                                    std::vector<size_t> &real_indices_1, std::vector<size_t> &real_indices_2);