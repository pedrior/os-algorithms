#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

using page = unsigned int;
using page_fault = unsigned int;
using page_distance = std::size_t;
using frame_capacity = std::size_t;
using virtual_memory = std::pair<frame_capacity, std::vector<page>>;

std::optional<virtual_memory> parse_input(const std::string &filepath);
page_fault fifo(frame_capacity capacity, const std::vector<page> &references);
page_fault otm(frame_capacity capacity, const std::vector<page> &references);
page_fault lru(frame_capacity capacity, const std::vector<page> &references);

constexpr auto kPageDistanceComparer = [](const std::pair<page, page_distance> &lhs,
                                          const std::pair<page, page_distance> &rhs) {
  return lhs.second < rhs.second;
};

int main(int argc, const char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <file>" << std::endl;
    std::cin.get();

    return EXIT_FAILURE;
  }

  const auto parse_result = parse_input(argv[1]);
  if (!parse_result) {
    std::cerr << "Parsing failed!\n" << std::endl;
    std::cin.get();

    return EXIT_FAILURE;
  }

  const auto &virtual_memory{parse_result.value()};
  const auto frame_capacity = std::get<0>(virtual_memory);
  const auto &page_references = std::get<1>(virtual_memory);

  std::cout << "FIFO " << fifo(frame_capacity, page_references) << "\n";
  std::cout << "OTM " << otm(frame_capacity, page_references) << "\n";
  std::cout << "LRU " << lru(frame_capacity, page_references) << std::endl;

  std::cin.get();

  return EXIT_SUCCESS;
}

page_fault fifo(frame_capacity capacity, const std::vector<page> &references) {
  page_fault faults{};
  std::deque<page> frame{};

  for (const page reference : references) {
    if (std::find(frame.begin(), frame.end(), reference) != frame.end()) {
      continue;
    }

    if (frame.size() >= capacity) {
      frame.pop_front();
    }

    frame.push_back(reference);
    faults++;
  }

  return faults;
}

page_fault otm(frame_capacity capacity, const std::vector<page> &references) {
  page_fault faults{};
  std::vector<page> frame{};

  std::unordered_map<page, page_distance> page_distances{};

  for (auto reference_it = references.begin(); reference_it != references.end(); reference_it++) {
    if (std::find(frame.begin(), frame.end(), *reference_it) != frame.end()) {
      continue;
    }

    if (frame.size() < capacity) {
      frame.emplace_back(*reference_it);
      faults++;

      continue;
    }

    const auto start_index{std::distance(references.begin(), reference_it)};
    for (const page page : frame) {
      page_distance distance{std::numeric_limits<page_distance>::max()};

      for (std::size_t end_index = start_index; end_index < references.size(); end_index++) {
        if (references[end_index] == page) {
          distance = end_index - start_index;
          break;
        }
      }

      page_distances[page] = distance;
    }

    const page page{std::max_element(page_distances.begin(), page_distances.end(), kPageDistanceComparer)->first};
    const auto page_index{std::distance(frame.begin(), std::find(frame.begin(), frame.end(), page))};

    frame[page_index] = *reference_it;
    faults++;

    page_distances.clear();
  }

  return faults;
}

page_fault lru(frame_capacity capacity, const std::vector<page> &references) {
  page_fault faults{};
  std::deque<page> frame{};

  for (const page reference : references) {
    const auto page_it{std::find(frame.begin(), frame.end(), reference)};
    if (page_it != frame.end()) {
      frame.erase(page_it);
      frame.push_back(reference);

      continue;
    }

    if (frame.size() >= capacity) {
      frame.pop_front();
    }

    frame.push_back(reference);
    faults++;
  }

  return faults;
}

std::optional<virtual_memory> parse_input(const std::string &filepath) {
  if (!std::filesystem::exists(filepath)) {
    std::cerr << "[FILE NOT FOUND] \"" + filepath + "\"" << std::endl;
    return std::nullopt;
  }

  std::fstream fs{filepath, std::ios::in};
  if (!fs) {
    std::cerr << "[UNABLE TO READ] \"" + filepath + "\"" << std::endl;
    return std::nullopt;
  }

  frame_capacity frame_capacity{};
  std::vector<page> page_references{};

  std::string line{};
  std::size_t line_count{};

  while (std::getline(fs, line)) {
    unsigned int tmp_token;
    std::istringstream iss{line};

    if (!(iss >> tmp_token)) {
      std::cerr << "[LINE " + std::to_string(line_count) + "] \"" + line +
                       "\" can't be interpreted as a valid number.\n"
                << std::endl;
      continue;
    }

    if (line_count == 0) {
      frame_capacity = tmp_token;
    } else {
      page_references.emplace_back(tmp_token);
    }

    line_count++;
  }

  return std::make_pair(frame_capacity, page_references);
}