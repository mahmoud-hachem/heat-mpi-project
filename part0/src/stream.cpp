// STREAM-style memory bandwidth test (copy, scale, add, triad).
// Arrays are far larger than any cache, so the result is main-memory bandwidth.
// Build: g++ -O3 -march=native -fopenmp -o stream stream.cpp   (OpenMP optional)
// Run:   OMP_NUM_THREADS=1 ./stream [n_doubles_per_array=20000000] [reps=10]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
#ifdef _OPENMP
#include <omp.h>
#endif

int main(int argc, char** argv) {
    const long n = argc > 1 ? std::atol(argv[1]) : 20000000L;  // 160 MB per array
    const int reps = argc > 2 ? std::atoi(argv[2]) : 10;
    std::vector<double> a(n), b(n), c(n);
    const double s = 3.0;

#pragma omp parallel for schedule(static)
    for (long i = 0; i < n; ++i) { a[i] = 1.0; b[i] = 2.0; c[i] = 0.0; }

    const char* name[4] = {"copy", "scale", "add", "triad"};
    const double bytes[4] = {2.0 * 8 * n, 2.0 * 8 * n, 3.0 * 8 * n, 3.0 * 8 * n};
    double best[4] = {1e30, 1e30, 1e30, 1e30};

    for (int r = 0; r < reps; ++r) {
        for (int k = 0; k < 4; ++k) {
            auto t0 = std::chrono::steady_clock::now();
            if (k == 0) {
#pragma omp parallel for schedule(static)
                for (long i = 0; i < n; ++i) c[i] = a[i];
            } else if (k == 1) {
#pragma omp parallel for schedule(static)
                for (long i = 0; i < n; ++i) b[i] = s * c[i];
            } else if (k == 2) {
#pragma omp parallel for schedule(static)
                for (long i = 0; i < n; ++i) c[i] = a[i] + b[i];
            } else {
#pragma omp parallel for schedule(static)
                for (long i = 0; i < n; ++i) a[i] = b[i] + s * c[i];
            }
            auto t1 = std::chrono::steady_clock::now();
            if (r > 0) best[k] = std::min(best[k], std::chrono::duration<double>(t1 - t0).count());
        }
    }

    int threads = 1;
#ifdef _OPENMP
    threads = omp_get_max_threads();
#endif
    std::printf("threads=%d n=%ld reps=%d check=%.1f\n", threads, n, reps, a[n / 2]);
    for (int k = 0; k < 4; ++k)
        std::printf("%-5s best_GB_s=%.3f\n", name[k], bytes[k] / best[k] / 1e9);
    return 0;
}
