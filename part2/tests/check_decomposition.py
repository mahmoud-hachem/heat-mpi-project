"""Check the row partitioning implemented in teammate's strips.cpp."""

def partition(N, P):
    interior = N - 1
    assert 1 <= P <= interior
    base = interior // P
    remainder = interior % P
    chunks = []
    for rank in range(P):
        nlocal = base + (1 if rank < remainder else 0)
        start = 1 + rank * base + min(rank, remainder)
        end = start + nlocal - 1
        chunks.append((rank, start, end, nlocal))
    return chunks

for N in (8, 10):
    for P in (1, 2, 3, 4):
        chunks = partition(N, P)
        all_rows = [j for _, start, end, _ in chunks for j in range(start, end + 1)]
        assert all_rows == list(range(1, N)), (N, P, all_rows)
        sizes = [nlocal for _, _, _, nlocal in chunks]
        assert max(sizes) - min(sizes) <= 1
        print(f'N={N}, P={P}:')
        for rank, start, end, nlocal in chunks:
            print(f'  rank {rank}: global rows {start}-{end}, own {nlocal}, allocate {nlocal + 2} rows')
print('\nPASS: all partitions cover each interior row once, with balanced strip sizes.')
