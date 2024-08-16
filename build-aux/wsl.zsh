#!/usr/bin/env zsh

if [[ $# -eq 0 ]]; then
    echo "Usage: $0 <on|off>"
    exit 1
fi

links=("run-clang-format" "run-cmake-format")

script_dir=$(dirname "$(realpath "$0")")

echo "Before:"
ls -la ${script_dir}

if [[ $1 == "on" ]]; then
    echo "Restoring unix links"
    for link in ${links[@]}; do
        rm "${script_dir}/$link"
        git restore "${script_dir}/$link"
    done
elif [[ $1 == "off" ]]; then
    echo "Removing unix links"
    for link in ${links[@]}; do
        rm -f "${script_dir}/$link"
    done
    echo "You will need to manually restore them from the Windows side"
fi

echo "After:"
ls -la ${script_dir}
