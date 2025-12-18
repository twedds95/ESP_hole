import math

n = 400_000.0 #num domains
m = 12_500_000.0 #num bits
k = 7.0 #num hashes

p = (1.0 - math.exp(-k * n / m))**k

print(f"Estimated Num Domains: {n}")
print(f"BITS: {m}")
print(f"HASH: {k}")
print(f"False Positives %: {p * 100}")
print(f"False Positives Estimate: {p*n}")