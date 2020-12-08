#include <mpi.h>
#include <chrono>
#include "strategy.hpp"
#include "storage.hpp"
#include "partition.hpp"
#include "util.hpp"

#ifdef FRONTIER_STORAGE
typedef FrontierStorage Storage;
#else
typedef FastStorage Storage;
#endif

const int master_rank = 0;

enum Tag
{
  ReportWork,
  ReportLcsLength,
};

template <class S>
class MPIStrategyFollower
{
public:
  static const int no_known_d = -1;

  MPIStrategyFollower(
      S *storage,
      const std::vector<int> &in_1,
      const std::vector<int> &in_2,
      int world_rank,
      int world_size) : storage(storage),
                        in_1(in_1),
                        in_2(in_2),
                        world_rank(world_rank),
                        world_size(world_size)
  {
    assert(world_size > 0);
    assert(world_rank >= 0 && world_rank < world_size);
  };

  inline void set(int d, int k, int v)
  {
    storage->set(d, k, v);
  }

  bool calculate(int d, int k)
  {
    assert(d >= 0 && abs(k) <= d);

    int x;
    if (d == 0)
    {
      x = 0;
    }
    else if (k == -d || k != d && storage->get(d - 1, k - 1) < storage->get(d - 1, k + 1))
    {
      x = storage->get(d - 1, k + 1);
    }
    else
    {
      x = storage->get(d - 1, k - 1) + 1;
    }

    int y = x - k;

    while (x < in_1.size() && y < in_2.size() && in_1.at(x) == in_2.at(y))
    {
      x++;
      y++;
    }

    storage->set(d, k, x);

    return x >= in_1.size() && y >= in_2.size() && k == in_1.size() - in_2.size();
  }

  void send(int d, int k, Side to)
  {
    int to_rank = to == Side::Left ? world_rank - 1 : world_rank + 1;
    assert(to_rank >= 0 && to_rank < world_size);
    int x = storage->get(d, k);
    std::vector<int> msg{int(other_side(to)), x};
    MPI_Send(msg.data(), msg.size(), MPI_INT, to_rank, Tag::ReportWork, MPI_COMM_WORLD);
  }

  bool has_incoming_message()
  {
    int flag;
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
    return flag != 0;
  }

  std::optional<std::pair<Side, int>> blocking_receive()
  {
    // stop if result already found
    int flag;
    MPI_Iprobe(MPI_ANY_SOURCE, Tag::ReportLcsLength, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
    if (flag)
    {
      return std::nullopt;
    }

    // wait for any message (could still be the result that arrived later)
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    if (status.MPI_TAG != Tag::ReportWork)
    {
      return std::nullopt;
    }

    assert(status.MPI_TAG == Tag::ReportWork);
    std::pair<Side, int> out;
    std::vector<int> msg(3);
    MPI_Recv(msg.data(), msg.size(), MPI_INT, MPI_ANY_SOURCE, Tag::ReportWork, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    out.first = Side(msg.at(0));
    out.second = msg.at(1);
    return std::optional{out};
  }

private:
  const std::vector<int> &in_1;
  const std::vector<int> &in_2;
  S *storage;
  int world_rank;
  int world_size;
};

void main_worker(std::string path_1, std::string path_2)
{
  std::chrono::_V2::system_clock::time_point t_in_start, t_in_end, t_sol_start, t_sol_end;

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  t_in_start = std::chrono::high_resolution_clock::now();
  std::vector<int> in_1 = read_vec_from_file(path_1);
  std::vector<int> in_2 = read_vec_from_file(path_2);
  t_in_end = std::chrono::high_resolution_clock::now();

  MPI_Barrier(MPI_COMM_WORLD);

  // Ideally the workers should all start at exactly the same time. This is never exactly possible, so t_sol_start is the time when the master started calculating.
  t_sol_start = std::chrono::high_resolution_clock::now();

  const int d_max_possible = in_1.size() + in_2.size();
  const int d_max = d_max_possible;

  RoundRobinPartition partition(world_size, world_rank);

  ReceiveSideIterator left_receive_begin(partition, Side::Left);
  ReceiveSideIterator left_receive_end(partition);
  ReceiveSideIterator right_receive_begin(partition, Side::Right);
  ReceiveSideIterator right_receive_end(partition);
  PerSide<ReceiveSideIterator<RoundRobinPartition> &> future_receive_begins(left_receive_begin, right_receive_begin);
  PerSide<ReceiveSideIterator<RoundRobinPartition> &> future_receive_ends(left_receive_end, right_receive_end);

  SendSideIterator left_send_begin(partition, Side::Left);
  SendSideIterator left_send_end(partition);
  SendSideIterator right_send_begin(partition, Side::Right);
  SendSideIterator right_send_end(partition);
  PerSide<SendSideIterator<RoundRobinPartition> &> future_send_begins(left_send_begin, right_send_begin);
  PerSide<SendSideIterator<RoundRobinPartition> &> future_send_ends(left_send_end, right_send_end);

  Storage storage(d_max);

  MPIStrategyFollower follower(&storage, in_1, in_2, world_rank, world_size);
  const int diamond_height_limit = 20;
  Strategy strategy(&follower, future_receive_begins, future_receive_ends, future_send_begins, future_send_ends, d_max, diamond_height_limit);

  while (true)
  {
    strategy.run();

    if (strategy.is_done())
    {
      break;
    }
    if (strategy.is_blocked_waiting_for_receive() || follower.has_incoming_message())
    {
      std::optional<std::pair<Side, int>> result_opt = follower.blocking_receive();
      if (!result_opt.has_value()) // received an interrupt or final result
      {
        break;
      }
      auto result = result_opt.value();
      strategy.receive(result.first, result.second);
    }
  }

  DEBUG(1, world_rank << " | "
                      << "self done in " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t_sol_start).count() << std::endl);

  int lcs_length;
  int lcs_found_by;
  if (strategy.get_final_result_location().has_value())
  {
    CellLocation loc = strategy.get_final_result_location().value();
    lcs_length = loc.d;
    lcs_found_by = world_rank;
    if (world_rank != master_rank)
    {
      MPI_Send(&lcs_length, 1, MPI_INT, master_rank, Tag::ReportLcsLength, MPI_COMM_WORLD);
    }
  }
  else
  {
    MPI_Status status;
    MPI_Recv(&lcs_length, 1, MPI_INT, MPI_ANY_SOURCE, Tag::ReportLcsLength, MPI_COMM_WORLD, &status);
    lcs_found_by = status.MPI_SOURCE;
  }

  t_sol_end = std::chrono::high_resolution_clock::now();

  if (world_rank != master_rank)
  {
    return;
  }

  for (int i = 0; i < world_size; i++)
  {
    if (i != world_rank && i != lcs_found_by)
    {
      MPI_Send(&lcs_length, 1, MPI_INT, i, Tag::ReportLcsLength, MPI_COMM_WORLD);
    }
  }

  std::cout << "min edit length " << lcs_length << std::endl;
  std::cout << "Read Input [μs]: \t" << std::chrono::duration_cast<std::chrono::microseconds>(t_in_end - t_in_start).count() << std::endl;
  std::cout << "Precompute [μs]: \t" << 0 << std::endl;
  std::cout << "Solution [μs]:   \t" << std::chrono::duration_cast<std::chrono::microseconds>(t_sol_end - t_sol_start).count() << std::endl;
  std::cout << "Edit Script [μs]: \t" << 0 << std::endl;
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

  main_worker(path_1, path_2);

  MPI_Finalize();
  return 0;
}
