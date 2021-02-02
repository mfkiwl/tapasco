/*
 * Copyright (c) 2014-2020 Embedded Systems and Applications, TU Darmstadt.
 *
 * This file is part of TaPaSCo
 * (see https://github.com/esa-tu-darmstadt/tapasco).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#include <sstream>
#endif

#include <tapasco.hpp>

using namespace tapasco;

int main(int argc, char **argv) {
  Tapasco tapasco;

  constexpr int max_pow = 30;
  constexpr int repetitions = 1000;

#ifdef _OPENMP
  int threads = 1;

  if (argc > 1) {
    std::stringstream s(argv[1]);
    s >> threads;
  }
  omp_set_num_threads(threads);
#endif

  static constexpr tapasco_kernel_id_t COUNTER_ID{14};
  static constexpr tapasco_kernel_id_t LATENCY_ID{742};

  uint64_t counter = tapasco.kernel_pe_count(COUNTER_ID);
  uint64_t latency = tapasco.kernel_pe_count(LATENCY_ID);
  if (!counter && !latency) {
    std::cout << "Need at least one counter or latencycheck instance to run."
              << std::endl;
    exit(1);
  }

  tapasco_kernel_id_t pe_id = COUNTER_ID;
  if (latency) {
    pe_id = LATENCY_ID;
  }

  std::chrono::duration<double, std::nano> elapsed_seconds;

  std::cout << "Byte,Nanoseconds" << std::endl;

  for (size_t s = 3; s < max_pow; ++s) {
    size_t len = 1 << s;

    size_t elements = std::max((size_t)1, len / sizeof(int));
    std::vector<int> arr_from(elements, -1);

    // Data will be copied back from the device only, no data will be moved to
    // the device
    auto result_buffer_out = tapasco::makeOutOnly(tapasco::makeWrappedPointer(
        arr_from.data(), arr_from.size() * sizeof(int)));
    auto start = std::chrono::steady_clock::now();
    auto end = std::chrono::steady_clock::now();

#ifdef _OPENMP
#pragma omp parallel for shared(elapsed_seconds)
#endif
    for (int i = 0; i < repetitions; ++i) {
      if (len > 8) {
        start = std::chrono::steady_clock::now();
        tapasco.launch(pe_id, 1, result_buffer_out)();
        end = std::chrono::steady_clock::now();
      } else {
        start = std::chrono::steady_clock::now();
        tapasco.launch(pe_id, 1)();
        end = std::chrono::steady_clock::now();
      }
      elapsed_seconds = end - start;
      std::cout << std::fixed << len << "," << elapsed_seconds.count()
                << std::endl;
    }
  }

  return 0;
}
