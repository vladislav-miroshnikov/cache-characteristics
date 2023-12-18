#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <set>
#include <numeric>

constexpr int MAX_MEMORY = 1024 * 1024 * 1024;
constexpr int MAX_ASSOCIATIVITY = 16;

namespace cache_timer {
    char *data;

    void alloc_memory() {
        data = (char *) malloc(MAX_MEMORY);
        if (data == nullptr) {
            std::cout << "Unable to allocate memory.\n";
            std::exit(1);
        }
    }

    void free_memory() {
        if (data != nullptr) {
            free(data);
        }
    }

    __attribute__ ((optimize(0))) long long measure(const int H, const int S) {
        char **x;
        for (int i = (S - 1) * H; i >= 0; i -= H) {
            x = (char **) &data[i];
            *x = i >= H ? &data[i - H] : &data[(S - 1) * H];
        }

        const long long iter_count = 20;
        std::vector<long long> times;
        for (long long k = 0L; k < iter_count; k++) {
            auto start = std::chrono::high_resolution_clock::now();
            for (long long i = 0L; i < 1000 * 1000; i++) {
                x = (char **) *x;
            }
            auto end = std::chrono::high_resolution_clock::now();
            times.push_back((end - start).count());
        }
        return std::accumulate(times.begin(), times.end(), 0L) / iter_count;
    }
}

std::pair<std::vector<std::set<int>>, int> calculate_jumps() {
    std::vector<std::set<int>> jumps;
    int H = MAX_ASSOCIATIVITY;

    for (; H < MAX_MEMORY / MAX_ASSOCIATIVITY; H *= 2) {
        auto prev_time = cache_timer::measure(H, 1);
        std::set<int> new_jumps;

        for (int S = 1; S <= MAX_ASSOCIATIVITY; S++) {
            auto curr_time = cache_timer::measure(H, S);
            if ((curr_time - prev_time) * 10 > curr_time) {
                new_jumps.insert(S - 1);
            }
            prev_time = curr_time;
        }

        bool same_jump = true;
        if (not jumps.empty()) {
            for (int new_jump: new_jumps) {
                same_jump &= jumps.back().count(new_jump);
            }
            for (int jump: jumps.back()) {
                same_jump &= new_jumps.count(jump);
            }
        }

        if (same_jump && H >= 256 * 1024) {
            break;
        }

        jumps.push_back(new_jumps);
    }

    return {jumps, H};
}

std::pair<int, int> build_caches(std::vector<std::set<int>> jumps, int H) {
    std::vector<std::pair<int, int>> caches;
    std::set<int> jumps_to_process = jumps[jumps.size() - 1];
    std::reverse(jumps.begin(), jumps.end());
    for (auto &jump: jumps) {
        std::set<int> jumps_to_delete;
        for (int s: jumps_to_process) {
            if (!jump.count(s)) {
                caches.emplace_back(H * s, s);
                jumps_to_delete.insert(s);
            }
        }
        for (int s: jumps_to_delete) {
            jumps_to_process.erase(s);
        }
        H /= 2;
    }
    if (caches.empty()) {
        std::cout << "Failed calculating L1 cache characteristics.\n";
        std::exit(1);
    }
    std::sort(caches.begin(), caches.end());
    // caches[0] will be L1 cache characteristics
    return caches[0];
}

int calculate_cache_line_size(int cache_size, int cache_assoc) {
    int cache_line_size = -1;
    int prev_first_jump = 1025;

    for (int L = 1; L <= cache_size; L *= 2) {
        auto prev_time = cache_timer::measure(cache_size / cache_assoc + L, 2);
        int first_jump = -1;
        for (int S = 1; S <= 1024; S *= 2) {
            auto curr_time = cache_timer::measure(cache_size / cache_assoc + L, S + 1);
            if ((curr_time - prev_time) * 10 > curr_time) {
                if (first_jump <= 0) {
                    first_jump = S;
                }
            }
            prev_time = curr_time;
        }
        if (first_jump > prev_first_jump) {
            cache_line_size = L * cache_assoc;
            break;
        }
        prev_first_jump = first_jump;
    }

    return cache_line_size;
}

int main() {
    std::cout << "Calculating L1 cache characteristics: ...\n";
    cache_timer::alloc_memory();
    auto [jumps, H] = calculate_jumps();
    auto [cache_size, cache_assoc] = build_caches(std::move(jumps), H);

    int cache_line_size = calculate_cache_line_size(cache_size, cache_assoc);

    std::cout << "L1 cache characteristics:\n" << "LEVEL1_CACHE_SIZE = " << cache_size <<
              "\nLEVEL1_CACHE_ASSOC = " << cache_assoc <<
              "\nLEVEL1_CACHE_LINESIZE = " << cache_line_size << std::endl;
    cache_timer::free_memory();
    return 0;
}