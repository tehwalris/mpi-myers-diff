#include <mpi.h>
#include <iostream>
#include <assert.h>
#include <vector>
#include <algorithm>

const int shutdown_sentinel = -1;
const int unknown_len = -1;

enum Tag
{
  AssignWork,
  ReportWork,
};

typedef std::vector<std::vector<int>> Results;

int &result_at(int d, int k, Results results)
{
  auto &row = results.at(d);
  return row.at(row.size() / 2 + k);
}

void print_vector(const std::vector<int> &vec)
{
  for (int i = 0; i < vec.size(); i++)
  {
    if (i != 0)
    {
      std::cout << " ";
    }
    std::cout << vec.at(i);
  }
  std::cout << std::endl;
}

inline int div_ceil(int a, int b)
{
  assert(b > 0);
  return (a + b - 1) / b;
}

void main_master()
{
  std::vector<int> in_1{2, 4, 1, 3, 3};
  std::vector<int> in_2{2, 4, 7, 1, 3, 3, 3};

  std::cout << "started master" << std::endl;

  int comm_size;
  MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
  assert(comm_size > 1);

  std::cout << "sending inputs" << std::endl;
  auto send_vector = [](const std::vector<int> &vec) {
    int temp_size = vec.size();
    MPI_Bcast(&temp_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast((void *)(vec.data()), vec.size(), MPI_INT, 0, MPI_COMM_WORLD);
  };
  send_vector(in_1);
  send_vector(in_2);

  int max_d = in_1.size() + in_2.size() + 1;
  MPI_Bcast(&max_d, 1, MPI_INT, 0, MPI_COMM_WORLD);

  int edit_len = unknown_len;
  Results results(max_d, std::vector<int>(2 * max_d + 1));
  for (int d = 0; d < max_d; d++)
  {
    int target_chunk_size = div_ceil((d + 1), (comm_size - 1));
    std::cout << "calculating layer " << d << " with chunk size " << target_chunk_size << std::endl;

    int min_k = -d;
    for (int i = 1; i < comm_size && min_k <= d; i++)
    {
      int max_k = std::min({min_k + target_chunk_size, d});
      std::vector<int> msg{d, min_k, max_k};
      MPI_Send(msg.data(), msg.size(), MPI_INT, 1, Tag::AssignWork, MPI_COMM_WORLD);

      min_k = max_k + 1;
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
      result_at(d, k, results) = x;

      int y = x - k;
      if (x >= in_1.size() && y >= in_2.size())
      {
        std::cout << "found lcs" << std::endl;
        edit_len = d;
        goto done;
      }
    }
  }

done:
  std::cout << "min edit length " << edit_len << "" << std::endl;

  std::cout << "shutting down workers" << std::endl;
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
  std::cout << "started worker" << std::endl;

  std::cout << "receiving inputs" << std::endl;
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
    int d, k_min, k_max;
    {
      std::vector<int> msg(3);
      MPI_Recv(msg.data(), msg.size(), MPI_INT, 0, Tag::AssignWork, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      if (msg.at(0) == shutdown_sentinel)
      {
        break;
      }
      d = msg.at(0);
      k_min = msg.at(1);
      k_max = msg.at(2);
    }

    std::cout << "\"working\" " << d << " " << k_min << " " << k_max << std::endl;

    for (int k = k_min + (k_min + d) % 2; k <= k_max; k += 2)
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

      std::cout << "x: " << x << std::endl;
      V_at(k) = x;
      {
        std::vector<int> msg{d, k, x};
        MPI_Send(msg.data(), msg.size(), MPI_INT, 0, Tag::ReportWork, MPI_COMM_WORLD);
      }
    }
  }

  std::cout << "worker exiting" << std::endl;
}

int main()
{
  std::ios_base::sync_with_stdio(false);

  MPI_Init(nullptr, nullptr);

  int world_size;
  assert(world_size > 1);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  if (world_rank == 0)
  {
    main_master();
  }
  else
  {
    main_worker();
  }

  MPI_Finalize();
  return 0;
}