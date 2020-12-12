#include <mpi.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <new>
#include <cmath>
#include "priority/storage.hpp"
#include "priority/calculate.hpp"
#include "snake_computation.hpp"

const int debug_level = 0;

#ifndef NDEBUG
#define DEBUG(level, x)          \
  if (debug_level >= level)      \
  {                              \
    std::cerr << x << std::endl; \
  }
#define DEBUG_NO_LINE(level, x) \
  if (debug_level >= level)     \
  {                             \
    std::cerr << x;             \
  }
#else
#define DEBUG(level, x)
#define DEBUG_NO_LINE(level, x)
#endif

#ifdef FRONTIER_STORAGE
typedef FrontierStorageWithHasValue Storage;
#else
#define EDIT_SCRIPT
typedef FastStorageWithHasValue Storage;
#endif

const int shutdown_sentinel = 1;
const int unknown_len = -1;
const int no_worker_rank = 0;

// This number must be greater or equal to 2
// use -min_entries 10 to set it manually
int MIN_ENTRIES = 200; // min. number of initial entries to compute on one node per layer d before the next node is started


enum Tag
{
  ResultEntry = 0,
  ResultEntryData = 1,
  StopWorkers = 2,
  ReadOut = 3,
  ReadOutData = 4,
  ReadOutStopWorkers = 5,
  ReadOutLcsLength = 6,
};

std::pair<int, int> get_k_bounds(const int d, const int worker_rank, const int num_workers)
{
  int k_min;
  int k_max;
  if (d >= num_workers * MIN_ENTRIES)
  {
    k_min = -(d) + (d / num_workers) * 2 * worker_rank + std::min(worker_rank * 2, ((d) % num_workers) * 2 + 2);
    int k_num = (d) / num_workers + ((d) % num_workers >= worker_rank ? 1 : 0);
    k_max = k_min + 2 * k_num - 2;
  }
  else
  {
    k_min = -(d) + (MIN_ENTRIES * 2) * worker_rank;
    if ((d) >= (worker_rank + 1) * MIN_ENTRIES - 1)
    {
      k_max = k_min + 2 * MIN_ENTRIES - 2;
    }
    else if ((d) >= worker_rank * MIN_ENTRIES)
    {
      k_max = k_min + ((d)-worker_rank * MIN_ENTRIES) * 2;
    }
    else
    {
      // Worker has no more work. Create empty interval
      k_max = k_min - 1;
    }
  }
  return std::pair<int, int>(k_min, k_max);
}

struct interval
{
  std::vector<std::pair<int, int>> m_interval_set;

  size_t size()
  {
    return m_interval_set.size();
  }

  void insert(int min, int max)
  {
    DEBUG(3, "INTERVAL: inserting (" << min << "," << max << ")");
    if (m_interval_set.size() == 0)
    {
      m_interval_set.push_back(std::pair<int, int>(min, max));
      return;
    }
    for (size_t i = 0; i < m_interval_set.size(); i++)
    {
      auto &cur = m_interval_set.at(i);
      DEBUG(3, "INTERVAL: cur (" << cur.first << "," << cur.second << ")");
      if (cur.first > max)
      {
        DEBUG(3, "INTERVAL: Inserting");
        m_interval_set.insert(m_interval_set.begin() + i, std::pair<int, int>(min, max));
        break;
      }
      else if (cur.first == max)
      {
        DEBUG(3, "INTERVAL: Prepending");
        // Can prepend?
        cur.first = min;
        break;
      }
      else if (cur.second == min)
      {
        DEBUG(3, "INTERVAL: Extending");
        // Can extend?
        cur.second = max;
        if (i < m_interval_set.size() - 1 && m_interval_set.at(i + 1).first == max)
        {
          DEBUG(3, "INTERVAL: Merging");
          // Can merge?
          cur.second = m_interval_set.at(i + 1).second;
          m_interval_set.erase(m_interval_set.begin() + i + 1);
        }
        break;
      }
    }
    //for(auto i = m_interval_set.begin(); i != m_interval_set.end();i++){
    //    DEBUG_NO_LINE(2,"(min: " << i->first << ", max: " << i->second << ")");
    //}
    DEBUG(2, "");
  }
};

