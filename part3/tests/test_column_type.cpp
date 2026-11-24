#include <mpi.h>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {

    MPI_Init(&argc, &argv);

    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != 2) {
        if (rank == 0)
            std::cerr << "Run with exactly 2 processes\n";

        MPI_Finalize();
        return 1;
    }

    int nx = 4;
    int ny = 4;

    int width = nx + 2;
    int height = ny + 2;

    std::vector<double> u(width * height, 0.0);

    auto idx = [width](int y, int x) {
        return y * width + x;
    };

    // Initialize owned points
    for (int y = 1; y <= ny; y++) {
        for (int x = 1; x <= nx; x++) {
            u[idx(y, x)] = 10*y + x;
        }
    }

    // Create the MPI column datatype
    MPI_Datatype column_type;

    MPI_Type_vector(ny, 1, width,
                    MPI_DOUBLE, &column_type);

    MPI_Type_commit(&column_type);

    // Send the first interior column
    if (rank == 0) {

        MPI_Send(&u[idx(1, 1)],
                 1, column_type,
                 1, 100, MPI_COMM_WORLD);
    }

    // Receive directly into the right ghost column
    if (rank == 1) {

        MPI_Recv(&u[idx(1, nx+1)],
                 1, column_type,
                 0, 100, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        std::cout << "Rank 1 after receiving column:\n";

        for (int y = height-1; y >= 0; y--) {
            for (int x = 0; x < width; x++) {
                std::cout << u[idx(y,x)] << "\t";
            }
            std::cout << "\n";
        }

        bool correct = true;

        for (int y = 1; y <= ny; y++) {
            if (u[idx(y, nx+1)] != 10*y+1)
                correct = false;
        }

        if (correct)
            std::cout << "PASS: Column exchange correct\n";
        else
            std::cout << "FAIL: Column exchange incorrect\n";
    }

    // Release the derived datatype
    MPI_Type_free(&column_type);

    MPI_Finalize();

    return 0;
}
