#!/bin/bash

if [[ $# -eq 0 ]] ; then
    echo "Usage: $0 <projectName>"
    exit 1
fi

projectName="$1"

mkdir -p "$1"

for file in template/*; do
    out="${file//template/$projectName}"
    if [ ! -f "$out" ]; then
        sed -e "s/template/$projectName/g" -e "s/Template/${projectName^}/g" "$file" > "$out"
        echo "$out created"
    fi
done

echo "Project $projectName Initialized!"
exit 0