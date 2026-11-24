#include <mpi.h>
#include <iostream>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int dims[2] = {0, 0};
    MPI_Dims_create(size, 2, dims);

    int periods[2] = {0, 0};
    MPI_Comm cart_comm;

    MPI_Cart_create(MPI_COMM_WORLD, 2, dims,
                    periods, 0, &cart_comm);

    int rank;
    int coords[2];

    MPI_Comm_rank(cart_comm, &rank);
    MPI_Cart_coords(cart_comm, rank, 2, coords);

    int bottom, top, left, right;

    // Vertical neighbors (dimension 0)
    MPI_Cart_shift(cart_comm, 0, 1,
                   &bottom, &top);

    // Horizontal neighbors (dimension 1)
    MPI_Cart_shift(cart_comm, 1, 1,
                   &left, &right);

    for (int i = 0; i < size; i++) {
        MPI_Barrier(cart_comm);

        if (rank == i) {
            std::cout << "Rank " << rank
                      << " | coords=("
                      << coords[0] << ","
                      << coords[1] << ")"
                      << " | bottom=" << bottom
                      << " | top=" << top
                      << " | left=" << left
                      << " | right=" << right
                      << std::endl;
        }
    }

    MPI_Comm_free(&cart_comm);
    MPI_Finalize();

    return 0;
}

