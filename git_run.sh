#! /usr/bin/bash

while getopts f:c: flag
do
    case "${flag}" in
        f) file_path=${OPTARG};;
        c) comment=${OPTARG};;
    esac
done

# echo "file_path: ${file_path}";
# echo "comment: ${comment}";

# echo "git add ${file_path} && git commit -m '${comment}'"

git add ${file_path} && git commit -m "${comment}"