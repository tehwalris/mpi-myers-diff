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

// Uncomment this line when performance is measured
#define NDEBUG

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
  ResultEntryData = 3,
  StopMaster = 1,
  StopWorkers = 2,
};

class Results{
private:
    int alloc_n_layers = 20;
    std::vector<int*> data_pointers;

    // allocate the data block that begins at layer index d_begin
    int* allocate_block(int d_begin){
      int d_max = d_begin + alloc_n_layers;
      int size = pyramid_size(d_max) - pyramid_size(d_begin);
      DEBUG(3, "PYRAMID: allocate new blocks for layer "<<d_begin<<" to "<<d_max-1<<" of size "<< size);
      
      return new int[size](); // initialized to 0
    }

    // total number of elements in a pyramid with l layers
    inline int pyramid_size(int l){
      return (l*(l+1))/2;
    }

public:
    int m_d_max;
    int num_blocks;

    Results(int d_max){
        this->m_d_max = d_max;

        // allocate initial block 0
        this->num_blocks = std::max(1, d_max / alloc_n_layers);
        data_pointers.resize(num_blocks, nullptr);
        data_pointers[0] = allocate_block(0); 
    }

    int &result_at(int d, int k){
        DEBUG(3, "PYRAMID: ("<<d<<", "<<k<<")");
        int block_idx = d / alloc_n_layers;

        // allocate new block if needed
        if (data_pointers.at(block_idx) == nullptr){
          DEBUG(3, "PYRAMID: Block is null "<<block_idx);
          data_pointers.at(block_idx) = allocate_block(block_idx*alloc_n_layers);
        }

        int start_d = pyramid_size(d) - pyramid_size(block_idx*alloc_n_layers);
        int offset = (k+d)/2;

        DEBUG(3, "PYRAMID: ("<<d<<", "<<k<<")   block: "<<block_idx<<" start:" << start_d << " offset:" << offset);
        assert(d < this->m_d_max);
        assert(offset >= 0 && offset <= d+1);

        return data_pointers[block_idx][start_d+offset];
    }

