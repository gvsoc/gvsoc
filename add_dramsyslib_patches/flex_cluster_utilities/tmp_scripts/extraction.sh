
get_uti_string() {
    local input_file="$1"
    local string=""
    local -a array

    string=$(grep "Finished" ${input_file})
    read -ra array <<< "$string"
    echo "${array[20]}"
}

get_area_string() {
    local input_file="$1"
    local string=""
    local -a array

    string=$(grep "Area" ${input_file})
    read -ra array <<< "$string"
    echo "${array[7]}"
}

get_efficiency_string() {
    local input_file="$1"
    local string=""
    local -a array

    string=$(grep "Efficiency" ${input_file})
    read -ra array <<< "$string"
    echo "${array[11]}"
}

src_folder="$1"
csv_file="$2"
echo "area,eff,uti" > ${csv_file}
for file in $(ls ${src_folder}/*); do
    area=$(get_area_string ${file})
    uti=$(get_uti_string ${file})
    eff=$(get_efficiency_string ${file})
    echo "${area},${eff},${uti}" >> ${csv_file}
done

