#include <mpi.h>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <vector>
#include <algorithm>

const int debug_level = 0;

#define DEBUG(level, x)          \
  if (debug_level >= level)      \
  {                              \
    std::cerr << x << std::endl; \
  }

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

    Results(int d_max){
        m_data = std::vector<int>(d_max);
    }

    int &result_at(int d, int k){
        int start = d*d;
        int access = k+d;
        std::cout << "DEBUG:" << "d:" << d << "k:" << k << " start:" << start << " access:" << access << std::endl;
        assert(access >= 0 && access <= d*2);
        return m_data.at(start+access);
    }
};

//typedef std::vector<std::vector<int>> Results;

//int &result_at(int d, int k, Results results)
//{
//  auto &row = results.at(d);
//  return row.at(row.size() / 2 + k);
//}

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

void main_master(const std::string path_1, const std::string path_2)
{
  DEBUG(2, "started master");

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

  int d_max = in_1.size() + in_2.size() + 1;
  MPI_Bcast(&d_max, 1, MPI_INT, 0, MPI_COMM_WORLD);

  int edit_len = unknown_len;
  Results results(d_max);
  std::cout << "s:" << sizeof(Results) << std::endl;
  std::vector<int> size_by_worker(num_workers, 0);
  int next_worker_to_extend = 0;
  for (int d = 0; d < d_max; d++)
  {
    DEBUG(2, "calculating layer " << d);

    size_by_worker.at(next_worker_to_extend)++;
    next_worker_to_extend = (next_worker_to_extend + 1) % num_workers;

    int k_min = -d;
    for (int i = 0; i < num_workers && k_min <= d; i++)
    {
      if (size_by_worker.at(i) == 0)
      {
        break;
      }
      int k_max = k_min + 2 * (size_by_worker.at(i) - 1);

      int down_receiver = no_worker_rank, up_receiver = no_worker_rank;
      int num_to_receive = 1;

      if (i > next_worker_to_extend)
      {
        // will be extended below, send down
        down_receiver = (i + 1) - 1;
      }
      else if (i < next_worker_to_extend)
      {
        // will be extended above, send up
        up_receiver = (i + 1) + 1;
      }
      else
      { // i == next_worker_to_extend
        num_to_receive++;
      }

      if (i == 0 || i + 1 == num_workers)
      {
        num_to_receive--;
      }

      std::vector<int> msg{d, k_min, k_max, down_receiver, up_receiver, num_to_receive};
      MPI_Send(msg.data(), msg.size(), MPI_INT, i + 1, Tag::AssignWork, MPI_COMM_WORLD);

      k_min = k_max + 2;
    }

    if (size_by_worker.at(next_worker_to_extend) == 0)
    {
      // HACK Special values that mean the worker should only receive
      std::vector<int> msg{d, 1, 0, no_worker_rank, no_worker_rank, 1};
      MPI_Send(msg.data(), msg.size(), MPI_INT, next_worker_to_extend + 1, Tag::AssignWork, MPI_COMM_WORLD);
    }

    for (int i = 0; i < d + 1; i++)
    {
      int d, k, x;
      {
        std::vector<int> msg(3);
        MPI_Recv(msg.data(), msg.size(), MPI_INT, MPI_ANY_SOURCE, Tag::ReportWork, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        d = msg.at(0);
        k = msg.at(1);
        x = msg.at(2);
      }
      results.result_at(d, k) = x;

      int y = x - k;
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

  DEBUG(2, "shutting down workers");
  {
    std::vector<int> msg{shutdown_sentinel, 0, 0};
    for (int i = 1; i < comm_size; i++)
    {
      MPI_Send(msg.data(), msg.size(), MPI_INT, i, Tag::AssignWork, MPI_COMM_WORLD);
    }
  }
}

void main_worker()
{
  int own_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &own_rank);

  DEBUG(2, own_rank << " | "
                    << "started worker");

  DEBUG(2, own_rank << " | "
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

  int max_d;
  MPI_Bcast(&max_d, 1, MPI_INT, 0, MPI_COMM_WORLD);

  std::vector<int> V(max_d * 2 + 1);
  auto V_at = [&V, max_d](int k) -> int & { return V.at(max_d + k); };

  while (true)
  {
    int d, k_min, k_max, down_receiver, up_receiver, num_to_receive;
    {
      std::vector<int> msg(6);
      MPI_Recv(msg.data(), msg.size(), MPI_INT, 0, Tag::AssignWork, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      if (msg.at(0) == shutdown_sentinel)
      {
        break;
      }
      d = msg.at(0);
      k_min = msg.at(1);
      k_max = msg.at(2);
      down_receiver = msg.at(3);
      up_receiver = msg.at(4);
      num_to_receive = msg.at(5);
    }

    DEBUG(2, own_rank << " | "
                      << "working " << d << " " << k_min << " " << k_max);

    if (k_min > k_max)
    {
      // HACK Special values that mean we should only receive
      assert(k_min == 1 && k_max == 0);
    }

    for (int k = k_min; k <= k_max; k += 2)
    {
      assert((k + d) % 2 == 0);

      int x;
      if (k == -d || k != d && V_at(k - 1) < V_at(k + 1))
      {
        x = V_at(k + 1);
      }
      else
      {
        x = V_at(k - 1) + 1;
      }

      int y = x - k;

      while (x < in_1.size() && y < in_2.size() && in_1.at(x) == in_2.at(y))
      {
        x++;
        y++;
      }

      DEBUG(2, own_rank << " | "
                        << "x: " << x);
      V_at(k) = x;
      {
        std::vector<int> msg{d, k, x};
        if (k == k_min && down_receiver != no_worker_rank)
        {
          MPI_Send(msg.data(), msg.size(), MPI_INT, down_receiver, Tag::ReportWork, MPI_COMM_WORLD);
        }
        else if (k == k_max && up_receiver != no_worker_rank)
        {
          MPI_Send(msg.data(), msg.size(), MPI_INT, up_receiver, Tag::ReportWork, MPI_COMM_WORLD);
        }
      }
    }

    DEBUG(2, own_rank << " | "
                      << "receiving " << num_to_receive << " in d " << d);
    for (int i = 0; i < num_to_receive; i++)
    {
      std::vector<int> msg(3);
      MPI_Recv(msg.data(), msg.size(), MPI_INT, MPI_ANY_SOURCE, Tag::ReportWork, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      DEBUG(2, own_rank << " | "
                        << "received " << msg.at(0) << " " << msg.at(1) << " " << msg.at(2) << " in d " << d);
      assert(msg.at(0) == d);
      assert((msg.at(1) < k_min || msg.at(1) > k_max));
      V_at(msg.at(1)) = msg.at(2);
    }

    // IMPORTANT Only send the results to the master now, so that the next round is not started before
    // we have received all values from other workers from the current round.
    for (int k = k_min; k <= k_max; k += 2)
    {
      std::vector<int> msg{d, k, V_at(k)};
      MPI_Send(msg.data(), msg.size(), MPI_INT, 0, Tag::ReportWork, MPI_COMM_WORLD);
    }
  }

  DEBUG(2, own_rank << " | "
                    << "worker exiting");
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
