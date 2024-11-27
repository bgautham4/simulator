#!/bin/bash

cd "${0%/*}" || exit #CWD to parent dir of script

function disp_help {
    echo "Usage: --trace imc|wc|dm|tf|km|sgd [--loads start:step:end]"
}

TEMP=$(getopt -o '-h' -l 'trace:,loads:' -- "$@")
if [[ $? -ne 0 ]]; then
    echo 'Invalid use, Terminating...' >&2
    echo 'Use -h to display help text.'
    exit 1
fi
eval set -- "$TEMP"
unset TEMP

while true; do
    case "$1" in
        '-h')
            disp_help
            exit 0
            ;;
        '--trace')
            trace="$2" 
            case "$2" in
                'imc')
                    trace_file='trace_files/CDF_imc10.txt'
                    ;;
                'ws')
                    trace_file='trace_files/CDF_websearch.txt'
                    ;;
                'dm')
                    trace_file='trace_files/CDF_datamining.txt'
                    ;;
                'tf')
                    trace_file='trace_files/CDF_tensorflow.txt'
                    ;;
                'km')
                    trace_file='trace_files/CDF_kmeans.txt'
                    ;;
                'sgd')
                    trace_file='trace_files/CDF_sgd.txt'
                    ;;
                *)
                    echo "Invalid trace, use -h to see available traces"
                    exit 1
                    ;;
            esac
            shift 2
            ;;
        '--loads')
            IFS=':' read -ra TEMP <<< "$2"
            start="${TEMP[0]}"
            step="${TEMP[1]}"
            stop="${TEMP[2]}"
            unset TEMP
            shift 2
            ;;
        '--')
            shift
            break
            ;;
        *)
            echo "$1"
            echo "Invalid arguments. Use -h to display useage."
            exit 1
            ;;
    esac
done

if [[ ! -f 'base_conf.txt' ]]; then
    echo 'Missing base_conf.txt file!'
    echo 'Did you accidentally delete it?'
    echo 'Find it here: https://github.com/bgautham4/simulator'
    exit 1
fi

if [[ -z "$start" ]]; then
    start='0.5'
    step='0.1'
    stop='0.5'
fi

conf_dir="/tmp/$(tr -dc A-Za-z0-9 < /dev/urandom | head -c 13)"
mkdir "$conf_dir"
base_conf="$conf_dir/base_conf.txt"

sed 's/#.*//g' ./base_conf.txt > "$base_conf"
sed -i '/^\s*$/d' "$base_conf"
sed -i "s|\(^flow_trace:\).*|\1 $trace_file|" "$base_conf" #Use | as file path (trace) contains / which interferes with sed's subsitution rule

for i in $(seq -f'%.2f' "$start" "$step" "$stop"); do
    sed "s/\(^load:\).*/\1 $i/" "$base_conf" > "$conf_dir/conf_file_$i.txt"
done

rm "$base_conf"

if [[ -d ./results/$trace ]]; then
    rm -rf "./results/$trace"
fi

mkdir -p "./results/$trace"

python3 run_sim.py "$conf_dir" "./results/$trace" 
RET="$?"
if [[ "$RET" -ne 0 ]];then
    echo "run_sim.py exited in error!"
fi
rm -rf "$conf_dir"
exit "$RET"