struct edit_step
{
  /** Position at which to perform the edit step */
  int x;
  /** Value to insert. This value is ignored when in delete mode */
  int insert_val;
  /** Mode of this edit step. 1 means addition, 0 deletion */
  int mode;
};

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

// a worker has reached the end of the input and informs the other workers
void stop_workers(const int num_workers, const int worker_rank, const Tag tag)
{

  DEBUG(2, worker_rank << " | shutting down workers");

  for (int i = 0; i < num_workers; i++)
  {
    if (i != worker_rank)
    {
      std::vector<int> msg{shutdown_sentinel, -1, -1};
      MPI_Send(msg.data(), msg.size(), MPI_INT, i, tag, MPI_COMM_WORLD);
    }
  }
}

// compute entry x and add it to the data structure
// returns true if it found the solution
inline bool compute_entry(int d, int k, Storage &data, const std::vector<int> &in_1, const std::vector<int> &in_2)
{
  int x;
  if (d == 0)
  {
    x = 0;
  }
  else if (k == -d || (k != d && data.get(d - 1, k - 1) < data.get(d - 1, k + 1)))
  {
    x = data.get(d - 1, k + 1);
  }
  else
  {
    x = data.get(d - 1, k - 1) + 1;
  }

  int y = x - k;

  compute_end_of_snake(x, y, in_1, in_2);

  DEBUG_NO_LINE(3, "(" << d << ", " << k << "): ");
  DEBUG_NO_LINE(3, "x: " << x);
  DEBUG(3, ", y; " << y);

  data.set(d, k, x);

  // LCS found
  if (x >= in_1.size() && y >= in_2.size())
  {
    return true;
  }

  return false;
}

