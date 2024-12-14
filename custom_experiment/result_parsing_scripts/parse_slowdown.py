from sys import argv
import numpy as np

MSS = 1460  # In bytes
BDP = 49 * MSS  # ~71.5KB

file_path = argv[1]

slowdown_histogram = {1: [], 2: [], 4: [], 8: [], 16: [], 32: []}

with open(file_path, "r") as file:
    for line in file:
        flow_data = line.split(" ")
        try:
            int(flow_data[0])  # Try parsing the first value as int
        except ValueError:
            continue
        flow_size = float(flow_data[1])
        slowdown = float(flow_data[8])  # In us
        flow_size_normed = flow_size // BDP  # In BDPs

        k = 1
        while (flow_size_normed != 0 and k < 32):
            k *= 2
            flow_size_normed //= 2

        while (k <= 32):
            slowdown_histogram[k].append(slowdown)
            k *= 2


slowdown_data = dict()
for bdp_bin, slowdown_list in slowdown_histogram.items():
    slowdown_list.sort()
    percentile_99 = np.percentile(slowdown_list, 99)
    mean = np.mean(slowdown_list)
    slowdown_data[bdp_bin] = (mean, percentile_99)

print('bin', 'mean', '99')
for bin in sorted(slowdown_data.keys()):
    print(bin, slowdown_data[bin][0], slowdown_data[bin][1])
