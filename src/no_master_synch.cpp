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


const int shutdown_sentinel = 1;
const int unknown_len = -1;
const int no_worker_rank = 0;

enum Tag
{
  ResultEntry = 0,
  StopMaster = 1,
  StopWorkers = 2,
};

// tag for layer d
int tag(int d){
  return d + 2;
}

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

// compute entry x and add it to the data structure
// returns true if it found the solution
inline bool compute_entry(int d, int k, Results &data, std::vector<int> &in_1, std::vector<int> &in_2){
  int x;
  if (d == 0){
      x = 0;
  } else if (k == -d || k != d && data.result_at(d - 1, k - 1) < data.result_at(d - 1, k + 1)){
      x = data.result_at(d - 1, k + 1);
  } else {
      x = data.result_at(d - 1, k - 1) + 1;
  }

  int y = x - k;

  while (x < in_1.size() && y < in_2.size() && in_1.at(x) == in_2.at(y)){
      x++;
      y++;
  }


  DEBUG_NO_LINE(2, "(" << d << ", " << k << "): ");
  DEBUG_NO_LINE(2, "x: " << x);
  DEBUG(2, ", y; " << y);

  data.result_at(d, k) = x;

  // LCS found
  if (x >= in_1.size() && y >= in_2.size()){
    return true; 
  }


  return false;
}

// send result entry to the master
void send_result(int d, int k, int x){

}

// a worker has reached the end of the input and the master should stop waiting for additional messages.
void stop_master(int edit_len){
  DEBUG(2, "Sending stop to master.");

  // send edit_len to master
  std::vector<int> msg_sol{edit_len};
  MPI_Send(msg_sol.data(), msg_sol.size(), MPI_INT, 0, Tag::StopMaster, MPI_COMM_WORLD);
}

