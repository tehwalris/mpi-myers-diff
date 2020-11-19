#include <mpi.h>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <chrono>

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

inline void send_result(int d, int k, int x){

}

inline void stop_master(){

}

void main_master(const std::string path_1, const std::string path_2)
{
  DEBUG(2, "started master");

  // start TIMER
  auto chrono_start = std::chrono::high_resolution_clock::now();


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
  const int MIN_ENTRIES = 3; // min. no. of initial entries to compute on one node before the next node is started

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

  // INITIAL PHASE
  // first worker works alone until it reaches a point where one layer contains MIN_ENTRIES 
  

  // GROW INDIVIDUAL WORKERS
  // after a worker has passed their initial phase, we add an additional worker

  // mine

  
  // ALL WORKERS ACTIVE
  // BALANCING

  
  int own_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &own_rank);

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
