import base64
import sys
import hashlib
import binascii

mat1 = "2565427"
mat2 = "2572741"
# Stored nonce
nonce = "xmZ5adIxEJCBFp/GBywx6Q=="
# Stored hash. We decode it to be able to compare directly to our generated hash
correct_hash = base64.b64decode("lcQ3AGW++w7zuiOtC2t2JpCs6XIcsy6+28PUIf/zxo8=")

# In order to optimize the brute-forcing performance, we parallelize the script
# execution into a number of processes, where each process goes through a partition
# of the whole 32-bit range. The number of partitions (and thus processes) is
# passed to the script as a command-line argument, alongside this instance's index
# in the partition. We determine the number of partitions argument according to
# the number of available processor cores on the machine, so that each core runs
# one process, achieving maximum CPU utilization
partition = int(sys.argv[1])
index = int(sys.argv[2])

step = 2 ** 32 // partition  # the number of sk values each instance is going to go through
start = index * step  # start sk value
end = (index + 1) * step  # end sk value

print(f"Starting hash search from {start} to {end}")
for secret in range(start, end):
    # Show progress to get a feeling for the feasibility of this method
    print(f"Progress: {(secret - start) * 100 // (end - start)}%", end='\r')
    # Construct the hash input
    data = bytes(mat1 + mat2 + nonce, 'utf-8') + secret.to_bytes(4, byteorder='big')
    # Calculate the hash
    h = hashlib.sha256(data).digest()
    if h == correct_hash:  # We found the secret
        print("\nSuccess!")
        print("SHA256: " + str(binascii.hexlify(h)))
        print("Secret: " + str(secret))
        break
