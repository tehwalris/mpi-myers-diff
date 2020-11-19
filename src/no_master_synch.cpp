#include <mpi.h>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <chrono>

// Uncomment this line when performance is measured
//#define NDEBUG

const int debug_level = 2;

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

enum Tag
{
  AssignWork,
  ReportWork,
};

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
        DEBUG(3, "PYRAMID: d_max:" << m_d_max << " d:" << d << " k:" << k << " start:" << start << " access:" << access);
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

inline int compute_entry(int d, int k, Results &data){
  int x = 0;
  return x;
}

// send result entry to the master
inline void send_result(int d, int k, int x){

}

// a worker has reached the end of the input and the master should stop waiting for additional messages.
inline void stop_master(){

}

void main_master(const std::string path_1, const std::string path_2)
{
  DEBUG(2, "started master");

  // start TIMER
  auto chrono_start = std::chrono::high_resolution_clock::now();

  std::vector<int> in_1, in_2;
  read_file(path_1, in_1);
  read_file(path_2, in_2);

  int comm_size;
  MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
  assert(comm_size > 1);
  int num_workers = comm_size - 1;

  DEBUG(2, "sending inputs");
  auto send_vector = [](const std::vector<int> &vec) {
    int temp_size = vec.size();
    MPI_Bcast(&temp_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast((void *)(vec.data()), vec.size(), MPI_INT, 0, MPI_COMM_WORLD);
  };
  send_vector(in_1);
  send_vector(in_2);


done:
  std::cout << "min edit length " << edit_len << std::endl;

  DEBUG(2, "shutting down workers");
  {
    std::vector<int> msg{shutdown_sentinel, 0, 0};
    for (int i = 1; i < comm_size; i++)
    {
      MPI_Send(msg.data(), msg.size(), MPI_INT, i, Tag::AssignWork, MPI_COMM_WORLD);
    }
  }
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
    for(int i=0; i < steps.size(); i++){
        struct Edit_step step = steps.at(i);
        if(step.mode){
            std::cout << step.x << " + " << step.insert_val << std::endl;
        } else  {
            std::cout << step.x << " -" << std::endl;
        }
    }
}

void main_worker()
{
  const int MIN_ENTRIES = 3; // min. number of initial entries to compute on one node per layer d before the next node is started

  int worker_rank;
  int worker_comm_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &worker_rank);

  int comm_size;
  MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
  assert(comm_size > 1);
  int num_workers = comm_size - 1; // without master

  DEBUG(2, worker_rank << " | "
                    << "receiving inputs");
  auto receive_vector = []() {
    int temp_size;
    MPI_Bcast(&temp_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    std::vector<int> vec(temp_size);
    MPI_Bcast((void *)(vec.data()), vec.size(), MPI_INT, 0, MPI_COMM_WORLD);
    return vec;
  };
  std::vector<int> in_1 = receive_vector();
  std::vector<int> in_2 = receive_vector();

  int d_max = in_1.size() + in_2.size() + 1;
  Results results(d_max);


  // 2b) grow last workload 1-by-1 requiring less initial communication
  // rank=1:
  // start at d=0, k=0
  // grow until d=MIN_ENTRIES: [-d, +d]
  // then compute only k=[-d, -d + 2*(MIN_ENTRIES-1)]
  // rank=2:
  // start at d=MIN_ENTRIES, k = [MIN_ENTRIES, MIN_ENTRIES] = [d, d]
  // grow until d=2*MIN_ENTRIES: k=[-d + 2*MIN_ENTRIES, d]
  // then compute only k=[-d + 2*MIN_ENTRIES, -d + 2*MIN_ENTRIES + 2*(MIN_ENTRIES-1)]

  // rank=worker_rank:
  // start at d=(worker_rank-1)*MIN_ENTRIES, k = [(worker_rank-1)*MIN_ENTRIES, (worker_rank-1)*MIN_ENTRIES] = [d,d]
  // grow until d=worker_rank*MIN_ENTRIES-1: k=[-d + 2*((worker_rank-1)*MIN_ENTRIES), d]		// only need to receive from worker_rank-1, no need to send

  
  int d_start = (worker_rank-1)*MIN_ENTRIES;
  int k_min = d_start;
  int k_max = d_start;

  // INITIAL PHASE
  // first worker starts alone until it reaches point where one layer contains MIN_ENTRIES
  if (worker_rank != 1) {
    // Receive (d_start-1, k_min-1) from worker_rank-1
  }
  
  

  for(int d = d_start; d < worker_rank*MIN_ENTRIES; ++d) {
    for (int k=k_min; k <= k_max; ++k) {
      // compute entry (d,k) // Test for d=0 or add dummy entry for first worker!
    }
    if (worker_rank != 1){
      // Receive entry (d, k_min-2) from worker_rank-1 for next round
    }
    k_min--;
    k_max++;
  }

  // GROW INDIVIDUAL WORKERS
  // after a worker has passed their initial phase, we add an additional worker

  //then compute only k=[-d + 2*((worker_rank-1)*MIN_ENTRIES), -d + 2*((worker_rank-1)*MIN_ENTRIES) + 2*(MIN_ENTRIES-1)]			// #entries = MIN_ENTRIES
  //			= [-d + 2*((worker_rank-1)*MIN_ENTRIES), -d + 2*(worker_rank*MIN_ENTRIES -1)]
  // == third (else) case below

  // UNTIL: d = worker_comm_size*MIN_ENTRIES - 1 -> all nodes have MIN_ENTRIES to compute

  int x;
  for (int d = worker_rank*MIN_ENTRIES; d < worker_comm_size * MIN_ENTRIES; ++d) {
    // compute (d, k_max)

    // Send entry (d, k_max) to worker_rank+1
    send_result(d, k_max, x);

    for (int k = k_min; k < k_max-2; k++){
      // compute (d,k)
    }
    // Receive entry (d, k_min-2) from worker_rank-1

    // new range for next round d+1
    k_min--;	//extend	// -d has decreased by 1, same number of entries on the left
    k_max--;	//decrease	// -d has decreased by 1, same total number of entries at this node
  }

 

  // ALL WORKERS ACTIVE
  // BALANCING

  

  DEBUG(2, own_rank << " | "
                    << "worker exiting");
}

int main(int argc, char *argv[])
{
  std::ios_base::sync_with_stdio(false);

  std::string path_1, path_2;

  if (argc < 3)
  {
    std::cerr << "You must provide two paths to files to be compared as arguments" << std::endl;
    exit(1);
  }
  else
  {
    path_1 = argv[1];
    path_2 = argv[2];
  }

  MPI_Init(nullptr, nullptr);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  if (world_rank == 0)
  {
    main_master(path_1, path_2);
  }
  else
  {
    main_worker();
  }

  MPI_Finalize();
  return 0;
}
