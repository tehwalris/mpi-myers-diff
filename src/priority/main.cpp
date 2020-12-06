#include <mpi.h>
#include "strategy.hpp"
#include "storage.hpp"
#include "partition.hpp"
#include "util.hpp"

enum Tag
{
  ReportWork,
  Interrupt,
};

template <class S>
class MPIStrategyFollower
{
public:
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
    int &stored = storage->at(d, k);
    assert(stored == S::undefined);
    stored = v;
  }

  bool calculate(int d, int k)
  {
    assert(d >= 0 && abs(k) <= d);

    int x;
    if (d == 0)
    {
      x = 0;
    }
    else if (k == -d || k != d && get(d - 1, k - 1) < get(d - 1, k + 1))
    {
      x = get(d - 1, k + 1);
    }
    else
    {
      x = get(d - 1, k - 1) + 1;
    }

    int y = x - k;

    while (x < in_1.size() && y < in_2.size() && in_1.at(x) == in_2.at(y))
    {
      x++;
      y++;
    }

    set(d, k, x);
    return x >= in_1.size() && y >= in_2.size();
  }

  void send(int d, int k, Side to)
  {
    int to_rank = to == Side::Left ? world_rank - 1 : world_rank + 1;
    assert(to_rank >= 0 && to_rank < world_size);
    int x = get(d, k);
    std::vector<int> msg{int(other_side(to)), x};
    MPI_Send(msg.data(), msg.size(), MPI_INT, to_rank, Tag::ReportWork, MPI_COMM_WORLD);
  }

  void send_interrupt(Side to)
  {
    if ((world_rank == 0 && to == Side::Left) || (world_rank + 1 == world_size && to == Side::Right))
    {
      return;
    }
    int to_rank = to == Side::Left ? world_rank - 1 : world_rank + 1;
    assert(to_rank >= 0 && to_rank < world_size);
    std::vector<int> msg{int(to)};
    MPI_Send(msg.data(), msg.size(), MPI_INT, to_rank, Tag::Interrupt, MPI_COMM_WORLD);
  }

  bool has_incoming_message()
  {
    int flag;
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
    return flag != 0;
  }

  std::optional<std::pair<Side, int>> blocking_receive()
  {
    // handle interrupt if one is already queued
    int flag;
    MPI_Iprobe(MPI_ANY_SOURCE, Tag::Interrupt, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
    if (flag)
    {
      consume_and_forward_interrupt();
      return std::nullopt;
    }

    // wait for any message (could still be an interrupt that arrives later)
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    if (status.MPI_TAG == Tag::Interrupt)
    {
      consume_and_forward_interrupt();
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

  inline int get(int d, int k)
  {
    int stored = storage->at(d, k);
    assert(stored != S::undefined);
    return stored;
  }

  void consume_and_forward_interrupt()
  {
    std::vector<int> msg(1);
    MPI_Recv(msg.data(), msg.size(), MPI_INT, MPI_ANY_SOURCE, Tag::Interrupt, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    Side to = Side(msg.at(0));
    send_interrupt(to);
  }
};

void main_worker(std::string path_1, std::string path_2)
{
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  std::vector<int> in_1 = read_vec_from_file(path_1);
  std::vector<int> in_2 = read_vec_from_file(path_2);

  int d_max = in_1.size() + in_2.size() + 1;

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

  SimpleStorage storage(d_max);
  MPIStrategyFollower<SimpleStorage> follower(&storage, in_1, in_2, world_rank, world_size);
  Strategy strategy(&follower, future_receive_begins, future_receive_ends, future_send_begins, future_send_ends, d_max);

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
      if (!result_opt.has_value())
      {
        break;
      }
      auto result = result_opt.value();
      strategy.receive(result.first, result.second);
    }
  }

  DEBUG(0, "all done ");

  if (strategy.get_final_result_location().has_value())
  {
    follower.send_interrupt(Side::Left);
    follower.send_interrupt(Side::Right);

    CellLocation loc = strategy.get_final_result_location().value();
    DEBUG(0, "result " << loc << " " << storage.at(loc.d, loc.k));
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

  main_worker(path_1, path_2);

  MPI_Finalize();
  return 0;
}
