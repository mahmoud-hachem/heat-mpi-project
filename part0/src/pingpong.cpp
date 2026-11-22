#include <mpi.h>
#include <iostream>
#include <vector>
#include <iomanip>
#include <cstdlib>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != 2) {
        if (rank == 0)
            std::cerr << "Exactly 2 MPI processes required\n";
        MPI_Finalize();
        return 1;
    }

    int repetitions = argc > 1 ? std::atoi(argv[1]) : 10;
    if (repetitions < 1) {
        MPI_Finalize();
        return 1;
    }

    std::vector<char> buffer(16 * 1024 * 1024, 0);

    if (rank == 0)
        std::cout << "bytes,roundtrip_ms,oneway_ms\n";

    for (int bytes = 8; bytes <= 16 * 1024 * 1024; bytes *= 2) {

        // Warm-up exchanges
        for (int k = 0; k < 2; ++k) {
            if (rank == 0) {
                MPI_Send(buffer.data(), bytes, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
                MPI_Recv(buffer.data(), bytes, MPI_BYTE, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            } else {
                MPI_Recv(buffer.data(), bytes, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(buffer.data(), bytes, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);
        double start = MPI_Wtime();

        for (int k = 0; k < repetitions; ++k) {
            if (rank == 0) {
                MPI_Send(buffer.data(), bytes, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
                MPI_Recv(buffer.data(), bytes, MPI_BYTE, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            } else {
                MPI_Recv(buffer.data(), bytes, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(buffer.data(), bytes, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
            }
        }

        double elapsed = MPI_Wtime() - start;

        if (rank == 0) {
            double roundtrip_ms = elapsed * 1000.0 / repetitions;
            std::cout << bytes << ","
                      << std::fixed << std::setprecision(6)
                      << roundtrip_ms << ","
                      << roundtrip_ms / 2.0 << std::endl;
        }
    }

    MPI_Finalize();
    return 0;
}
