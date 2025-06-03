#!/bin/bash

# accepts 2 args. the first is a path to a file, the second is a string
# exists if the arguments were not specified
# creates a new file with file name writefile and file content writestr


writefile=$1
writestr=$2

if [ $# -ne 2 ]
then
	echo "Error: not enough arguments"
	exit 1
else
	dirpath=$(dirname "$writefile")
	mkdir -p "$dirpath"
	echo "$writestr" > "$writefile"
	if [ $? -ne 0 ] 
	then 
	echo "Error: failed to write to file"
	exit 1
	fi
fi
exit 0
