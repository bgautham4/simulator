#! /bin/bash

#Arguments <exp_type> <var_param_2>
exp_type=$1
var_param_2=$2

result_dir="../results/flow_type_$exp_type"

if [[ ! -f "$result_dir/util.csv" ]]; then
    > "$result_dir/util.csv"
fi


    for log_file in "$result_dir/flow_details"*; do
        variable_parameter=$(basename -s .txt "$log_file" | cut -d'_' -f3)
        util=$(grep '^##' "$log_file" | tail -n 1 | awk 'BEGIN{x = 0; y = 0; z = 0;}{x = $4 ; y = $10 ; z = y - x;}END{print z/y}')

        if [[ -z "$var_param_2" ]]; then
            echo "$variable_parameter,$util" >> "$result_dir/util.csv"
        else
            echo "$var_param_2,$variable_parameter,$util" >> "$result_dir/util.csv"
        fi
        echo "Parsed $log_file.."
    done

