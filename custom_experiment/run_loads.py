import sys
import subprocess
from multiprocessing.pool import ThreadPool

conf_dir = sys.argv[1]
output_dir = sys.argv[2]


def run_loads(load):
    with open(f'{output_dir}/flow_details_{load:.2f}.txt', "w") as f:
        print(f'Starting exp for load : {load}')
        subprocess.run(
            ['../simulator', '1', f'{conf_dir}/conf_file_{load:.2f}.txt'], stdout=f)
    return


start = 0.50
step = 0.05
stop = 0.85

loads = []
while (start <= stop):
    loads.append(start)
    start += step

with ThreadPool(8) as pool:
    pool.map(run_loads, loads)
