import os
import re
import sys
import requests
from collections import defaultdict
from pathlib import Path

# ==========================
# Configuration
# ==========================
URLS = [
    "https://easylist.to/easylist/easylist.txt",
    "https://pgl.yoyo.org/adservers/serverlist.php?hostformat=hosts&mimetype=plaintext&useip=0.0.0.0",
    "https://raw.githubusercontent.com/Perflyst/PiHoleBlocklist/master/android-tracking.txt",
    "https://raw.githubusercontent.com/RPiList/specials/master/Blocklisten/Win10Telemetry",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_1.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_2.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_4.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_7.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_11.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_18.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_30.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_48.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_55.txt",
]

DATA_DIR = Path("./data_doms")
TIMEOUT = 20

# ==========================
# Regex patterns
# ==========================
DOMAIN_RE = re.compile(
    r"^(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+"
    r"[a-z]{2,}$"
)


# ==========================
# Helpers
# ==========================
def normalize_domain(line: str) -> str | None:
    line = line.strip().lower()

    if not line:
        return None

    # Comments
    if line.startswith(("!", "#")):
        return None

    # Cosmetic / element hiding
    if "##" in line or "#@#" in line or "#?#" in line:
        return None

    # Regex rules
    if line.startswith("/") and line.endswith("/"):
        return None

    # Reject modifiers
    if "$" in line:
        return None

    # uBO network rules
    if line.startswith("||"):
        rule = line[2:]

        # Reject paths, queries, fragments
        if "/" in rule or "?" in rule or "#" in rule:
            return None

        # Strip trailing ^
        if rule.endswith("^"):
            rule = rule[:-1]

        domain = rule

    # Hosts format
    elif line.startswith("0.0.0.0 "):
        parts = line.split()
        if len(parts) != 2:
            return None
        domain = parts[1]

    # Bare domain only
    else:
        # Reject anything containing slashes or spaces
        if "/" in line or " " in line:
            return None
        domain = line

    # Must contain a dot
    if "." not in domain:
        return None

    # Strict hostname validation
    if not DOMAIN_RE.fullmatch(domain):
        return None

    return domain


def build_bloom():
    import hashlib
    import math
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

# ==========================
# Main
# ==========================
def main():
    domains = set()
    total_lines = 0
    matched_lines = 0

    for url in URLS:
        print(f"\nDownloading: {url}")
        try:
            r = requests.get(url, timeout=TIMEOUT)
            r.raise_for_status()
        except requests.RequestException as e:
            print(f"  ❌ Failed: {e}")
            continue

        lines = r.text.splitlines()
        print(f"  ✔ {len(lines):,} lines")

        for line in lines:
            total_lines += 1
            line = line.strip()

            if not line or line.startswith(("#", "!")):
                continue

            matched_lines += 1
            domain = normalize_domain(line)
            if domain:
                domains.add(domain)

    print("\n==========================")
    print(f"Processed lines : {total_lines:,}")
    print(f"Matched entries : {matched_lines:,}")
    print(f"Unique domains  : {len(domains):,}")
    print("==========================\n")

    if not domains:
        print("No domains extracted — exiting.")
        sys.exit(1)

    DATA_DIR.mkdir(parents=True, exist_ok=True)
    # Cleanup old files
    for f in DATA_DIR.glob("hosts_*"):
        f.unlink()

    file_count = 0
    buckets = defaultdict(list) 
    for d in domains: 
        buckets[len(d)].append(d) 
    
    for length, values in buckets.items(): 
        fname = Path(DATA_DIR / f"hosts_{length}") 
        with fname.open("w", encoding="utf-8") as f: 
            for v in sorted(values): 
                f.write(v + "\n") 
            f.write(",@@@\n")        
        file_count += 1

    print(f"✔ Generated {file_count} host files")
    print(f"✔ Output written to {DATA_DIR.resolve()}")
    
    build_bloom()

if __name__ == "__main__":
    main()
