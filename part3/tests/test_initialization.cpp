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

    for (int r = 0; r < size; r++) {

        MPI_Barrier(cart_comm);

        if (rank == r) {

            std::cout << "\nCase " << test_case
                      << " | Rank " << rank
                      << " | Owned=" << nx << "x" << ny
                      << std::endl;

            std::cout << std::fixed
                      << std::setprecision(4);

            for (int y = height-1; y >= 0; y--) {

                for (int x = 0; x < width; x++) {
                    std::cout << u[idx(y, x)] << " ";
                }

                std::cout << "\n";
            }

            std::cout << std::flush;
        }
    }

    MPI_Barrier(cart_comm);

    MPI_Comm_free(&cart_comm);
    MPI_Finalize();

    return 0;
}