    // returns pointer to first that element.
    // is guaranteed to continue for at least end of layer d.
    // unlike result_at(d,k) does not perform allocation of next layers
    int* get_pointer(int d, int k){
        int block_idx = d / alloc_n_layers;
        int start_d = pyramid_size(d) - pyramid_size(block_idx*alloc_n_layers);
        int offset = (k+d) / 2;

        return data_pointers[block_idx] + start_d + offset;
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
    for (size_t i = 0, size = vec.size(); i < size; i++)
    {
        if (i != 0)
        {
            DEBUG_NO_LINE(2, " ");
        }
        DEBUG_NO_LINE(2, vec.at(i));
    }
    DEBUG_NO_LINE(2, std::endl);
}


void print_vector_colored(const std::vector<int> &vec, int color_index)
{
    std::ostringstream oss;
    oss << "\33[9" << color_index << "m";
    for (size_t i = 0, size = vec.size(); i < size; i++)
    {
        if (i != 0)
        {
            oss << " ";
        }
        oss << vec.at(i);
    }
    oss << "\33[0m";
    std::string s = oss.str();
    DEBUG(2, s);
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

// send result data row to the master
// Sends the whole layer
// returns true if the message can not be delivered, as master is no longer receiving data
bool send_result(int d, int k_min, int k_max, Results &results){
    // Prevent deadlocking on send when master already stopped receiving
    // Still does not work...
    int has_message = false;
    MPI_Iprobe(0, Tag::StopWorkers, MPI_COMM_WORLD, &has_message, MPI_STATUS_IGNORE);
    if(has_message){
        return true;
    }
    std::vector<int> msg_sol{d, k_min, k_max};
    DEBUG(2, "Sending results");
    int entries = (k_max - k_min)/2+1;
    int* start = results.get_pointer(d,k_min);
    MPI_Send(msg_sol.data(), msg_sol.size(), MPI_INT, 0, Tag::ResultEntry, MPI_COMM_WORLD);
    MPI_Send(start, entries, MPI_INT, 0, Tag::ResultEntryData, MPI_COMM_WORLD);
    return false;
}

// a worker has reached the end of the input and the master should stop waiting for additional messages.
void stop_master(int edit_len){
  DEBUG(2, "Sending stop to master.");

  // send edit_len to master
  std::vector<int> msg_sol{edit_len};
  MPI_Send(msg_sol.data(), msg_sol.size(), MPI_INT, 0, Tag::StopMaster, MPI_COMM_WORLD);
}

// compute entry x and add it to the data structure
// returns true if it found the solution
inline bool compute_entry(int d, int k, Results &data, std::vector<int> &in_1, std::vector<int> &in_2){

  int in1_size = in_1.size();
  int in2_size = in_2.size();

  int x;
  if (d == 0){
      x = 0;
  } else if (k == -d || (k != d && data.result_at(d - 1, k - 1) < data.result_at(d - 1, k + 1))){
      x = data.result_at(d - 1, k + 1);
  } else {
      x = data.result_at(d - 1, k - 1) + 1;
  }

  int y = x - k;

  while (x < in1_size && y < in2_size && in_1.at(x) == in_2.at(y)){
      x++;
      y++;
  }


  DEBUG_NO_LINE(2, "(" << d << ", " << k << "): ");
  DEBUG_NO_LINE(2, "x: " << x);
  DEBUG(2, ", y; " << y);

  data.result_at(d, k) = x;

  // LCS found
  if (x >= in1_size && y >= in2_size){
    return true; 
  }


  return false;
}

// blocking wait to receive a message from worker_rank-1 for layer d
// updates the entires in Results
// returns true if StopWorkers received
bool Recv(int source_rank, int d, int k, Results &results, int worker_rank /*only for debug output*/){
    MPI_Status status;
    std::vector<int> msg(3);
    int k_rcv, x;

    if(d >= results.m_d_max){
        // This worker will not be needed as its d is bigger than the maximum allowed one
        return true;
    }
    
    // check if already received
    x = results.result_at(d, k);
    if (x > 0) return false;

    DEBUG(2, worker_rank << " | WAIT for " << source_rank << " ("<< d << ", "<< k << ")");
    int d_rcv = -1;
    while (d_rcv != d){ // epxected value

      MPI_Recv(msg.data(), msg.size(), MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      if (status.MPI_TAG == Tag::StopWorkers){
        return true;
      }

      // save result
      d_rcv = msg.at(0);
      k_rcv = msg.at(1);
      x = msg.at(2);
      results.result_at(d_rcv, k_rcv) = x;

      DEBUG(2, worker_rank << " | " << "receiving from "<< status.MPI_SOURCE <<": (" << d_rcv <<", "<< k_rcv <<"): "<< x);
      assert(x > 0);
    }

    return false;
}

void main_master(const std::string path_1, const std::string path_2)
{
  DEBUG(2, "started master");

  // start TIMER
  auto time_start = std::chrono::high_resolution_clock::now();

  std::vector<int> in_1, in_2;
  read_file(path_1, in_1);
  read_file(path_2, in_2);

  int comm_size;
  MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
  assert(comm_size > 1);

  DEBUG(2, "sending inputs");
  auto send_vector = [](const std::vector<int> &vec) {
    int temp_size = vec.size();
    MPI_Bcast(&temp_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast((void *)(vec.data()), vec.size(), MPI_INT, 0, MPI_COMM_WORLD);
  };
  send_vector(in_1);
  send_vector(in_2);

  int d_max = in_1.size() + in_2.size() + 1;
  Results results(d_max);
  std::vector<int> msg_buffer(3);
  MPI_Status msg_status;
  int edit_len = -1;
  
  // receive solution
  std::vector<int> rcv_buf;
  while(true){
    MPI_Recv(msg_buffer.data(), msg_buffer.size(), MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &msg_status);
    if(msg_status.MPI_TAG == Tag::StopMaster){
      edit_len = msg_buffer.at(0);
      break;
    }
    int d_rcv = msg_buffer.at(0);
    int k_min_rcv = msg_buffer.at(1);
    int k_max_rcv = msg_buffer.at(2);
    rcv_buf.resize((k_max_rcv - k_min_rcv)/2+1);
    MPI_Recv(rcv_buf.data(), rcv_buf.size(), MPI_INT, msg_status.MPI_SOURCE, Tag::ResultEntryData, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // print_vector_colored(rcv_buf, 4);
    std::copy(rcv_buf.data(), rcv_buf.data()+rcv_buf.size(), &results.result_at(d_rcv, k_min_rcv));
    std::ostringstream oss;
    oss << "\33[94m" << "MASTER | Received from " << msg_status.MPI_SOURCE << " (d:"<<d_rcv<<", k_min:"<<k_min_rcv<<", k_max:"<<k_max_rcv<<")" << "\33[0m";
    std::string s = oss.str();
    DEBUG(2, s);
  }

  // TIMER for solution
  auto time_sol_start = std::chrono::high_resolution_clock::now();
  auto time_sol = std::chrono::duration_cast<std::chrono::microseconds>(time_sol_start - time_start).count();


  DEBUG(2, "shutting down workers");
  {
    for (int i = 1; i < comm_size; i++)
    {
      std::vector<int> msg{shutdown_sentinel, -1, -1};
      MPI_Send(msg.data(), msg.size(), MPI_INT, i, Tag::StopWorkers, MPI_COMM_WORLD);
    }
  }

  std::vector<struct Edit_step> steps(edit_len);
  int k = in_1.size() - in_2.size();
  for(int d = edit_len; d > 0; d--){
    if (k == -d || (k != d && results.result_at(d - 1, k - 1) < results.result_at(d - 1, k + 1)))
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

  // TIMER edit script
  auto time_edit_start = std::chrono::high_resolution_clock::now();
  auto time_edit = std::chrono::duration_cast<std::chrono::microseconds>(time_edit_start - time_sol_start).count();

  for(int i=0, size = steps.size(); i < size; i++){
    struct Edit_step step = steps.at(i);
    if(step.mode){
      std::cout << step.x << " + " << step.insert_val << std::endl;
    } else  {
      std::cout << step.x << " -" << std::endl;
    }
  }
  //print_vector(results.m_data);

  std::cout << "Solution [μs]: \t\t" << time_sol << std::endl;
  std::cout << "Edit Script [μs]: \t" << time_edit << std::endl << std::endl;
  std::cout << "min edit length " << edit_len << std::endl;

}

void main_worker()
{
  // This number must be greater or equal to 2
  const int MIN_ENTRIES = 100; // min. number of initial entries to compute on one node per layer d before the next node is started

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

  
  int d_start = (worker_rank-1)*MIN_ENTRIES; // first included layer
  int d_end = num_workers * MIN_ENTRIES - 1; // last included layer
  int k_min = d_start;                       // left included k index
  int k_max = d_start;                       // right included k index
  int d_bot_right = worker_rank*(MIN_ENTRIES) - 1;


  // WAITING on first required input
  if (worker_rank != 1) {
    // Receive (d_start-1, k_min-1) from worker_rank-1
    if (Recv(worker_rank-1, d_start-1, k_min-1, results, worker_rank)) return;
  }
  
  // INITIAL PHASE
  // first worker starts alone on its pyramid until it reaches point where one layer contains MIN_ENTRIES
  for(int d = d_start; d < std::min(worker_rank*MIN_ENTRIES, d_max); ++d) {
    for (int k=k_min; k <= k_max; k+=2) {
      // compute entry (d,k) // Test for d=0 or add dummy entry for first worker!
      DEBUG_NO_LINE(2, worker_rank << " | ");
      bool done = compute_entry(d, k, results, in_1, in_2);
      if (done) {
        send_result(d, k_min, k_max, results);
        stop_master(d);
        return;
      }
    }
    bool done = send_result(d, k_min, k_max, results);
    if(done)
        return;
    // // check for shutdown
    // MPI_Test(&shutdown_request, &shutdown_flag, MPI_STATUS_IGNORE);
    // if (shutdown_flag) return;

    // Receive entry (d, k_min-2) from worker_rank-1 for next round
    if (worker_rank != 1 && d < d_end){
      if (Recv(worker_rank-1, d, k_min-2, results, worker_rank)) return;
    }

    k_min--;
    k_max++;
  }


  // send bottom right edge node to next worker
  int x;
  if (worker_rank != num_workers){
    x = results.result_at(d_bot_right, d_bot_right);
    std::vector<int> msg{d_bot_right, d_bot_right, x};
    DEBUG(2, worker_rank << " | Send to " << worker_rank+1 << " ("<<d_bot_right<<", "<<d_bot_right<<")");
    MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank+1, Tag::ResultEntry, MPI_COMM_WORLD);
  }
  DEBUG(2, worker_rank << " | spawned new worker");
  // GROW INDIVIDUAL WORKERS
  // after a worker has passed their initial phase, we add additional workers
  // Each worker computes MIN_ENTRIES nodes per layer and shifts to the left with every additional layer.
  k_max = worker_rank*(MIN_ENTRIES) - 2;
  for (int d = worker_rank*MIN_ENTRIES; d < std::min(num_workers * MIN_ENTRIES, d_max); ++d) {

    // compute (d, k_max)
    DEBUG_NO_LINE(2, worker_rank << " | ");
    bool done = compute_entry(d, k_max, results, in_1, in_2);
    x = results.result_at(d, k_max);
    if (done) {
      send_result(d, k_min, k_max, results);
      stop_master(d);
      return;
    }    

    // Send right entry (d, k_max) to worker_rank+1
    // except right-most worker never sends and we are not in the last layer of the growth phase.
    if (worker_rank != num_workers && d < d_end){
      DEBUG(2, worker_rank << " | Send to " << worker_rank+1 << " ("<<d<<", "<<k_max<<")");
      std::vector<int> msg{d, k_max, x};
      MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank+1, Tag::ResultEntry, MPI_COMM_WORLD);
    }

    for (int k = k_min; k < k_max; k+= 2){

      // compute (d,k)
      DEBUG_NO_LINE(2, worker_rank << " | ");
      bool done = compute_entry(d, k, results, in_1, in_2);
      x = results.result_at(d, k);
      if (done) {
        send_result(d, k_min, k_max, results);
        stop_master(d);
        return;
      }      
    }
    done = send_result(d, k_min, k_max, results);
    if(done)
        return;
    // Receive entry on left of row (d, k_min-2) from worker_rank-1
    // except worker 1 never receives and doesn't apply to very last row of growth phase.
    if (worker_rank != 1 && d < d_end){
      if (Recv(worker_rank-1, d, k_min-2, results, worker_rank)) return;
    }

    // new range for next round d+1
    k_min--;    //extend    // -d has decreased by 1, same number of entries on the left
    k_max--;    //decrease  // -d has decreased by 1, same total number of entries at this node
  }
 
  // Correct range for new phase
  k_min++;
  k_max++;

  DEBUG(2, worker_rank << " | All workers active");

  // ALL WORKERS ACTIVE
  // BALANCING
  for(int d=num_workers * MIN_ENTRIES; d < d_max; ++d) {
    if ((d) % num_workers + 1 < worker_rank) {
      // Send entry (d, k_min) to worker_rank-1
      x = results.result_at(d-1, k_min);
      std::vector<int> msg{d-1, k_min, x};
      DEBUG(2, worker_rank << " | Send to " << worker_rank-1 << " ("<<d-1<<", "<<k_min<<", "<<x<<")");
      MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank-1, Tag::ResultEntry, MPI_COMM_WORLD);
      assert(worker_rank != 1);
      if(worker_rank != num_workers){
        // Receive entry (d, k_max+2) from worker_rank+1  //TODO: receive later, only when needed?
        if (Recv(worker_rank+1, d-1, k_max+2, results, worker_rank)) return;
      }

      // new range for next round d+1
      k_min++;  //decrease  // -d has decreased by 1, but one more entry on the left -> +2
      k_max++;  //extend
    } else if ((d) % num_workers + 1 == worker_rank) {
      std::vector<int> msg(3);
      if(worker_rank > 1){
        // Receive entry (d, k_min-2) from worker_rank-1
        if (Recv(worker_rank-1, d-1, k_min-2, results, worker_rank)) return;

      }
      if(worker_rank != num_workers){
        // Receive entry (d, k_max+2) from worker_rank+1
        if (Recv(worker_rank+1, d-1, k_max+2, results, worker_rank)) return;
      }

      // new range for next round d+1
      k_min--;  //extend    // -d has decreased by 1, same number of entries on the left
      k_max++;  //extend    // 1 additional entry -> += 2
    } else {

      // Send entry (d, k_max) to worker_rank+1
      if(worker_rank != num_workers){
        x = results.result_at(d-1, k_max);
        std::vector<int> msg{d-1, k_max, x};
        DEBUG(2, worker_rank << " | Send to " << worker_rank+1 << " ("<<d-1<<", "<<k_max<<", "<<x<<")");
        MPI_Send(msg.data(), msg.size(), MPI_INT, worker_rank+1, Tag::ResultEntry, MPI_COMM_WORLD);
      }

      // Receive entry (d, k_min-2) from worker_rank-1
      if(worker_rank > 1){
        if (Recv(worker_rank-1, d-1, k_min-2, results, worker_rank)) return;
      }

      // new range for next round d+1
      k_min--;  //extend    // -d has decreased by 1, same number of entries on the left
      k_max--;  //decrease  // -d has decreased by 1, no additional entry
    }
    // TODO: Priorize entries
    for (int k=k_min; k <= k_max; k+=2) {
      DEBUG_NO_LINE(2, worker_rank << " | ");
      bool done = compute_entry(d, k, results, in_1, in_2);
      if (done) {
        send_result(d, k_min, k_max, results);
        stop_master(d);
        return;
      }
    }
    bool done = send_result(d, k_min, k_max, results);
    if(done)
        return;
  }
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

  DEBUG(2, world_rank << " | " << "EXITING\n");
  MPI_Finalize();
  return 0;
}
