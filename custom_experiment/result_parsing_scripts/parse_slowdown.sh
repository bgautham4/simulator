#! /bin/bash

#Arguments <trace> (imc|ws|dm|tf|sgd|km)

trace="$1"
SCRIPT_DIR="${0%/*}"
cd "$SCRIPT_DIR" || exit
cd "../results/$trace" || exit

for log_file in *; do
    variable_parameter=$(basename -s .txt "$log_file" | cut -d'_' -f2)
    FNR=$(awk '/^##.*/ {print FNR}' "$log_file")
    head -n "$FNR" "$log_file" > /tmp/temp.txt
    slowdown=$(cd "$SCRIPT_DIR" && python3 parse_slowdown.py /tmp/temp.txt)
    echo "$slowdown" > "sdown_$variable_parameter.txt"
    echo "Parsed $log_file.."
done

