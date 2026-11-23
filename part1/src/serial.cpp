
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <chrono>

int main(int argc, char* argv[]) {

    // Default parameters
    int N = 4;
    int steps = 10;
    double r = 0.2;
    double requestedT = 0.0;

    std::string caseNumber = "1";
    std::string outputFile = "";

    bool useT = false;
    bool useSteps = false;
    bool debugGrid = false;
    std::string decomp = "serial";

    // Read command-line arguments
    for (int a = 1; a < argc; a++) {
        std::string option = argv[a];

        // Flags that do not take a value
        if (option == "--debug") {
            debugGrid = true;
            continue;
        }
        if (option == "--help") {
            std::cout << "Usage: ./heat [--decomp serial] [--case 1|2|V] "
                      << "[--N intervals] [--r value] "
                      << "[--T time | --steps count] [--out file] [--debug]\n";
            return 0;
        }

        if (a + 1 >= argc) {
            std::cerr << "Missing value for "
                      << option << "\n";
            return 1;
        }

        if (option == "--N") {
            N = std::stoi(argv[++a]);
        }
        else if (option == "--case") {
            caseNumber = argv[++a];
        }
        else if (option == "--steps") {
            steps = std::stoi(argv[++a]);
            useSteps = true;
        }
        else if (option == "--r") {
            r = std::stod(argv[++a]);
        }
        else if (option == "--T") {
            requestedT = std::stod(argv[++a]);
            useT = true;
        }
        else if (option == "--out") {
            outputFile = argv[++a];
        }
        else if (option == "--decomp") {
            decomp = argv[++a];
            if (decomp != "serial") {
                std::cerr << "This Part 1 executable only supports --decomp serial.\n";
                return 1;
            }
        }
        else {
            std::cerr << "Unknown option: "
                      << option << "\n";
            return 1;
        }
    }

    // Validate parameters
    if (N < 2 || steps < 0 ||
        r <= 0.0 || r > 0.25 ||
        !std::isfinite(r)) {

        std::cerr << "Invalid parameters. "
                  << "Use N >= 2, steps >= 0, "
                  << "and 0 < r <= 0.25.\n";
        return 1;
    }

    if (caseNumber != "1" &&
        caseNumber != "2" &&
        caseNumber != "V") {

        std::cerr << "Case must be 1, 2 or V.\n";
        return 1;
    }

    if (useT && useSteps) {
        std::cerr << "Use --T or --steps, not both.\n";
        return 1;
    }

    if (useT && (!std::isfinite(requestedT) ||
                 requestedT < 0.0)) {

        std::cerr << "T must be non-negative.\n";
        return 1;
    }

    // Grid spacing and time step
    const double pi = std::acos(-1.0);

    double h = 1.0 / N;
    double dt = r * h * h;  // alpha = 1

    // Calculate steps from requested T
    if (useT) {

        double calculatedSteps = requestedT / dt;

        if (!std::isfinite(calculatedSteps) ||
            calculatedSteps > 2147483647.0) {

            std::cerr << "Too many iterations.\n";
            return 1;
        }

        // Round UP to respect maximum time step
        steps = static_cast<int>(
            std::ceil(calculatedSteps)
        );

        // Adjust dt and r to reach requested T
        if (steps > 0) {
            dt = requestedT / steps;
            r = dt / (h * h);
        }
    }

    // Create the grid
    int width = N + 1;

    std::vector<double> u(width * width, 0.0);
    std::vector<double> next(width * width, 0.0);

    // Convert (i,j) into a 1D position
    auto idx = [width](int i, int j) {
        return j * width + i;
    };

    // Initial conditions and boundaries
    if (caseNumber == "V") {

        for (int j = 0; j <= N; j++) {
            for (int i = 0; i <= N; i++) {

                double x = i * h;
                double y = j * h;

                u[idx(i, j)] =
                    std::sin(pi * x) *
                    std::sin(pi * y);
            }
        }

        // Case V: zero boundaries
        for (int k = 0; k <= N; k++) {
            u[idx(k, 0)] = 0.0;
            u[idx(k, N)] = 0.0;
            u[idx(0, k)] = 0.0;
            u[idx(N, k)] = 0.0;
        }
    }
    else {

        for (int k = 0; k <= N; k++) {

            if (caseNumber == "1") {

                // Case 1: all four edges hot
                u[idx(k, 0)] = 1.0;
                u[idx(k, N)] = 1.0;
                u[idx(0, k)] = 1.0;
                u[idx(N, k)] = 1.0;
            }
            else {

                // Case 2: top and bottom hot
                // Left and right remain zero
                u[idx(k, 0)] = 1.0;
                u[idx(k, N)] = 1.0;
            }
        }
    }

    next = u;

    // START TIMER: computation only
    auto start = std::chrono::steady_clock::now();

    // Explicit Euler time iterations
    for (int step = 0; step < steps; step++) {

        for (int j = 1; j < N; j++) {
            for (int i = 1; i < N; i++) {

                next[idx(i, j)] =
                    (1.0 - 4.0 * r) * u[idx(i, j)]
                    + r * (
                        u[idx(i - 1, j)]
                        + u[idx(i + 1, j)]
                        + u[idx(i, j - 1)]
                        + u[idx(i, j + 1)]
                    );
            }
        }

        // Swap old and new grids
        u.swap(next);
    }

    // END TIMER
    auto end = std::chrono::steady_clock::now();

    // Computation time in seconds
    double t_comp =
        std::chrono::duration<double>(end - start).count();

    // Actual final simulation time
    double T = steps * dt;

    // Save binary output if requested
    if (!outputFile.empty()) {

        std::ofstream file(
            outputFile, std::ios::binary
        );

        if (!file) {
            std::cerr << "Cannot open output file: "
                      << outputFile << "\n";
            return 1;
        }

        file.write(
            reinterpret_cast<const char*>(u.data()),
            static_cast<std::streamsize>(
                u.size() * sizeof(double)
            )
        );

        if (!file) {
            std::cerr << "Error writing binary file.\n";
            return 1;
        }

        file.close();

        if (debugGrid) {
            std::cerr << "Saved binary file: " << outputFile << "\n";
        }
    }

    // Optional grid display for debugging only (stderr, not the summary stdout).
    if (debugGrid) {
        std::cerr << "Case " << caseNumber << ", N=" << N
                  << ", r=" << r << ", steps=" << steps
                  << ", T=" << T << "\n";
        std::cerr << std::fixed << std::setprecision(3);
        for (int j = 0; j <= N; j++) {
            for (int i = 0; i <= N; i++) {
                std::cerr << std::setw(8) << u[idx(i, j)];
            }
            std::cerr << "\n";
        }
    }

    // Error variables for final summary
    double maxError = 0.0;
    double rmsError = 0.0;
    double l2hError = 0.0;

    // Verification error for Case V
    if (caseNumber == "V") {

        double sumSquaredError = 0.0;

        for (int j = 0; j <= N; j++) {
            for (int i = 0; i <= N; i++) {

                double x = i * h;
                double y = j * h;

                double exact =
                    std::sin(pi * x)
                    * std::sin(pi * y)
                    * std::exp(-2.0 * pi * pi * T);

                double error =
                    std::abs(u[idx(i, j)] - exact);

                maxError = std::max(
                    maxError, error
                );

                sumSquaredError += error * error;
            }
        }

        // RMS error
        rmsError = std::sqrt(
            sumSquaredError / (width * width)
        );

        // Discrete L2 error required by PDF
        l2hError = std::sqrt(
            h * h * sumSquaredError
        );

        // Only the final machine-readable summary is printed to stdout.
    }

    // FINAL SUMMARY: one key=value line
    // In serial mode there is no MPI communication, so step-loop
    // total time equals computation time; setup and output are excluded.
    std::cout << std::setprecision(12)
              << "decomp=" << decomp
              << " P=1"
              << " case=" << caseNumber
              << " N=" << N
              << " r=" << r
              << " steps=" << steps
              << " T=" << T
              << " t_total=" << t_comp
              << " t_comp=" << t_comp
              << " t_comm=0";

    // Errors only have meaning for Case V
    if (caseNumber == "V") {
        std::cout << " err_inf=" << maxError
                  << " err_l2h=" << l2hError;
    }

    std::cout << "\n";

    return 0;
}
