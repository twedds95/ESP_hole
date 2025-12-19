import math
import argparse

parser = argparse.ArgumentParser(description="Bloom filter false positive estimator")

parser.add_argument("-n", "--domains", type=float, default=300_000.0,
                    help="Number of domains (default: 300000)")
parser.add_argument("-m", "--bits", type=float, default=10_000_000.0,
                    help="Number of bits (default: 10000000)")
parser.add_argument("-k", "--hashes", type=float, default=6.0,
                    help="Number of hash functions (default: 6)")

args = parser.parse_args()

n = args.domains
m = args.bits
k = args.hashes

p = (1.0 - math.exp(-k * n / m)) ** k

print(f"Estimated Num Domains: {n:,.0f}")
print(f"BITS: {m:,.0f}")
print(f"HASH: {k:.0f}")
print(f"False Positives %: {(p * 100):.4f}")
print(f"On a random set of {int(n):,} domains, ~{(p * n):.1f} domains would get blocked that should not be.")
