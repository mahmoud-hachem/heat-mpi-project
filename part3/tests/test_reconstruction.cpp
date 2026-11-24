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

    int N = argc > 1 ? std::atoi(argv[1]) : 8;
    std::string test_case = argc > 2 ? argv[2] : "1";

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

    int rank;
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

    std::vector<double> u(width * height, 0.0);
    std::vector<double> u_next(width * height, 0.0);

    auto idx = [width](int y, int x) {
        return y * width + x;
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

    int steps = argc > 3 ? std::atoi(argv[3]) : 1;
    const double r = 0.2;

    if (steps < 1) {
        MPI_Abort(cart_comm, 1);
    }

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

    if (rank == 0) {
        std::cout << std::setprecision(12)
                  << "decomp=blocks"
                  << " P=" << size
                  << " Py=" << dims[0]
                  << " Px=" << dims[1]
                  << " case=" << test_case
                  << " N=" << N
                  << " steps=" << steps
                  << " t_total=" << max_total
                  << " t_comp=" << max_comp
                  << " t_comm=" << max_comm
                  << std::endl;
    }

    MPI_Type_free(&column_type);

    if (rank == 0) {
        std::cout << "Completed " << steps
                  << " time steps" << std::endl;
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

        std::cout << "Reconstructed grid: "
                  << global_width << "x"
                  << global_width << std::endl;

        std::cout << "Gathered interior points: "
                  << gathered.size() << std::endl;

        // Display the reconstructed grid
        if (N <= 16) {

            std::cout << std::fixed
                      << std::setprecision(4);

            for (int y = N; y >= 0; y--) {
                for (int x = 0; x <= N; x++) {
                    std::cout
                        << global[global_idx(y,x)]
                        << " ";
                }
                std::cout << std::endl;
            }
        }

        // Save a binary file for correctness testing
        std::string output = argc > 4
            ? argv[4]
            : "results/reconstructed_test.bin";

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

        std::cout << "Binary saved: "
                  << output << std::endl;
    }

    MPI_Comm_free(&cart_comm);
    MPI_Finalize();

    return 0;
}
