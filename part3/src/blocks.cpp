#include <algorithm>
#include <limits>
#include <fstream>
#include <mpi.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <string>
#include <cstdlib>

void split_dimension(int total, int parts, int coord,
                     int& count, int& start) {

    int base = total / parts;
    int remainder = total % parts;

    count = base + (coord < remainder ? 1 : 0);

    start = 1 + coord * base
          + (coord < remainder ? coord : remainder);
}



int main(int argc, char** argv) {

    MPI_Init(&argc, &argv);

    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int N = 8;
    int steps = 10;
    double r = 0.2;
    double requestedT = 0.0;
    bool useT = false;
    bool useSteps = false;
    std::string output = "";
    std::string decomp = "blocks";
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
                std::cout << "Usage: mpirun -np P ./heat_blocks "
                          << "--decomp blocks --case 1|2|V "
                          << "--N intervals --r value "
                          << "[--steps count | --T time] "
                          << "[--out path] [--debug-halos]\n";
            }
            MPI_Finalize();
            return 0;
        }
        else if (arg == "--decomp" && k + 1 < argc) {
            decomp = argv[++k];
            if (decomp != "blocks") {
                if (rank == 0) {
                    std::cerr << "This executable supports only --decomp blocks\n";
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

    const std::string& test_case = caseNumber;

    if (N > std::numeric_limits<int>::max() - 2) {
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int dims[2] = {0, 0};
    MPI_Dims_create(size, 2, dims);

    if (N < 2 || dims[0] > N-1 || dims[1] > N-1 ||
        (test_case != "1" && test_case != "2" &&
         test_case != "V")) {
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int periods[2] = {0, 0};
    MPI_Comm cart_comm;

    MPI_Cart_create(MPI_COMM_WORLD, 2, dims,
                    periods, 0, &cart_comm);

    int coords[2];

    MPI_Comm_rank(cart_comm, &rank);
    MPI_Cart_coords(cart_comm, rank, 2, coords);

    int nx, ny;
    int x_start, y_start;

    split_dimension(N-1, dims[1], coords[1],
                    nx, x_start);

    split_dimension(N-1, dims[0], coords[0],
                    ny, y_start);

    int width = nx + 2;
    int height = ny + 2;

    std::vector<double> u(static_cast<size_t>(width) * height, 0.0);
    std::vector<double> u_next(static_cast<size_t>(width) * height, 0.0);

    auto idx = [width](int y, int x) {
        return static_cast<size_t>(y) * width + x;
    };


    const double pi = std::acos(-1.0);
    const double h = 1.0 / N;

    // Initialize temperatures
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            int gx = x_start + x - 1;
            int gy = y_start + y - 1;

            bool owned = x >= 1 && x <= nx
                      && y >= 1 && y <= ny;

            double value = 0.0;

            // Top and bottom physical boundaries
            if (gy == 0 || gy == N) {
                value = test_case == "V" ? 0.0 : 1.0;
            }

            // Left and right physical boundaries
            else if (gx == 0 || gx == N) {
                value = test_case == "1" ? 1.0 : 0.0;
            }

            // Case V initial interior temperature
            else if (owned && test_case == "V") {
                value = std::sin(pi * (gx * h))
                      * std::sin(pi * (gy * h));
            }

            u[idx(y, x)] = value;
        }
    }


    // Both buffers must preserve physical boundaries
    u_next = u;

    // Identify neighbors
    int bottom, top, left, right;

    MPI_Cart_shift(cart_comm, 0, 1, &bottom, &top);
    MPI_Cart_shift(cart_comm, 1, 1, &left, &right);

    // Create column datatype
    MPI_Datatype column_type;

    MPI_Type_vector(ny, 1, width,
                    MPI_DOUBLE, &column_type);

    MPI_Type_commit(&column_type);

    double t_comm = 0.0;
    double t_comp = 0.0;

    MPI_Barrier(cart_comm);

    double total_start = MPI_Wtime();

    // Main time-stepping loop
    for (int step = 0; step < steps; step++) {

        double comm_start = MPI_Wtime();

        // Exchange top and bottom rows
        MPI_Sendrecv(
            &u[idx(ny, 1)], nx, MPI_DOUBLE, top, 100,
            &u[idx(0, 1)], nx, MPI_DOUBLE, bottom, 100,
            cart_comm, MPI_STATUS_IGNORE
        );

        MPI_Sendrecv(
            &u[idx(1, 1)], nx, MPI_DOUBLE, bottom, 101,
            &u[idx(ny+1, 1)], nx, MPI_DOUBLE, top, 101,
            cart_comm, MPI_STATUS_IGNORE
        );

        // Exchange left and right columns
        MPI_Sendrecv(
            &u[idx(1, nx)], 1, column_type, right, 102,
            &u[idx(1, 0)], 1, column_type, left, 102,
            cart_comm, MPI_STATUS_IGNORE
        );

        MPI_Sendrecv(
            &u[idx(1, 1)], 1, column_type, left, 103,
            &u[idx(1, nx+1)], 1, column_type, right, 103,
            cart_comm, MPI_STATUS_IGNORE
        );

        t_comm += MPI_Wtime() - comm_start;

        if (debugHalos && step == 0) {
            for (int p = 0; p < size; p++) {
                MPI_Barrier(cart_comm);

                if (rank == p) {
                    std::cout << "HALO_DEBUG rank=" << rank
                              << " nx=" << nx
                              << " ny=" << ny
                              << " x_start=" << x_start
                              << " y_start=" << y_start
                              << std::endl;

                    for (int y = height-1; y >= 0; y--) {
                        for (int x = 0; x < width; x++) {
                            std::cout << std::fixed
                                      << std::setprecision(4)
                                      << u[idx(y,x)] << " ";
                        }
                        std::cout << "\n";
                    }
                    std::cout << std::flush;
                }
            }

            MPI_Barrier(cart_comm);
        }

        double comp_start = MPI_Wtime();

        // Update owned interior points
        for (int y = 1; y <= ny; y++) {
            for (int x = 1; x <= nx; x++) {

                double C = u[idx(y, x)];
                double L = u[idx(y, x-1)];
                double R = u[idx(y, x+1)];
                double B = u[idx(y-1, x)];
                double T = u[idx(y+1, x)];

                u_next[idx(y, x)] =
                    (1.0 - 4.0*r)*C
                    + r*(L + R + B + T);
            }
        }

        // Move to the next time level
        u.swap(u_next);

        t_comp += MPI_Wtime() - comp_start;
    }

    double t_total = MPI_Wtime() - total_start;

    double max_total = 0.0;
    double max_comp = 0.0;
    double max_comm = 0.0;

    MPI_Reduce(&t_total, &max_total, 1,
               MPI_DOUBLE, MPI_MAX, 0, cart_comm);

    MPI_Reduce(&t_comp, &max_comp, 1,
               MPI_DOUBLE, MPI_MAX, 0, cart_comm);

    MPI_Reduce(&t_comm, &max_comm, 1,
               MPI_DOUBLE, MPI_MAX, 0, cart_comm);

    MPI_Type_free(&column_type);

    const double T = steps * r * h * h;

    // Case V numerical errors
    double err_inf = 0.0;
    double err_l2h = 0.0;

    if (test_case == "V") {

        double local_max = 0.0;
        double local_sum = 0.0;

        for (int y = 1; y <= ny; y++) {
            for (int x = 1; x <= nx; x++) {

                double gx = (x_start + x - 1) * h;
                double gy = (y_start + y - 1) * h;

                double exact =
                    std::sin(pi * gx)
                    * std::sin(pi * gy)
                    * std::exp(-2.0 * pi * pi * T);

                double error =
                    std::abs(u[idx(y,x)] - exact);

                local_max = std::max(local_max, error);
                local_sum += error * error;
            }
        }

        double global_sum = 0.0;

        MPI_Reduce(&local_max, &err_inf, 1,
                   MPI_DOUBLE, MPI_MAX, 0, cart_comm);

        MPI_Reduce(&local_sum, &global_sum, 1,
                   MPI_DOUBLE, MPI_SUM, 0, cart_comm);

        if (rank == 0) {
            err_l2h = h * std::sqrt(global_sum);
        }
    }

    if (!output.empty()) {

        if (static_cast<long long>(N-1) * (N-1)
            > std::numeric_limits<int>::max()) {
            MPI_Abort(cart_comm, 1);
        }

        // Pack owned interior points only
        std::vector<double> packed(nx * ny);

        int k = 0;

        for (int y = 1; y <= ny; y++) {
            for (int x = 1; x <= nx; x++) {
                packed[k++] = u[idx(y, x)];
            }
        }

        // Gather block sizes
        int local_count = nx * ny;

        std::vector<int> counts(size, 0);
        std::vector<int> displs(size, 0);

        MPI_Gather(&local_count, 1, MPI_INT,
                   counts.data(), 1, MPI_INT,
                   0, cart_comm);

        std::vector<double> gathered;

        if (rank == 0) {

            int total = 0;

            for (int p = 0; p < size; p++) {
                displs[p] = total;
                total += counts[p];
            }

            gathered.resize(total);
        }

        // Gather all owned blocks
        MPI_Gatherv(
            packed.data(), local_count, MPI_DOUBLE,
            gathered.data(), counts.data(),
            displs.data(), MPI_DOUBLE,
            0, cart_comm
        );

        if (rank == 0) {

            // Allocate the complete global grid
            int global_width = N + 1;

            std::vector<double> global(
                static_cast<size_t>(global_width)
                * global_width, 0.0);

            auto global_idx = [global_width](int y, int x) {
                return static_cast<size_t>(y)
                       * global_width + x;
            };

            // Initialize physical boundaries
            for (int y = 0; y <= N; y++) {
                for (int x = 0; x <= N; x++) {

                    if (y == 0 || y == N) {
                        global[global_idx(y,x)] =
                            test_case == "V" ? 0.0 : 1.0;
                    }

                    else if (x == 0 || x == N) {
                        global[global_idx(y,x)] =
                            test_case == "1" ? 1.0 : 0.0;
                    }
                }
            }

            // Reconstruct every block
            for (int p = 0; p < size; p++) {

                int pc[2];

                MPI_Cart_coords(cart_comm, p, 2, pc);

                int bx, by, sx, sy;

                split_dimension(N-1, dims[1], pc[1],
                                bx, sx);

                split_dimension(N-1, dims[0], pc[0],
                                by, sy);

                int offset = displs[p];

                for (int y = 0; y < by; y++) {
                    for (int x = 0; x < bx; x++) {

                        global[global_idx(sy+y, sx+x)] =
                            gathered[offset++];
                    }
                }
            }

            std::ofstream file(output, std::ios::binary);

            file.write(
                reinterpret_cast<const char*>(global.data()),
                global.size() * sizeof(double)
            );

            file.close();

            if (!file) {
                std::cerr << "Binary output failed"
                          << std::endl;
                MPI_Abort(cart_comm, 1);
            }

        }

    }

    if (rank == 0) {

        std::cout << std::defaultfloat
                  << std::setprecision(12)
                  << "decomp=blocks"
                  << " P=" << size
                  << " Py=" << dims[0]
                  << " Px=" << dims[1]
                  << " case=" << test_case
                  << " N=" << N
                  << " r=" << r
                  << " steps=" << steps
                  << " T=" << T
                  << " t_total=" << max_total
                  << " t_comp=" << max_comp
                  << " t_comm=" << max_comm;

        if (test_case == "V") {
            std::cout << " err_inf=" << err_inf
                      << " err_l2h=" << err_l2h;
        }

        std::cout << std::endl;
    }

    MPI_Comm_free(&cart_comm);
    MPI_Finalize();

    return 0;
}
