#include <mpi.h>
#include <iostream>
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

    // Split interior columns
    split_dimension(N-1, dims[1], coords[1],
                    nx, x_start);

    // Split interior rows
    split_dimension(N-1, dims[0], coords[0],
                    ny, y_start);

    int local_points = nx * ny;
    int total_points = 0;

    MPI_Reduce(&local_points, &total_points,
               1, MPI_INT, MPI_SUM, 0, cart_comm);

    for (int i = 0; i < size; i++) {

        MPI_Barrier(cart_comm);

        if (rank == i) {
            std::cout << "Rank " << rank
                      << " | nx=" << nx
                      << " | ny=" << ny
                      << " | x=[" << x_start
                      << "," << x_start+nx-1 << "]"
                      << " | y=[" << y_start
                      << "," << y_start+ny-1 << "]"
                      << " | points=" << local_points
                      << std::endl;
        }
    }

    MPI_Barrier(cart_comm);

    if (rank == 0) {
        std::cout << "Total owned points: "
                  << total_points << std::endl;

        std::cout << "Expected points: "
                  << (N-1)*(N-1) << std::endl;

        if (total_points == (N-1)*(N-1))
            std::cout << "PASS: Point count matches\n";
        else
            std::cout << "FAIL: Incorrect point count\n";
    }

    MPI_Comm_free(&cart_comm);
    MPI_Finalize();

    return 0;
}

