
import struct
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

N = 16
width = N + 1

# Read the unstable grid from the binary file
with open("results/instability_1000.bin", "rb") as f:
    data = f.read()

values = struct.unpack("<" + str(width * width) + "d", data)

# Convert the flat array to a 2D grid
grid = [
    values[j * width:(j + 1) * width]
    for j in range(width)
]

# Create the heatmap
plt.figure(figsize=(7, 6))

plt.imshow(
    grid,
    origin="lower",
    extent=[0, 1, 0, 1],
    cmap="seismic",
    interpolation="nearest"
)

plt.colorbar(label="Numerical temperature")
plt.xlabel("x")
plt.ylabel("y")
plt.title("Numerical Instability - r=0.2625, N=16, 1000 steps")

plt.tight_layout()
plt.savefig("results/instability.png", dpi=200)

print("Saved: results/instability.png")
