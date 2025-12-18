import math
from pathlib import Path

DATA_DIR = Path("./data_doms")

def build_bloom():
    # ---- CONFIG ----
    BITS = 12_500_000
    HASHES = 7
    OUT_FILE =  f"data/bloom.bin"
    HOSTS_GLOB = "hosts_*"

    BYTES = (BITS + 7) // 8
    bitarray = bytearray(BYTES)
    
    ### Ensure ESP files use same hashing
    BLOOM_CHECK_H_FILE = Path("./include/BloomCheck.h")    
    with open(BLOOM_CHECK_H_FILE, "r", encoding="utf-8") as f:
        lines = f.readlines()

    with open(BLOOM_CHECK_H_FILE, "w", encoding="utf-8") as f:
        for line in lines:
            if "#define BLOOM_BITS" in line:                
                f.write(f"#define BLOOM_BITS {BITS}\n")
            elif "#define BLOOM_HASHES" in line:
                f.write(f"#define BLOOM_HASHES {HASHES}\n")
            else:
                f.write(line)

    def set_bit(i):
        bitarray[i >> 3] |= 1 << (i & 7)


    def bloom_hash(domain: str, seed: int) -> int:
        h = seed
        for b in domain.encode("utf-8"):
            h ^= b
            h = (h * 0x5bd1e995) & 0xFFFFFFFF
            h ^= (h >> 15)
        return h

    def hashes(domain: str):
        for i in range(HASHES):
            h = bloom_hash(domain, 0x9747b28c + i)
            yield h % BITS


    count = 0
    for path in DATA_DIR.glob(HOSTS_GLOB):
        if not path.is_file():
            continue

        with path.open("r", encoding="utf-8") as f:
            for line in f:
                d = line.strip()
                if not d or d == ",@@@":
                    continue

                for h in hashes(d):
                    set_bit(h)

                count += 1
                
    n = float(count) #num domains
    m = float(BITS) #num bits
    k = float(HASHES) #num hashes

    p = float(1.0 - math.exp(-k * n / m))**k

    print(f"False Positives %: {p}")
    print(f"False Positives Estimate: {p*n}")

    with open(OUT_FILE, "wb") as f:
        f.write(bitarray)

    print(f"Bloom built: {count} domains")
    print(f"Size: {BYTES / 1024:.1f} KB")

def main():
    build_bloom()
    
if __name__ == "__main__":
    main()