// blocking wait to receive a message from worker_rank-1 for layer d
// updates the entires in results
// returns true if StopWorkers received
bool Recv(int source_rank, int d, int k, Storage &results, int d_max, int worker_rank /*only for debug output*/)
{
  MPI_Status status;
  std::vector<int> msg(3);
  int x;

  if (d >= d_max)
  {
    // This worker will not be needed as its d is bigger than the maximum allowed one
    return true;
  }
  // check if already received
  if (results.has_value(d, k))
    return false;

  DEBUG(2, worker_rank << " | WAIT for " << source_rank << " (" << d << ", " << k << ")");
  int d_rcv = -1;
  int k_rcv = k + 1;
  while (d_rcv != d || k_rcv != k)
  { // expected value

    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    if (status.MPI_TAG == Tag::ReadOut || status.MPI_TAG == Tag::ReadOutData || status.MPI_TAG == Tag::ReadOutStopWorkers)
    {
      DEBUG(2, worker_rank << " | There is a worker already reading out. Stop." << status.MPI_SOURCE);
      return true;
    }

    MPI_Recv(msg.data(), msg.size(), MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
    if (status.MPI_TAG == Tag::StopWorkers)
    {
      DEBUG(2, worker_rank << " | received stop signal from " << status.MPI_SOURCE);
      return true;
    }

    // save result
    d_rcv = msg.at(0);
    k_rcv = msg.at(1);
    x = msg.at(2);
    results.set(d_rcv, k_rcv, x);

    DEBUG(2, worker_rank << " | "
                         << "receiving from " << status.MPI_SOURCE << ": (" << d_rcv << ", " << k_rcv << "): " << x << " (Tag: " << status.MPI_TAG << ")");
    assert(x > 0);
  }

  return false;
}

/**
* Computes the entries in the DP-table
* returns: edit length or -1 if another worker found the solution
*/
int calculate_table(const std::vector<int> &in_1, const std::vector<int> &in_2, Storage &results, int d_max,
                    const int worker_rank, const int num_workers)
{

  int d_start = (worker_rank)*MIN_ENTRIES;   // first included layer
  int d_end = num_workers * MIN_ENTRIES - 1; // last included layer
  int k_min = d_start;                       // left included k index
  int k_max = d_start;                       // right included k index
  int d_bot_right = (worker_rank + 1) * (MIN_ENTRIES)-1;

  // WAITING on first required input
  if (worker_rank != 0)
  {
    // Receive (d_start-1, k_min-1) from worker_rank-1
    if (Recv(worker_rank - 1, d_start - 1, k_min - 1, results, d_max, worker_rank))
      return -1;
  }
  DEBUG(2, worker_rank << " | got input");

  // INITIAL PHASE
  // first worker starts alone on its pyramid until it reaches point where one layer contains MIN_ENTRIES
  for (int d = d_start; d < std::min((worker_rank + 1) * MIN_ENTRIES, d_max); ++d)
  {
    std::optional<int> done = calculate_row_shared(results, in_1, in_2, d, k_min, k_max);
    if (done.has_value())
    {
      stop_workers(num_workers, worker_rank, Tag::StopWorkers);
      return d;
    }
    // check for shutdown
    // MPI_Test(&shutdown_request, &shutdown_flag, MPI_STATUS_IGNORE);
    // if (shutdown_flag) return;

    // Receive entry (d, k_min-2) from worker_rank-1 for next round
    if (worker_rank != 0 && d < d_end)
    {
      if (Recv(worker_rank - 1, d, k_min - 2, results, d_max, worker_rank))
        return -1;
    }
#ifndef NDEBUG
    auto bounds = get_k_bounds(d, worker_rank, num_workers);
    int calc_k_min = bounds.first;
    int calc_k_max = bounds.second;
    DEBUG(2, worker_rank << " | d:" << d << " actual_k_min:" << k_min << " calc_k_min:" << calc_k_min << " actual_k_max:" << k_max << " calc_k_max:" << calc_k_max);
    assert(k_min == calc_k_min && k_max == calc_k_max);
#endif
    k_min--;
    k_max++;
  }

  // send bottom right edge node to next worker
  int x;
  if (worker_rank != num_workers - 1)
  {
    x = results.get(d_bot_right, d_bot_right);
    std::vector<int> msg{d_bot_right, d_bot_right, x};
    DEBUG(2, worker_rank << " | Send to " << worker_rank + 1 << " (" << d_bot_right << ", " << d_bot_right << ")");
    MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank + 1, Tag::ResultEntry, MPI_COMM_WORLD);
  }
  DEBUG(2, worker_rank << " | spawned new worker");
  // GROW INDIVIDUAL WORKERS
  // after a worker has passed their initial phase, we add additional workers
  // Each worker computes MIN_ENTRIES nodes per layer and shifts to the left with every additional layer.
  k_max = (worker_rank + 1) * (MIN_ENTRIES)-2;
  for (int d = (worker_rank + 1) * MIN_ENTRIES; d < std::min(num_workers * MIN_ENTRIES, d_max); ++d)
  {

    // compute (d, k_max)
    DEBUG_NO_LINE(3, worker_rank << " | ");
    bool done = compute_entry(d, k_max, results, in_1, in_2);
    x = results.get(d, k_max);
    if (done)
    {
      stop_workers(num_workers, worker_rank, Tag::StopWorkers);
      return d;
    }

    // Send right entry (d, k_max) to worker_rank+1
    // except right-most worker never sends and we are not in the last layer of the growth phase.
    if (worker_rank != num_workers - 1 && d < d_end)
    {
      DEBUG(2, worker_rank << " | Send to " << worker_rank + 1 << " (" << d << ", " << k_max << ")");
      std::vector<int> msg{d, k_max, x};
      MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank + 1, Tag::ResultEntry, MPI_COMM_WORLD);
    }

    std::optional<int> done_row = calculate_row_shared(results, in_1, in_2, d, k_min, k_max-2);
    if (done_row.has_value())
    {
      stop_workers(num_workers, worker_rank, Tag::StopWorkers);
      return d;
    }
    // Receive entry on left of row (d, k_min-2) from worker_rank-1
    // except worker 1 never receives and doesn't apply to very last row of growth phase.
    if (worker_rank != 0 && d < d_end)
    {
      if (Recv(worker_rank - 1, d, k_min - 2, results, d_max, worker_rank))
        return -1;
    }
#ifndef NDEBUG
    auto bounds = get_k_bounds(d, worker_rank, num_workers);
    int calc_k_min = bounds.first;
    int calc_k_max = bounds.second;
    DEBUG(2, worker_rank << " | d:" << d << " actual_k_min:" << k_min << " calc_k_min:" << calc_k_min << " actual_k_max:" << k_max << " calc_k_max:" << calc_k_max);
    assert(k_min == calc_k_min && k_max == calc_k_max);
#endif
    // new range for next round d+1
    k_min--; //extend    // -d has decreased by 1, same number of entries on the left
    k_max--; //decrease  // -d has decreased by 1, same total number of entries at this node
  }

  // Correct range for new phase
  k_min++;
  k_max++;

  DEBUG(2, worker_rank << " | All workers active");

  if (worker_rank > 0 && num_workers * MIN_ENTRIES - 1 < d_max)
  {
    int d = num_workers * MIN_ENTRIES - 1;
    // Send entry (d, k_min) to worker_rank-1
    x = results.get(d, k_min);
    std::vector<int> msg{d, k_min, x};
    DEBUG(2, worker_rank << " | Pre Send to " << worker_rank - 1 << " (" << d << ", " << k_min << ", " << x << ")");
    MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank - 1, Tag::ResultEntry, MPI_COMM_WORLD);
  }

  // ALL WORKERS ACTIVE
  // BALANCING
  for (int d = num_workers * MIN_ENTRIES; d < d_max; ++d)
  {
    bool done = false;
    if (d % num_workers == num_workers - 1)
    {
      DEBUG(2, worker_rank << " | in case 5");
      k_min--;
      if (worker_rank == num_workers - 1)
      {
        k_max++;
      }
      else
      {
        k_max--;
      }
      // Receive entry (d, k_min-2) from worker_rank-1
      if (worker_rank > 0)
      {
        if (Recv(worker_rank - 1, d - 1, k_min - 1, results, d_max, worker_rank))
          return -1;
      }
      DEBUG_NO_LINE(3, worker_rank << " | ");
      done = compute_entry(d, k_min, results, in_1, in_2);
      if (done)
      {
        stop_workers(num_workers, worker_rank, Tag::StopWorkers);
        return d;
      }
      if (worker_rank > 0)
      {
        // Send entry (d, k_min) to worker_rank-1
        x = results.get(d, k_min);
        std::vector<int> msg{d, k_min, x};
        DEBUG(2, worker_rank << " | Send to " << worker_rank - 1 << " (" << d << ", " << k_min << ", " << x << ")");
        MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank - 1, Tag::ResultEntry, MPI_COMM_WORLD);
      }
      DEBUG_NO_LINE(3, worker_rank << " | ");
      done = compute_entry(d, k_max, results, in_1, in_2);
      if (done)
      {
        stop_workers(num_workers, worker_rank, Tag::StopWorkers);
        return d;
      }
    }
    else if (d % num_workers == worker_rank)
    {
      DEBUG(2, worker_rank << " | in case 3");
      k_min--;
      k_max++;
      if (worker_rank != num_workers - 1)
      {
        // Receive entry (d, k_max+2) from worker_rank+1  //TODO: receive later, only when needed?
        if (Recv(worker_rank + 1, d - 1, k_max + 1, results, d_max, worker_rank))
          return -1;
      }
      // Receive entry (d, k_min-2) from worker_rank-1
      if (worker_rank > 0)
      {
        if (Recv(worker_rank - 1, d - 1, k_min - 1, results, d_max, worker_rank))
          return -1;
      }
      DEBUG_NO_LINE(3, worker_rank << " | ");
      done = compute_entry(d, k_max, results, in_1, in_2);
      if (done)
      {
        stop_workers(num_workers, worker_rank, Tag::StopWorkers);
        return d;
      }
      if (worker_rank != num_workers - 1)
      {
        x = results.get(d, k_max);
        std::vector<int> msg{d, k_max, x};
        DEBUG(2, worker_rank << " | Send to " << worker_rank + 1 << " (" << d << ", " << k_max << ", " << x << ")");
        MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank + 1, Tag::ResultEntry, MPI_COMM_WORLD);
      }
      DEBUG_NO_LINE(3, worker_rank << " | ");
      done = compute_entry(d, k_min, results, in_1, in_2);
      if (done)
      {
        stop_workers(num_workers, worker_rank, Tag::StopWorkers);
        return d;
      }
    }
    else if (d % num_workers + 1 == worker_rank)
    {
      DEBUG(2, worker_rank << " | in case 2");
      k_min++;
      k_max++;
      if (worker_rank != num_workers - 1)
      {
        // Receive entry (d, k_max+2) from worker_rank+1  //TODO: receive later, only when needed?
        if (Recv(worker_rank + 1, d - 1, k_max + 1, results, d_max, worker_rank))
          return -1;
      }
      DEBUG_NO_LINE(3, worker_rank << " | ");
      done = compute_entry(d, k_max, results, in_1, in_2);
      if (done)
      {
        stop_workers(num_workers, worker_rank, Tag::StopWorkers);
        return d;
      }
      DEBUG_NO_LINE(3, worker_rank << " | ");
      done = compute_entry(d, k_min, results, in_1, in_2);
      if (done)
      {
        stop_workers(num_workers, worker_rank, Tag::StopWorkers);
        return d;
      }
    }
    else if (d % num_workers < worker_rank)
    {
      DEBUG(2, worker_rank << " | in case 1");
      k_min++;
      k_max++;
      if (worker_rank != num_workers - 1)
      {
        // Receive entry (d, k_max+2) from worker_rank+1  //TODO: receive later, only when needed?
        if (Recv(worker_rank + 1, d - 1, k_max + 1, results, d_max, worker_rank))
          return -1;
      }
      DEBUG_NO_LINE(3, worker_rank << " | ");
      done = compute_entry(d, k_min, results, in_1, in_2);
      if (done)
      {
        stop_workers(num_workers, worker_rank, Tag::StopWorkers);
        return d;
      }
      if (worker_rank > 0)
      {
        // Send entry (d, k_min) to worker_rank-1
        x = results.get(d, k_min);
        std::vector<int> msg{d, k_min, x};
        DEBUG(2, worker_rank << " | Send to " << worker_rank - 1 << " (" << d << ", " << k_min << ", " << x << ")");
        MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank - 1, Tag::ResultEntry, MPI_COMM_WORLD);
      }
      DEBUG_NO_LINE(3, worker_rank << " | ");
      done = compute_entry(d, k_max, results, in_1, in_2);
      if (done)
      {
        stop_workers(num_workers, worker_rank, Tag::StopWorkers);
        return d;
      }
    }
    else
    {
      DEBUG(2, worker_rank << " | in case 4");
      k_min--;
      k_max--;
      // Receive entry (d, k_min-2) from worker_rank-1
      if (worker_rank > 0)
      {
        if (Recv(worker_rank - 1, d - 1, k_min - 1, results, d_max, worker_rank))
          return -1;
      }
      DEBUG_NO_LINE(3, worker_rank << " | ");
      done = compute_entry(d, k_max, results, in_1, in_2);
      if (done)
      {
        stop_workers(num_workers, worker_rank, Tag::StopWorkers);
        return d;
      }
      if (worker_rank != num_workers - 1)
      {
        x = results.get(d, k_max);
        std::vector<int> msg{d, k_max, x};
        DEBUG(2, worker_rank << " | Send to " << worker_rank + 1 << " (" << d << ", " << k_max << ", " << x << ")");
        MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank + 1, Tag::ResultEntry, MPI_COMM_WORLD);
      }
      DEBUG_NO_LINE(3, worker_rank << " | ");
      done = compute_entry(d, k_min, results, in_1, in_2);
      if (done)
      {
        stop_workers(num_workers, worker_rank, Tag::StopWorkers);
        return d;
      }
    }

