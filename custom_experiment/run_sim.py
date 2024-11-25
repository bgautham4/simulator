from sys import argv
import subprocess
from multiprocessing.pool import ThreadPool
import os

conf_dir = argv[1]
output_dir = argv[2]


def run_sim(conf_file):
    load = os.path.splitext(
        os.path.basename(conf_file))[0].split("_")[-1]
    res_file = 'res_' + load + '.txt'
    with open(f'{os.path.join(os.path.curdir, output_dir, res_file)}', "w") as f:
        print(f'Started sim for {conf_file}')
        ret_obj = subprocess.run(
            ['../simulator', '1', f'{conf_file}'], stdout=f)
    return (conf_file, ret_obj.returncode)


conf_files = [os.path.join(conf_dir, f) for f in os.listdir(
    conf_dir) if os.path.isfile(os.path.join(conf_dir, f))]

with ThreadPool(8) as pool:
    outputs = pool.map(run_sim, conf_files)
    for op in outputs:
        print(f'Sim for {op[0]} returned {op[1]}')
