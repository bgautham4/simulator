#! /bin/bash

exp=$1
conf_dir=conf_files/flow_type_"$exp"_loads
output_dir=results/flow_type_"$exp"_loads

if [[ ! -e "$output_dir" ]]; then
    mkdir -p "$output_dir"
fi

python3 run_loads.py "$conf_dir" "$output_dir" 