#ifndef NDEBUG
    auto bounds = get_k_bounds(d, worker_rank, num_workers);
    int calc_k_min = bounds.first;
    int calc_k_max = bounds.second;
    DEBUG(2, worker_rank << " | d:" << d << " actual_k_min:" << k_min << " calc_k_min:" << calc_k_min << " actual_k_max:" << k_max << " calc_k_max:" << calc_k_max);
    assert(k_min == calc_k_min && k_max == calc_k_max);
#endif
    DEBUG(2, worker_rank << " | Do rest of line");
    std::optional<int> done_row = calculate_row_shared(results, in_1, in_2, d, k_min+2, k_max-2);
    if (done_row.has_value())
    {
      stop_workers(num_workers, worker_rank, Tag::StopWorkers);
      return d;
    }
  }
  return -1;
}

void main_worker(const std::string &path_1, const std::string &path_2, bool edit_script_to_file, const std::string &edit_script_path)
{

  int worker_rank;
  int comm_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &worker_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
  int num_workers = comm_size;

  std::vector<int> in_1;
  std::vector<int> in_2;

  // Init Timers
  // Reading Input file (and sending), precomputation (snakes), computation of edit length, reconstructing the solution and the edit script
  std::chrono::_V2::system_clock::time_point t_in_start, t_in_end, t_pre_start, t_pre_end, t_sol_start, t_sol_end, t_script_start, t_script_end;

  if (worker_rank > 0)
  {
    DEBUG(2, worker_rank << " | "
                         << "receiving inputs");
    auto receive_vector = []() {
      int temp_size;
      MPI_Bcast(&temp_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
      std::vector<int> vec(temp_size);
      MPI_Bcast((void *)(vec.data()), vec.size(), MPI_INT, 0, MPI_COMM_WORLD);
      return vec;
    };
    in_1 = receive_vector();
    in_2 = receive_vector();
  }
  else
  {
    t_in_start = std::chrono::high_resolution_clock::now();
    read_file(path_1, in_1);
    read_file(path_2, in_2);

    DEBUG(2, worker_rank << " | sending inputs");
    auto send_vector = [](const std::vector<int> &vec) {
      int temp_size = vec.size();
      MPI_Bcast(&temp_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
      MPI_Bcast((void *)(vec.data()), vec.size(), MPI_INT, 0, MPI_COMM_WORLD);
    };
    send_vector(in_1);
    send_vector(in_2);
    t_in_end = std::chrono::high_resolution_clock::now();
  }

  t_sol_start = std::chrono::high_resolution_clock::now();
  int d_max = in_1.size() + in_2.size() + 1;
  Storage results(d_max);

  int edit_len = calculate_table(in_1, in_2, results, d_max, worker_rank, num_workers);
  DEBUG(2, worker_rank << " | Done calculating");

  // TODO pascalm timer of worker 0 is not the time it took until the solution, but the time until
  // it found a solution OR until it received the shutdown message.
  // Worker that finds solution (unless rank==0) should send 2 timers to 0:
  //    - t_sol_end
  //    - t_script_start
  t_sol_end = std::chrono::high_resolution_clock::now();

#ifdef EDIT_SCRIPT

  // read result and send it to next worker
  t_script_start = std::chrono::high_resolution_clock::now();
  std::vector<struct edit_step> steps;
  bool first = false;
  int d = edit_len;
  int k = in_1.size() - in_2.size();
  if (edit_len >= 0)
  {
    steps = std::vector<struct edit_step>(edit_len);
    first = true;
  }
  MPI_Datatype MPI_edit_step_t;
  MPI_Type_contiguous(3, MPI_INT, &MPI_edit_step_t);
  MPI_Type_commit(&MPI_edit_step_t);

  // For worker 0 to know if it has to wait for other workers
  interval recv_part;

  std::vector<int> msg(3);
  MPI_Status status;
  while (true)
  {
    if (!first)
    {
      DEBUG(2, worker_rank << " | Waiting for work");
      while (true)
      {
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (worker_rank == 0)
        {
          if (status.MPI_TAG == Tag::ReadOutData)
          {
            DEBUG(2, worker_rank << " | reading data. Tag: " << status.MPI_TAG);
            int num;
            MPI_Get_count(&status, MPI_edit_step_t, &num);
            std::vector<struct edit_step> buf(num);
            MPI_Recv(buf.data(), buf.size(), MPI_edit_step_t, status.MPI_SOURCE, Tag::ReadOutData, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            int d_recv = buf[0].x;
            int e_len = buf[0].insert_val;
            int size = buf.size() - 1;
            DEBUG(2, worker_rank << " | Received Data. (d:" << d_recv << ", edit_len:" << e_len << ", mode:" << buf[0].mode << ")");
            assert(buf[0].mode == 2);
            if (edit_len != e_len)
            {
              steps = std::vector<struct edit_step>(buf[0].insert_val);
              edit_len = e_len;
              recv_part.insert(edit_len, edit_len);
            }
            std::copy(buf.begin() + 1, buf.end(), steps.begin() + buf[0].x);
            recv_part.insert(d_recv, d_recv + size);
            continue;
          }
        }
        // Make sure node 0 does not receive another mesage than the probed one
        MPI_Recv(msg.data(), msg.size(), MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
        assert(status.MPI_TAG != Tag::ReadOutData);
        if (status.MPI_TAG == Tag::ReadOutStopWorkers)
        {
          DEBUG(2, worker_rank << " | received stop signal from " << status.MPI_SOURCE);
          return;
        }
        else if (status.MPI_TAG != Tag::ReadOut)
        {
          // Ignore any old messages.
          continue;
        }
        d = msg.at(0);
        k = msg.at(1);
        int e_len = msg.at(2);
        DEBUG(2, worker_rank << " | Received work. Start calculating (d:" << d << ", k:" << k << ")");
        if (edit_len != e_len)
        {
          steps = std::vector<struct edit_step>(e_len);
          edit_len = e_len;
        }
        break;
      }
    }
    first = false;
    int d_start = d;
    for (; d > 0; d--)
    {
      auto bounds = get_k_bounds(d, worker_rank, num_workers);
      int k_min = bounds.first;
      int k_max = bounds.second;
      DEBUG(2, worker_rank << " | d: " << d << " k: " << k << " k_min:" << k_min << " k_max:" << k_max);
      assert(k_min <= k && k <= k_max);
      if (k == -d || (k != d && results.get(d - 1, k - 1) < results.get(d - 1, k + 1)))
      {
        k = k + 1;
        int x = results.get(d - 1, k);
        int y = x - k;
        int val = in_2.at(y);
        DEBUG(2, "y: " << y << " in_2: " << val);
        steps[d - 1] = {x, val, 1};
      }
      else
      {
        k = k - 1;
        int x = results.get(d - 1, k) + 1;
        steps[d - 1] = {x, -1, 0};
      }
      DEBUG(2, worker_rank << " | Added step x:" << steps[d - 1].x << " val:" << steps[d - 1].insert_val << " mode:" << steps[d - 1].mode);
      bounds = get_k_bounds(d - 1, worker_rank, num_workers);
      k_min = bounds.first;
      k_max = bounds.second;
      DEBUG(2, worker_rank << " | next: d: " << d - 1 << " k: " << k << " k_min: " << k_min << " k_max: " << k_max);
      if (k > k_max || k < k_min)
      {
        int offset = k > k_max ? +1 : -1; // neighbour left or right
        msg = {d - 1, k, edit_len};
        if (worker_rank > 0)
        {
          int length = d_start - d + 1;
          DEBUG(2, worker_rank << " | Sending Data to worker 0 (d:" << d - 1 << " d_start: " << d_start << " length: " << length << ")");
          std::vector<struct edit_step> buf(length + 1);
          buf[0] = {d - 1, edit_len, 2};
          std::copy(steps.begin() + d - 1, steps.begin() + d_start, buf.begin() + 1);
          MPI_Send(buf.data(), buf.size(), MPI_edit_step_t, 0, Tag::ReadOutData, MPI_COMM_WORLD);
        }
        else
        {
          recv_part.insert(d - 1, d_start);
        }
        DEBUG(2, worker_rank << " | Sending message to " << worker_rank + offset << " (" << msg[0] << "," << msg[1] << "," << msg[2] << ")");
        MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank + offset, Tag::ReadOut, MPI_COMM_WORLD);
        break;
      }
    }
    if (d == 0)
    {
      recv_part.insert(d, d_start);
      DEBUG(2, worker_rank << " | All done");
      stop_workers(num_workers, worker_rank, Tag::ReadOutStopWorkers);
      DEBUG(1, worker_rank << " | recv_part size:" << recv_part.size());
      assert(recv_part.size() > 0);
      while (recv_part.size() != 1)
      {
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == Tag::ReadOutData)
        {
          DEBUG(2, worker_rank << " | reading data. Tag: " << status.MPI_TAG);
          int num;
          MPI_Get_count(&status, MPI_edit_step_t, &num);
          std::vector<struct edit_step> buf(num);
          MPI_Recv(buf.data(), buf.size(), MPI_edit_step_t, status.MPI_SOURCE, Tag::ReadOutData, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
          int d_recv = buf[0].x;
          int e_len = buf[0].insert_val;
          int size = buf.size() - 1;
          DEBUG(2, worker_rank << " | Received Data. (d:" << d_recv << ", edit_len:" << e_len << ", mode:" << buf[0].mode << ")");
          assert(buf[0].mode == 2);
          if (edit_len != e_len)
          {
            steps = std::vector<struct edit_step>(buf[0].insert_val);
            edit_len = e_len;
            recv_part.insert(edit_len, edit_len);
          }
          std::copy(buf.begin() + 1, buf.end(), steps.begin() + buf[0].x);
          recv_part.insert(d_recv, d_recv + size);
          continue;
        }
        else
        {
          // TODO: Uhm, what do do in this case? Can this happen?
        }
      }
      t_script_end = std::chrono::high_resolution_clock::now();

      break;
    }
  }

  DEBUG(2, worker_rank << " | Finished reading out");

  if (worker_rank == 0)
  {
    // redirect output
    std::ofstream edit_script_file;
    auto cout_buf = std::cout.rdbuf(); // stdout
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

    for (int i = 0, size = steps.size(); i < size; i++)
    {
      struct edit_step step = steps.at(i);
      assert(!(step.mode == 0) || step.x != 0);
      if (step.mode)
      {
        std::cout << step.x << " + " << step.insert_val << std::endl;
      }
      else
      {
        std::cout << step.x << " -" << std::endl;
      }
    }

    std::cout.rdbuf(cout_buf);
  }

#else

  t_script_start = std::chrono::high_resolution_clock::now();
  t_script_end = t_script_start;

  bool found_solution = false;
  if (edit_len != -1)
  {
    auto bounds = get_k_bounds(edit_len, worker_rank, num_workers);
    int k = in_1.size() - in_2.size();
    if (k >= bounds.first && k <= bounds.second)
    {
      found_solution = true;
    }
  }

  if (worker_rank != 0 && found_solution)
  {
    MPI_Send(&edit_len, 1, MPI_INT, 0, Tag::ReadOutLcsLength, MPI_COMM_WORLD);
  }

  if (worker_rank == 0 && !found_solution)
  {
    MPI_Recv(&edit_len, 1, MPI_INT, MPI_ANY_SOURCE, Tag::ReadOutLcsLength, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }

#endif

  if (worker_rank == 0)
  {
    assert(edit_len >= 0);
    std::cout << "\nmin edit length " << edit_len << std::endl
              << std::endl;
    std::cout << "mpi comm_world: " << comm_size << std::endl;
    std::cout << "Read Input [μs]: \t" << std::chrono::duration_cast<std::chrono::microseconds>(t_in_end - t_in_start).count() << std::endl;
    std::cout << "Precompute [μs]: \t" << 0 << std::endl;
    std::cout << "Solution [μs]:   \t" << std::chrono::duration_cast<std::chrono::microseconds>(t_sol_end - t_sol_start).count() << std::endl;
    std::cout << "Edit Script [μs]: \t" << std::chrono::duration_cast<std::chrono::microseconds>(t_script_end - t_script_start).count() << std::endl;
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
    std::cerr << "args: file1 file2 (edit_script_file) (-min_entries 20)" << std::endl;
    exit(1);
  }

  path_1 = argv[1];
  path_2 = argv[2];

  // parse additional args
  if (argc >= 4)
  {
    for (int i = 3; i < argc; i++)
    {
      // check if any arg is -min_entires X
      if (std::strcmp(argv[i], "-min_entries") == 0)
      {
        MIN_ENTRIES = std::atoi(argv[++i]); // skip next arg
      } 
      else
      {
        edit_script_path = argv[i];
        edit_script_to_file = true;
      }
      
    }
  }
    

  MPI_Init(nullptr, nullptr);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  main_worker(path_1, path_2, edit_script_to_file, edit_script_path);

  DEBUG(2, world_rank << " | "
                      << "EXITING\n");
  MPI_Finalize();
  return 0;
}
