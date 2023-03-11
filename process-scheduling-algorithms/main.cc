#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <locale>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

namespace ps {
struct Process {
  int at;   // Arrival time
  int bt;   // Burst time
  int st;   // Start time
  int ct;   // Completion time (start time + burst time)
  int tt;   // Turnaround time (completion time - arrival time)
  int rt;   // Response time (start time - arrival time)
  int wt;   // Wait time (turnaround time - burst time)
  int rbt;  // Remaining burst time (used in RR)
  bool queued;
  bool finished;
};

struct ProcessAverageMetrics {
  float tt;
  float rt;
  float wt;
};

class Scheduler {
 public:
  explicit Scheduler(std::vector<Process> processes)
      : processes_(std::move(processes)), processes_count_{processes_.size()} {}

  virtual ~Scheduler() = default;

  virtual ProcessAverageMetrics Start() = 0;

 protected:
  void SortArrivalTimeAsceding() {
    auto comparer = [](const Process& lhs, const Process& rhs) {
      return lhs.at < rhs.at;
    };

    std::sort(processes_.begin(), processes_.end(), comparer);
  }

  std::vector<Process> processes_;
  std::size_t processes_count_;
};

class FCFSScheduler : public Scheduler {
 public:
  explicit FCFSScheduler(const std::vector<Process>& processes) : Scheduler(processes) {}

  ~FCFSScheduler() override = default;

  ProcessAverageMetrics Start() override {
    ProcessAverageMetrics metrics{};

    SortArrivalTimeAsceding();

    for (std::size_t i = 0; i < processes_count_; i++) {
      auto& process{processes_[i]};

      process.st = i == 0 ? process.at : std::max(process.at, processes_[i - 1].ct);
      process.ct = process.st + process.bt;

      process.tt = process.ct - process.at;
      process.rt = process.st - process.at;
      process.wt = process.tt - process.bt;

      metrics.tt += static_cast<float>(process.tt);
      metrics.rt += static_cast<float>(process.rt);
      metrics.wt += static_cast<float>(process.wt);
    }

    metrics.tt /= static_cast<float>(processes_count_);
    metrics.rt /= static_cast<float>(processes_count_);
    metrics.wt /= static_cast<float>(processes_count_);

    return metrics;
  }
};

class SJFScheduler : public Scheduler {
 public:
  explicit SJFScheduler(const std::vector<Process>& processes) : Scheduler(processes) {}

  ~SJFScheduler() override = default;

  ProcessAverageMetrics Start() override {
    ProcessAverageMetrics metrics{};

    int time_passed{};

    std::size_t finished_count{};
    while (finished_count < processes_count_) {
      Process* pProcess{};

      int bt_threshold = std::numeric_limits<int>::max();

      for (auto& process : processes_) {
        if (process.finished || process.at > time_passed) {
          continue;
        }

        bool found{process.bt < bt_threshold};
        if (process.bt == bt_threshold && pProcess) {
          found = process.at < pProcess->at;
        }

        if (found) {
          bt_threshold = process.bt;
          pProcess = &process;
        }
      }

      if (!pProcess) {
        time_passed++;
        continue;
      }

      pProcess->st = time_passed;
      pProcess->ct = pProcess->st + pProcess->bt;

      pProcess->tt = pProcess->ct - pProcess->at;
      pProcess->rt = pProcess->st - pProcess->at;
      pProcess->wt = pProcess->tt - pProcess->bt;

      time_passed = pProcess->ct;

      metrics.tt += static_cast<float>(pProcess->tt);
      metrics.rt += static_cast<float>(pProcess->rt);
      metrics.wt += static_cast<float>(pProcess->wt);

      pProcess->finished = true;
      finished_count++;
    }

    metrics.tt /= static_cast<float>(processes_count_);
    metrics.rt /= static_cast<float>(processes_count_);
    metrics.wt /= static_cast<float>(processes_count_);

    return metrics;
  }
};

class RRScheduler : public Scheduler {
 public:
  explicit RRScheduler(const std::vector<Process>& processes, int quantum)
      : quantum_{quantum}, Scheduler(processes) {}

  ~RRScheduler() override = default;

