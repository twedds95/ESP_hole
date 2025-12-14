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
    # "https://adguardteam.github.io/HostlistsRegistry/assets/filter_1.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_2.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_4.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_7.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_11.txt",
    # "https://adguardteam.github.io/HostlistsRegistry/assets/filter_18.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_30.txt",
    # "https://adguardteam.github.io/HostlistsRegistry/assets/filter_48.txt",
    "https://adguardteam.github.io/HostlistsRegistry/assets/filter_55.txt",
]

DATA_DIR = Path("./data")
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

    SPLIT_THRESHOLD = 1_000

    # ==========================
    # First pass: bucket by length
    # ==========================
    by_length = defaultdict(list)

    for d in domains:
        by_length[len(d)].append(d)

    DATA_DIR.mkdir(parents=True, exist_ok=True)

    # Cleanup old files
    for f in DATA_DIR.glob("hosts_*"):
        f.unlink()

    file_count = 0

    # ==========================
    # Second pass: write files
    # ==========================
    for length, values in by_length.items():

        # Decide if we split
        if len(values) > SPLIT_THRESHOLD:
            # Split by first letter
            by_letter = defaultdict(list)
            for d in values:
                by_letter[d[0]].append(d)

            for letter, items in by_letter.items():
                fname = DATA_DIR / f"hosts_{length}_{letter}"
                with fname.open("w", encoding="ascii") as f:
                    for v in items:
                        f.write(v + "\n")
                    f.write(",@@@\n")
                file_count += 1

        else:
            # Single file
            fname = DATA_DIR / f"hosts_{length}"
            with fname.open("w", encoding="ascii") as f:
                for v in values:
                    f.write(v + "\n")
                f.write(",@@@\n")
            file_count += 1

    print(f"✔ Generated {file_count} host files")
    print(f"✔ Output written to {DATA_DIR.resolve()}")


if __name__ == "__main__":
    main()
