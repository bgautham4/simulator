#! /bin/bash

#Arguments <trace> (imc|ws|dm|tf|sgd|km)

trace="$1"
cd "${0%/*}" || exit
cd "../results/$trace" || exit

for log_file in *; do
    variable_parameter=$(basename -s .txt "$log_file" | cut -d'_' -f2)
    util=$(grep '^##' "$log_file" | tail -n 1 | awk '{printf("%f", ($10 - $4)/$10)}')
    echo "$variable_parameter,$util" >> "util.csv"
    echo "Parsed $log_file.."
done