  ProcessAverageMetrics Start() override {
    SortArrivalTimeAsceding();

    ProcessAverageMetrics metrics{};

    int time_passed{};

    std::queue<std::size_t> ready_indexes_queue{};

    ready_indexes_queue.push(0);

    std::size_t finished_count{};
    while (finished_count < processes_count_) {
      const auto curr_index{ready_indexes_queue.front()};
      auto& curr{processes_[curr_index]};

      ready_indexes_queue.pop();

      if (curr.rbt == curr.bt) {
        curr.st = std::max(time_passed, curr.at);
        time_passed = curr.st;
      }

      if (curr.rbt - quantum_ > 0) {
        curr.rbt -= quantum_;
        time_passed += quantum_;
      } else {
        time_passed += curr.rbt;

        curr.ct = time_passed;
        curr.tt = curr.ct - curr.at;
        curr.rt = curr.st - curr.at;
        curr.wt = curr.tt - curr.bt;

        metrics.tt += static_cast<float>(curr.tt);
        metrics.rt += static_cast<float>(curr.rt);
        metrics.wt += static_cast<float>(curr.wt);

        curr.rbt = 0;
        curr.finished = true;

        finished_count++;
      }

      std::size_t next_index{1};

      for (auto it = processes_.begin() + 1; it != processes_.end(); it++, next_index++) {
        if (it->queued || it->finished) {
          continue;
        }

        if (it->at <= time_passed) {
          ready_indexes_queue.push(next_index);
          it->queued = true;
        }
      }

      if (!curr.finished) {
        ready_indexes_queue.push(curr_index);
      }

      if (ready_indexes_queue.empty()) {
        next_index = 1;

        for (auto it = processes_.begin() + 1; it != processes_.end();
             it++, next_index++) {
          if (it->finished) {
            continue;
          }

          ready_indexes_queue.push(next_index);
          it->queued = true;

          break;
        }
      }
    }

    metrics.tt /= static_cast<float>(processes_count_);
    metrics.rt /= static_cast<float>(processes_count_);
    metrics.wt /= static_cast<float>(processes_count_);

    return metrics;
  }

 private:
  int quantum_;
};
}  // namespace ps

// Custom numeric separator (",") for std output.
class NumericSeparator : public std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
};

std::vector<ps::Process> ParseFile(const std::filesystem::path& filepath);

int main(int argc, char** argv) {
  std::cout.imbue(std::locale(std::cout.getloc(), new NumericSeparator));

  if (argc < 2) {
    std::cout << "Usage: "
              << std::filesystem::path(argv[0]).filename().string()
              << " [processes file]" << std::endl;

    std::cin.get();
    return EXIT_SUCCESS;
  }

  const std::filesystem::path filepath{argv[1]};
  if (!std::filesystem::exists(filepath)) {
    std::cerr << "File not found: " + filepath.string() << std::endl;

    std::cin.get();
    return EXIT_FAILURE;
  }

  const auto& processes{ParseFile(filepath)};
  if (processes.empty()) {
    std::cout << "No process to schedule." << std::endl;

    std::cin.get();
    return EXIT_SUCCESS;
  }

  std::vector<std::tuple<std::string, ps::Scheduler*>> schedulers = {
      std::tuple{"FCFS", new ps::FCFSScheduler{processes}},
      std::tuple{"SJF", new ps::SJFScheduler{processes}},
      std::tuple{"RR", new ps::RRScheduler{processes, 2}}};

  for (auto& pair : schedulers) {
    const auto& name{std::get<0>(pair)};
    const auto& scheduler{std::get<1>(pair)};

    const auto& metrics{scheduler->Start()};

    std::cout << std::setprecision(1) << std::fixed << name << " " << metrics.tt << " "
              << metrics.rt << " " << metrics.wt << std::endl;

    delete scheduler;
  }

  std::cin.get();
}

std::vector<ps::Process> ParseFile(const std::filesystem::path& filepath) {
  std::ifstream file_stream{filepath, std::ios::in};
  if (!file_stream) {
    return {};
  }

  std::vector<ps::Process> result{};

  std::string line{};
  std::string token{};

  int at{};
  int bt{};

  while (std::getline(file_stream, line)) {
    std::stringstream line_stream{line};

    // 0 -> arrival time; 1 -> burst time;
    int token_index{};

    while (std::getline(line_stream, token, ' ')) {
      std::stringstream token_stream{token};

      if (token_index == 0) {
        token_stream >> at;
      } else if (token_index == 1) {
        token_stream >> bt;
      }

      if (token_index > 1 || token_stream.fail()) {
        std::cerr << "Bad formatted input: " << line << std::endl;
      }

      token_index++;
    }

    result.push_back({.at = at, .bt = bt, .rbt = bt});
  }

  return result;
}
