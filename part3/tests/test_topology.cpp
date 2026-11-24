#include <mpi.h>
#include <iostream>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Determine the process grid dimensions
    int dims[2] = {0, 0};

    MPI_Dims_create(size, 2, dims);

    // Non-periodic boundaries
    int periods[2] = {0, 0};

    // Preserve the original rank numbering
    int reorder = 0;

    MPI_Comm cart_comm;

    MPI_Cart_create(
        MPI_COMM_WORLD,
        2,
        dims,
        periods,
        reorder,
        &cart_comm
    );

    int cart_rank;
    int coords[2];

    MPI_Comm_rank(cart_comm, &cart_rank);
    MPI_Cart_coords(cart_comm, cart_rank, 2, coords);

    if (cart_rank == 0) {
        std::cout << "Process grid: "
                  << dims[0] << " x "
                  << dims[1] << std::endl;
    }

    // Print ranks in order
    for (int i = 0; i < size; i++) {
        MPI_Barrier(cart_comm);

        if (cart_rank == i) {
            std::cout << "Rank " << cart_rank
                      << " | Coordinates: ("
                      << coords[0] << ", "
                      << coords[1] << ")"
                      << std::endl;
        }
    }

    MPI_Comm_free(&cart_comm);
    MPI_Finalize();

    return 0;
}

