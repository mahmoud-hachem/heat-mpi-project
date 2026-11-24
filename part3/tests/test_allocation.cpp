#include <mpi.h>
#include <iostream>
#include <vector>
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

    int dims[2] = {0, 0};
    MPI_Dims_create(size, 2, dims);

    if (N < 2 || dims[0] > N-1 || dims[1] > N-1) {
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

    // Include ghost layers
    int width = nx + 2;
    int height = ny + 2;

    // Allocate two temperature arrays
    std::vector<double> u(width * height, 0.0);
    std::vector<double> u_next(width * height, 0.0);

    // Convert 2D coordinates to a 1D index
    auto idx = [width](int y, int x) {
        return y * width + x;
    };

    // Mark owned points with the rank number
    // Ghost cells remain zero
    for (int y = 1; y <= ny; y++) {
        for (int x = 1; x <= nx; x++) {
            u[idx(y, x)] = rank + 1;
        }
    }

    for (int r = 0; r < size; r++) {

        MPI_Barrier(cart_comm);

        if (rank == r) {

            std::cout << "\nRank " << rank
                      << " | Owned=" << nx << "x" << ny
                      << " | Allocated=" << width
                      << "x" << height
                      << " | Values=" << u.size()
                      << std::endl;

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