// blocking wait to receive a message from worker_rank-1 for layer d
// updates the entires in Results
// returns true if StopWorkers received
bool Recv(int source_rank, int d, Results &results){
    MPI_Status status;
    std::vector<int> msg(3);

    int d_rcv = -1;
    while (d_rcv != d){ // epxected value
      MPI_Recv(msg.data(), msg.size(), MPI_INT, source_rank, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

      if (status.MPI_TAG == Tag::StopWorkers){
        return true;
      }

      // save result
      d_rcv = msg.at(0);
      k_rcv = msg.at(1);
      x = msg.at(2);
      results.result_at(d_rcv, k_rcv) = x;
    }

    return false;
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


  // receive solution
  std::vector<int> msg(1);
  MPI_Recv(msg.data(), msg.size(), MPI_INT, MPI_ANY_SOURCE, Tag::StopMaster, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  int edit_len = msg.at(0);

  std::cout << "min edit length " << edit_len << std::endl;

  DEBUG(2, "shutting down workers");
  {
    for (int i = 1; i < comm_size; i++)
    {
      MPI_Send(&shutdown_sentinel, 1, MPI_INT, i, Tag::StopWorkers, MPI_COMM_WORLD);
    }
  }
    // std::vector<struct Edit_step> steps(edit_len);
    // int k = in_1.size() - in_2.size();
    // for(int d = edit_len; d > 0; d--){
    //     if (k == -d || k != d && results.result_at(d - 1, k - 1) < results.result_at(d - 1, k + 1))
    //     {
    //         k = k + 1;
    //         int x = results.result_at(d - 1, k);
    //         int y = x - k;
    //         int val = in_2.at(y);
    //         DEBUG(2, "y: " << y << " in_2: " << val);
    //         steps[d-1] = {x, val, true};
    //     } else {
    //         k = k - 1;
    //         int x = results.result_at(d - 1, k) + 1;
    //         steps[d-1] = {x, -1, false};
    //     }
    // }
    // for(int i=0; i < steps.size(); i++){
    //     struct Edit_step step = steps.at(i);
    //     if(step.mode){
    //         std::cout << step.x << " + " << step.insert_val << std::endl;
    //     } else  {
    //         std::cout << step.x << " -" << std::endl;
    //     }
    // }
}

void main_worker()
{
  const int MIN_ENTRIES = 3; // min. number of initial entries to compute on one node per layer d before the next node is started

  int worker_rank;
  int comm_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &worker_rank);
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

  /**
   * TODO shutdown waiting problem
   * If we find the solution during the grow phase, it means that some workers are still (blocking) waiting on their
   * initial message to start computation. How does the shutdown message reach them?
   **/


  // listen for shutdown message
  int buffer;
  int shutdown_flag = 0;
  MPI_Request shutdown_request;
  MPI_Status status;
  MPI_Irecv(&buffer, 1, MPI_INT, 0, Tag::StopWorkers, MPI_COMM_WORLD, &shutdown_request);
  
  int d_start = (worker_rank-1)*MIN_ENTRIES; // first included layer
  int d_end = num_workers * MIN_ENTRIES - 1; // last included layer
  int k_min = d_start;                       // left included k index
  int k_max = d_start;                       // right included k index


  // WAITING on first required input
  int x, d_rcv, k_rcv;
  if (worker_rank != 1) {
    // Receive (d_start-1, k_min-1) from worker_rank-1
    std::vector<int> msg(3);
    DEBUG(2, worker_rank << " | WAIT for " << worker_rank-1 << " ("<<d_start-1<<", "<<k_min-1<<")");
    MPI_Recv(msg.data(), msg.size(), MPI_INT, worker_rank-1, tag(d_start-1), MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    d_rcv = msg.at(0);
    k_rcv = msg.at(1);
    x = msg.at(2);

    results.result_at(d_rcv, k_rcv) = x;
    DEBUG(2, worker_rank << " | "
      << "receiving (" << d_rcv <<", "<< k_rcv <<"): "<< x);
  }
  
  // INITIAL PHASE
  // first worker starts alone on its pyramid until it reaches point where one layer contains MIN_ENTRIES
  for(int d = d_start; d < worker_rank*MIN_ENTRIES; ++d) {
    for (int k=k_min; k <= k_max; k+=2) {
      // compute entry (d,k) // Test for d=0 or add dummy entry for first worker!
      DEBUG_NO_LINE(2, worker_rank << " | ");
      bool done = compute_entry(d, k, results, in_1, in_2);
      if (done) {
        stop_master(d);
        return;
      }
    }
    // // check for shutdown
    // MPI_Test(&shutdown_request, &shutdown_flag, MPI_STATUS_IGNORE);
    // if (shutdown_flag) return;

    // Receive entry (d, k_min-2) from worker_rank-1 for next round
    if (worker_rank != 1 && d < d_end){
      std::vector<int> msg(3);
      DEBUG(2, worker_rank << " | WAIT for " << worker_rank-1 << " ("<<d<<", "<<k_min-2<<")");
      MPI_Recv(msg.data(), msg.size(), MPI_INT, worker_rank-1, tag(d), MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      d_rcv = msg.at(0);
      k_rcv = msg.at(1);
      x = msg.at(2);
      assert(k_rcv == k_min-2);
      results.result_at(d_rcv, k_rcv) = x;
    }

    k_min--;
    k_max++;
  }


  // send bottom right edge node to next worker
  int d_bot_right = worker_rank*(MIN_ENTRIES) - 1;
  if (worker_rank != num_workers){
    x = results.result_at(d_bot_right, d_bot_right);
    std::vector<int> msg{d_bot_right, d_bot_right, x};
    DEBUG(2, worker_rank << " | Send to " << worker_rank+1 << " ("<<d_bot_right<<", "<<d_bot_right<<")");
    MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank+1, tag(d_bot_right), MPI_COMM_WORLD);
  }

  // GROW INDIVIDUAL WORKERS
  // after a worker has passed their initial phase, we add additional workers
  // Each worker computes MIN_ENTRIES nodes per layer and shifts to the left with every additional layer.
  k_max = worker_rank*(MIN_ENTRIES) - 2;
  for (int d = worker_rank*MIN_ENTRIES; d < num_workers * MIN_ENTRIES; ++d) {

    // compute (d, k_max)
    DEBUG_NO_LINE(2, worker_rank << " | ");
    bool done = compute_entry(d, k_max, results, in_1, in_2);
    x = results.result_at(d, k_max);
    if (done) {
      stop_master(d);
      return;
    }    

    // Send right entry (d, k_max) to worker_rank+1
    // except right-most worker never sends and we are not in the last layer of the growth phase.
    if (worker_rank != num_workers && d < d_end){
      std::vector<int> msg{d, k_max, x};
      DEBUG(2, worker_rank << " | Send to " << worker_rank+1 << " ("<<d<<", "<<k_max<<")");
      MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank+1, tag(d), MPI_COMM_WORLD);
    }

    for (int k = k_min; k < k_max; k+= 2){

      // compute (d,k)
      DEBUG_NO_LINE(2, worker_rank << " | ");
      bool done = compute_entry(d, k, results, in_1, in_2);
      x = results.result_at(d, k);
      if (done) {
        stop_master(d);
        return;
      }      
    }

    // Receive entry on left of row (d, k_min-2) from worker_rank-1
    // except worker 1 never receives and doesn't apply to very last row of growth phase.
    if (worker_rank != 1 && d < d_end){
      std::vector<int> msg(3);
      DEBUG(2, worker_rank << " | WAIT for " << worker_rank-1 << " ("<<d<<", "<<k_min-2<<")");
      MPI_Recv(msg.data(), msg.size(), MPI_INT, worker_rank-1, tag(d), MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      d_rcv = msg.at(0);
      k_rcv = msg.at(1);
      x = msg.at(2);
      results.result_at(d_rcv, k_rcv) = x;
    }

    // new range for next round d+1
    k_min--;    //extend    // -d has decreased by 1, same number of entries on the left
    k_max--;    //decrease  // -d has decreased by 1, same total number of entries at this node
  }
 
  // Correct range for new phase
  int new_k_min = k_min+1;
  int new_k_max = k_max+1;

  // ALL WORKERS ACTIVE
  // BALANCING
  for(int d=num_workers * MIN_ENTRIES; d < d_max; ++d) {
    k_min = new_k_min;
    k_max = new_k_max;
    if ((d) % num_workers + 1 < worker_rank) {
      // Send entry (d, k_min) to worker_rank-1
      std::vector<int> msg{d, k_max, x};
      DEBUG(2, worker_rank << " | Send to " << worker_rank-1 << " ("<<d<<", "<<k_max<<")");
      MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank-1, tag(d), MPI_COMM_WORLD);

      if(worker_rank != num_workers){
        // Receive entry (d, k_max+2) from worker_rank+1  //TODO: receive later, only when needed?
        DEBUG(2, worker_rank << " | WAIT for " << worker_rank+1 << " ("<<d<<", "<<k_max+2<<")");
        MPI_Recv(msg.data(), msg.size(), MPI_INT, worker_rank+1, tag(d), MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        d_rcv = msg.at(0);
        k_rcv = msg.at(1);
        x = msg.at(2);
        results.result_at(d_rcv, k_rcv) = x;
      }

      // new range for next round d+1
      new_k_min = k_min+1;  //decrease  // -d has decreased by 1, but one more entry on the left -> +2
      new_k_max = k_max+1;  //extend
    } else if ((d) % num_workers == worker_rank) {
      std::vector<int> msg(3);
      if(worker_rank > 1){
        // Receive entry (d, k_min-2) from worker_rank-1
        DEBUG(2, worker_rank << " | WAIT for " << worker_rank-1 << " ("<<d<<", "<<k_min-2<<")");
        MPI_Recv(msg.data(), msg.size(), MPI_INT, worker_rank-1, tag(d), MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        d_rcv = msg.at(0);
        k_rcv = msg.at(1);
        x = msg.at(2);
        results.result_at(d_rcv, k_rcv) = x;
      }
      // Receive entry (d, k_max+2) from worker_rank+1
      DEBUG(2, worker_rank << " | WAIT for " << worker_rank+1 << " ("<<d<<", "<<k_max+2<<")");
      MPI_Recv(msg.data(), msg.size(), MPI_INT, worker_rank+1, tag(d), MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      d_rcv = msg.at(0);
      k_rcv = msg.at(1);
      x = msg.at(2);
      results.result_at(d_rcv, k_rcv) = x;

      // new range for next round d+1
      new_k_min = k_min-1;  //extend    // -d has decreased by 1, same number of entries on the left
      new_k_max = k_max+1;  //extend    // 1 additional entry -> += 2
    } else {

      // Send entry (d, k_max) to worker_rank+1
      std::vector<int> msg{d, k_max, x};
      DEBUG(2, worker_rank << " | Send to " << worker_rank-1 << " ("<<d<<", "<<k_max<<")");
      MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank-1, tag(d), MPI_COMM_WORLD);

      // Receive entry (d, k_min-2) from worker_rank-1
      if(worker_rank > 1){
        DEBUG(2, worker_rank << " | WAIT for " << worker_rank-1 << " ("<<d<<", "<<k_min-2<<")");
        MPI_Recv(msg.data(), msg.size(), MPI_INT, worker_rank-1, tag(d), MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        d_rcv = msg.at(0);
        k_rcv = msg.at(1);
        x = msg.at(2);
        results.result_at(d_rcv, k_rcv) = x;
      }

      // new range for next round d+1
      new_k_min = k_min-1;  //extend    // -d has decreased by 1, same number of entries on the left
      new_k_max = k_max-1;  //decrease  // -d has decreased by 1, no additional entry
    }
    // TODO: Priorize entries
    for (int k=k_min; k < k_max; k+=2) {
      DEBUG_NO_LINE(2, worker_rank << " | ");
      bool done = compute_entry(d, k, results, in_1, in_2);
      if (done) {
        stop_master(d);
        return;
      }
    }

  }

  DEBUG(2, worker_rank << " | "
                    << "EXITING");
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
