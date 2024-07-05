#!/bin/bash

# result_folder="result/dialog"
# result_folder="result/all_to_one"
result_folder="result/round_shift_left"
result_file="round_shift_left_res.csv"

noc_dim_list=(1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32)

echo "config,cycle,start,end" > ${result_folder}/${result_file}

for dim in ${noc_dim_list[@]}; do
	file="NoC_${dim}x${dim}.log"
	config=${file%%.*}
	file_path=${result_folder}/${file}
	start=$(sed -n '$!{h;1!H};${g;s/.*\n//;p}' "$file_path" | awk 'END{print $2}' | sed 's/[^0-9]*//g')
	end=$(awk 'END{print $2}' "$file_path" | sed 's/[^0-9]*//g')
	cycle=$((end - start))
	echo "${config},${cycle},${start},${end}" >> ${result_folder}/${result_file}
done

