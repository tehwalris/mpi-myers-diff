#include <mpi.h>
#include <iostream>
#include <assert.h>
#include <vector>

const int shutdown_sentinel = -1;

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
  std::cout << "\n";
}

void main_master()
{
  std::cout << "started master\n";

  std::vector<int> in_1{2, 4, 1, 3, 3};
  std::vector<int> in_2{2, 4, 7, 1, 3, 3, 3};

  std::cout << "sending inputs\n";
  auto send_vector = [](const std::vector<int> &vec) {
    int temp_size = vec.size();
    MPI_Bcast(&temp_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast((void *)(vec.data()), vec.size(), MPI_INT, 0, MPI_COMM_WORLD);
  };
  send_vector(in_1);
  send_vector(in_2);

  int max_d = in_1.size() + in_2.size() + 1;
  Results results(max_d, std::vector<int>(2 * max_d + 1));

  for (int d = 0; d < 1; d++)
  {
    std::vector<int> msg{d, -d, d};
    MPI_Send(msg.data(), msg.size(), MPI_INT, 1, Tag::AssignWork, MPI_COMM_WORLD);
  }

  std::cout << "shutting down workers\n";
  // TODO send to all workers
  std::vector<int> msg{shutdown_sentinel, 0, 0};
  MPI_Send(msg.data(), msg.size(), MPI_INT, 1, Tag::AssignWork, MPI_COMM_WORLD);
}

void main_worker()
{
  std::cout << "started worker\n";

  std::cout << "receiving inputs\n";
  auto receive_vector = []() {
    int temp_size;
    MPI_Bcast(&temp_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    std::vector<int> vec(temp_size);
    MPI_Bcast((void *)(vec.data()), vec.size(), MPI_INT, 0, MPI_COMM_WORLD);
    return vec;
  };
  std::vector<int> in_1 = receive_vector();
  std::vector<int> in_2 = receive_vector();

  while (true)
  {
    std::vector<int> msg(3);
    MPI_Recv(msg.data(), msg.size(), MPI_INT, 0, Tag::AssignWork, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (msg.at(0) == shutdown_sentinel)
    {
      break;
    }

    std::cout << "\"working\" " << msg.at(0) << " " << msg.at(1) << " " << msg.at(2) << "\n";

    int x = 0, y = 0;
    for (int k = msg.at(1); k <= msg.at(2); k += 2)
    {
      x++;
      y++;
    }

    std::cout << x << "\n";
  }

  std::cout << "worker exiting\n";
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