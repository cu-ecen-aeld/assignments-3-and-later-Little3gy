#!/bin/sh

# a simple bash script

filesdir=$1
searchstr=$2

if [ $# -ne 2 ]
then
	echo "Error: $# arguments entered. Please enter a directory locaiton and a text string"
	exit 1

elif [ ! -d "$filesdir" ]
then
	echo "Error: $filesdir isn't a valid directory"
	exit 1


else

	X=$( find "$filesdir" -type f 2>/dev/null | wc -l )
	Y=$( grep -rI "$searchstr" "$filesdir" 2>/dev/null | wc -l )
	
	echo "The number of files are $X and the number of matching lines are $Y"
	exit 0

fi
