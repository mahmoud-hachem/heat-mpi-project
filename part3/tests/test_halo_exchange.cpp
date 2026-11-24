#include <mpi.h>
#include <iostream>
#include <vector>
#include <iomanip>
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

    // Create Cartesian topology
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

    // Identify neighbors
    int bottom, top, left, right;

    MPI_Cart_shift(cart_comm, 0, 1,
                   &bottom, &top);

    MPI_Cart_shift(cart_comm, 1, 1,
                   &left, &right);

    // Distribute rows and columns
    int nx, ny;
    int x_start, y_start;

    split_dimension(N-1, dims[1], coords[1],
                    nx, x_start);

    split_dimension(N-1, dims[0], coords[0],
                    ny, y_start);

    // Allocate local block
    int width = nx + 2;
    int height = ny + 2;

    std::vector<double> u(
        static_cast<size_t>(width) * height, -1.0);

    auto idx = [width](int y, int x) {
        return static_cast<size_t>(y) * width + x;
    };

    // Give each global point a unique test value
    for (int y = 1; y <= ny; y++) {
        for (int x = 1; x <= nx; x++) {

            int gx = x_start + x - 1;
            int gy = y_start + y - 1;

            u[idx(y, x)] = gy * (N+1) + gx;
        }
    }

    // Create column datatype
    MPI_Datatype column_type;

    MPI_Type_vector(ny, 1, width,
                    MPI_DOUBLE, &column_type);

    MPI_Type_commit(&column_type);

    // Exchange 1: send top, receive bottom
    MPI_Sendrecv(
        &u[idx(ny, 1)], nx, MPI_DOUBLE, top, 100,
        &u[idx(0, 1)], nx, MPI_DOUBLE, bottom, 100,
        cart_comm, MPI_STATUS_IGNORE
    );

    // Exchange 2: send bottom, receive top
    MPI_Sendrecv(
        &u[idx(1, 1)], nx, MPI_DOUBLE, bottom, 101,
        &u[idx(ny+1, 1)], nx, MPI_DOUBLE, top, 101,
        cart_comm, MPI_STATUS_IGNORE
    );

    // Exchange 3: send right, receive left
    MPI_Sendrecv(
        &u[idx(1, nx)], 1, column_type, right, 102,
        &u[idx(1, 0)], 1, column_type, left, 102,
        cart_comm, MPI_STATUS_IGNORE
    );

    // Exchange 4: send left, receive right
    MPI_Sendrecv(
        &u[idx(1, 1)], 1, column_type, left, 103,
        &u[idx(1, nx+1)], 1, column_type, right, 103,
        cart_comm, MPI_STATUS_IGNORE
    );

    // Automatically verify all cells
    int local_errors = 0;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            int gx = x_start + x - 1;
            int gy = y_start + y - 1;

            bool corner =
                (x == 0 || x == nx+1) &&
                (y == 0 || y == ny+1);

            double expected = -1.0;

            if (!corner &&
                gx > 0 && gx < N &&
                gy > 0 && gy < N) {

                expected = gy * (N+1) + gx;
            }

            if (u[idx(y, x)] != expected) {
                local_errors++;
            }
        }
    }

    int total_errors = 0;

    MPI_Allreduce(&local_errors, &total_errors,
                  1, MPI_INT, MPI_SUM, cart_comm);

    // Display every local block
    for (int r = 0; r < size; r++) {

        MPI_Barrier(cart_comm);

        if (rank == r) {

            std::cout << "\nRank " << rank
                      << " | Owned=" << nx << "x" << ny
                      << " | Errors=" << local_errors
                      << "\n";

            for (int y = height-1; y >= 0; y--) {
                for (int x = 0; x < width; x++) {
                    std::cout << std::setw(5)
                              << u[idx(y, x)];
                }
                std::cout << "\n";
            }

            std::cout << std::flush;
        }
    }

    MPI_Barrier(cart_comm);

    if (rank == 0) {
        if (total_errors == 0)
            std::cout << "PASS: All halo exchanges correct\n";
        else
            std::cout << "FAIL: " << total_errors
                      << " incorrect cells\n";
    }

    MPI_Type_free(&column_type);
    MPI_Comm_free(&cart_comm);
    MPI_Finalize();

    return total_errors == 0 ? 0 : 1;
}
