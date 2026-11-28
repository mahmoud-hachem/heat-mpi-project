#include <mpi.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <cmath>
#include <iomanip>
#include <fstream>

int main(int argc, char** argv) {

    MPI_Init(&argc, &argv);

    int rank, P;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    // Step 4 test: Case 1, N=8, 10 steps
    int N = 8;
    int steps = 10;
    double r = 0.2;
    double requestedT = 0.0;
    bool useT = false;
    bool useSteps = false;
    std::string output = "";
    std::string decomp = "strips";
    std::string caseNumber = "1";
    bool debugHalos = false;

    // Read command-line parameters
    for (int k = 1; k < argc; k++) {
        std::string arg = argv[k];

        if (arg == "--debug-halos") {
            debugHalos = true;
        }
        else if (arg == "--help") {
            if (rank == 0) {
                std::cout << "Usage: mpirun -np P ./heat_strips "
                          << "--decomp strips --case 1|2|V "
                          << "--N intervals --r value "
                          << "[--steps count | --T time] "
                          << "[--out path] [--debug-halos]\n";
            }
            MPI_Finalize();
            return 0;
        }
        else if (arg == "--decomp" && k + 1 < argc) {
            decomp = argv[++k];
            if (decomp != "strips") {
                if (rank == 0) {
                    std::cerr << "This executable supports only --decomp strips\n";
                }
                MPI_Finalize();
                return 1;
            }
        }
        else if (arg == "--case" && k + 1 < argc) {
            caseNumber = argv[++k];
        }
        else if (arg == "--N" && k + 1 < argc) {
            N = std::stoi(argv[++k]);
        }
        else if (arg == "--r" && k + 1 < argc) {
            r = std::stod(argv[++k]);
        }
        else if (arg == "--steps" && k + 1 < argc) {
            steps = std::stoi(argv[++k]);
            useSteps = true;
        }
        else if (arg == "--T" && k + 1 < argc) {
            requestedT = std::stod(argv[++k]);
            useT = true;
        }
        else if (arg == "--out" && k + 1 < argc) {
            output = argv[++k];
        }
        else {
            if (rank == 0)
                std::cerr << "Invalid argument: "
                          << arg << std::endl;

            MPI_Finalize();
            return 1;
        }
    }

    if (N < 2 || steps < 0 || r <= 0.0 || r > 0.25 ||
        !std::isfinite(r) ||
        (caseNumber != "1" && caseNumber != "2" &&
         caseNumber != "V")) {
        if (rank == 0)
            std::cerr << "Invalid N, r or steps" << std::endl;

        MPI_Finalize();
        return 1;
    }

    if (useT && useSteps) {
        if (rank == 0) std::cerr << "Use --T or --steps, not both\n";
        MPI_Finalize();
        return 1;
    }

    // Calculate number of steps from final time T
    if (useT) {
        if (!std::isfinite(requestedT) || requestedT < 0.0) {
            if (rank == 0)
                std::cerr << "Invalid T" << std::endl;
            MPI_Finalize();
            return 1;
        }

        double h = 1.0 / N;
        double dt = r * h * h;
        double calculatedSteps = requestedT / dt;

        if (!std::isfinite(calculatedSteps) ||
            calculatedSteps > 2147483647.0) {
            if (rank == 0)
                std::cerr << "Too many iterations" << std::endl;
            MPI_Finalize();
            return 1;
        }

        steps = static_cast<int>(std::ceil(calculatedSteps));

        if (steps > 0) {
            dt = requestedT / steps;
            r = dt / (h * h);
        }
    }

    int interior = N - 1;

    if (P > interior) {
        if (rank == 0)
            std::cerr << "Too many processes\n";
        MPI_Finalize();
        return 1;
    }

    // Step 1: Row decomposition
    int base = interior / P;
    int remainder = interior % P;

    int nlocal = base + (rank < remainder ? 1 : 0);
    int start = 1 + rank * base
                + std::min(rank, remainder);
    int end = start + nlocal - 1;

    // Step 2: Local grid + ghost rows
    int width = N + 1;

    std::vector<double> u((nlocal + 2) * width, 0.0);
    std::vector<double> next((nlocal + 2) * width, 0.0);

    auto idx = [width](int i, int j) {
        return j * width + i;
    };

    // Initialize local grid: Cases 1, 2, V
    const double pi = std::acos(-1.0);
    double h = 1.0 / N;

    // Owned rows
    for (int j = 1; j <= nlocal; j++) {
        int global_j = start + j - 1;

        for (int i = 0; i <= N; i++) {
            if (caseNumber == "V") {
                double x = i * h;
                double y = global_j * h;
                u[idx(i, j)] =
                    std::sin(pi * x) * std::sin(pi * y);
            }
        }

        // Fixed left/right boundaries
        u[idx(0, j)] = (caseNumber == "1") ? 1.0 : 0.0;
        u[idx(N, j)] = (caseNumber == "1") ? 1.0 : 0.0;
    }

    // Physical bottom and top boundaries
    double edge = (caseNumber == "V") ? 0.0 : 1.0;

    if (rank == 0) {
        for (int i = 0; i <= N; i++) {
            u[idx(i, 0)] = edge;
        }
    }

    if (rank == P - 1) {
        for (int i = 0; i <= N; i++) {
            u[idx(i, nlocal + 1)] = edge;
        }
    }

    // Preserve fixed boundaries in both time buffers
    next = u;

    int below = (rank == 0)
                ? MPI_PROC_NULL : rank - 1;

    int above = (rank == P - 1)
                ? MPI_PROC_NULL : rank + 1;

    // Step 7: Timing
    double t_comp = 0.0;
    double t_comm = 0.0;

    MPI_Barrier(MPI_COMM_WORLD);
    double total_start = MPI_Wtime();

    // Step 4: Time-stepping loop
    for (int step = 0; step < steps; step++) {

        // Refresh ghost rows
        double start_comm = MPI_Wtime();
        MPI_Sendrecv(
            &u[idx(0, nlocal)], width, MPI_DOUBLE, above, 10,
            &u[idx(0, 0)],      width, MPI_DOUBLE, below, 10,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE
        );

        MPI_Sendrecv(
            &u[idx(0, 1)],          width, MPI_DOUBLE, below, 20,
            &u[idx(0, nlocal + 1)], width, MPI_DOUBLE, above, 20,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE
        );

        t_comm += MPI_Wtime() - start_comm;

        // Diagnostic only: after the first halo exchange, print each
        // rank's complete local array, including physical/ghost rows.
        // The barriers keep the output grouped by rank. This does not
        // change any temperatures used by the numerical update.
        if (debugHalos && step == 0) {
            for (int printer = 0; printer < P; printer++) {
                MPI_Barrier(MPI_COMM_WORLD);
                if (rank == printer) {
                    std::cout << "HALO_DEBUG rank=" << rank
                              << " owns_global_rows=" << start
                              << "-" << end
                              << " nlocal=" << nlocal << "\n";
                    for (int local_j = 0; local_j <= nlocal + 1; local_j++) {
                        int global_j = start + local_j - 1;
                        std::cout << "  local=" << local_j
                                  << " global=" << global_j
                                  << " type="
                                  << (local_j == 0 ? "bottom_ghost" :
                                     local_j == nlocal + 1 ? "top_ghost" : "owned")
                                  << " values=" << std::fixed
                                  << std::setprecision(3);
                        for (int i = 0; i <= N; i++) {
                            std::cout << ' ' << u[idx(i, local_j)];
                        }
                        std::cout << "\n";
                    }
                    std::cout << std::flush;
                }
            }
            MPI_Barrier(MPI_COMM_WORLD);
        }

        // Same 5-point stencil as Serial
        double start_comp = MPI_Wtime();
        for (int j = 1; j <= nlocal; j++) {
            for (int i = 1; i < N; i++) {

                next[idx(i, j)] =
                    (1.0 - 4.0 * r) * u[idx(i, j)]
                    + r * (
                        u[idx(i - 1, j)]
                        + u[idx(i + 1, j)]
                        + u[idx(i, j - 1)]
                        + u[idx(i, j + 1)]
                    );
            }
        }

        u.swap(next);
        t_comp += MPI_Wtime() - start_comp;
    }

    double t_total = MPI_Wtime() - total_start;

    double max_comp = 0.0;
    double max_comm = 0.0;
    double max_total = 0.0;

    MPI_Reduce(&t_comp, &max_comp, 1, MPI_DOUBLE,
               MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Reduce(&t_comm, &max_comm, 1, MPI_DOUBLE,
               MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Reduce(&t_total, &max_total, 1, MPI_DOUBLE,
               MPI_MAX, 0, MPI_COMM_WORLD);

    // Independent MPI_MAX reductions report the slowest rank for each metric.
    // They do not need to add up exactly because their maxima may be on
    // different ranks. Debug output is not suitable for benchmarking.

    int center_i = N / 2;

    // Find the center point (global i=4, j=4)
    double myCenter = 0.0;

    if (start <= center_i && center_i <= end) {
        int local_j = center_i - start + 1;
        myCenter = u[idx(center_i, local_j)];
    }

    double center = 0.0;

    MPI_Reduce(
        &myCenter, &center, 1, MPI_DOUBLE,
        MPI_SUM, 0, MPI_COMM_WORLD
    );

    // The center has been computed for diagnostics; omit it from stdout
    // so the ordinary run prints exactly one machine-readable summary.


    // Step 5: Gather all owned rows on Rank 0, only when the full grid is
    // needed (binary output or the Case V error check). Like the blocks
    // solver, a plain benchmark run skips it: at N=8192 over Tailscale the
    // gather moves hundreds of MB and outlasts the benchmark timeout. All
    // ranks parse the same arguments, so they agree on this collective.
    const bool need_full = !output.empty() || caseNumber == "V";

    std::vector<int> counts(P), displs(P);

    for (int p = 0; p < P; p++) {
        int rows = base + (p < remainder ? 1 : 0);
        int first = 1 + p * base + std::min(p, remainder);

        counts[p] = rows * width;
        displs[p] = first * width;
    }

    std::vector<double> full;

    if (rank == 0 && need_full) {
        full.resize(static_cast<size_t>(width) * width, 0.0);

        // Restore physical bottom and top boundaries
        double edge = (caseNumber == "V") ? 0.0 : 1.0;

        for (int i = 0; i <= N; i++) {
            full[idx(i, 0)] = edge;
            full[idx(i, N)] = edge;
        }
    }

    if (need_full) {
        MPI_Gatherv(
            &u[idx(0, 1)], nlocal * width, MPI_DOUBLE,
            rank == 0 ? full.data() : nullptr,
            counts.data(), displs.data(), MPI_DOUBLE,
            0, MPI_COMM_WORLD
        );
    }

    // Gather completes on rank 0; keep stdout clean for automated parsing.



    double err_inf = 0.0;
    double err_l2h = 0.0;

    // Case V: Calculate error against exact solution
    if (rank == 0 && caseNumber == "V") {

        double h = 1.0 / N;
        double T = steps * r * h * h;
        double sum_sq = 0.0;

        const double pi = std::acos(-1.0);

        for (int j = 1; j < N; j++) {
            for (int i = 1; i < N; i++) {

                double x = i * h;
                double y = j * h;

                double exact =
                    std::sin(pi * x) *
                    std::sin(pi * y) *
                    std::exp(-2.0 * pi * pi * T);

                double error =
                    std::abs(full[idx(i, j)] - exact);

                err_inf = std::max(err_inf, error);
                sum_sq += error * error;
            }
        }

        err_l2h = h * std::sqrt(sum_sq);
    }

    // Step 6: Write the full grid to a binary file, only if requested.
    if (rank == 0 && !output.empty()) {
        std::ofstream file(output,
                           std::ios::binary);

        if (!file) {
            std::cerr << "Cannot open output file: " << output << "\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        } else {
            file.write(
                reinterpret_cast<const char*>(full.data()),
                full.size() * sizeof(double)
            );

            if (!file) {
                std::cerr << "Error writing binary file: " << output << "\n";
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
    }

    // Final single-line output, consistent with the serial solver.
    if (rank == 0) {
        const double T = steps * r * h * h;
        std::cout << std::defaultfloat << std::setprecision(12)
                  << "decomp=" << decomp
                  << " P=" << P
                  << " case=" << caseNumber
                  << " N=" << N
                  << " r=" << r
                  << " steps=" << steps
                  << " T=" << T
                  << " t_total=" << max_total
                  << " t_comp=" << max_comp
                  << " t_comm=" << max_comm;
        if (caseNumber == "V") {
            std::cout << " err_inf=" << err_inf
                      << " err_l2h=" << err_l2h;
        }
        std::cout << "\n";
    }

    MPI_Finalize();
    return 0;
}
