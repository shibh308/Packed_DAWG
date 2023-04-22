#!/bin/bash

exec_file="cmake-build-release/Packed_DAWG"
files=("english" "dna" "sources")
methods=("HeavyTree" "HeavyTreeBP" "HeavyPath" "Simple")
# lengthes=(10 20 50 100 200 500 1000 2000 5000 10000 20000 50000 100000 200000 1000000 2000000 5000000 10000000 10485760)
lengthes=(10485760)

rm data/output_memory.txt

for file in "${files[@]}"
do
	for method in "${methods[@]}"
	do
		for length in "${lengthes[@]}"
		do
			$exec_file $file $method $length
			echo
		done
	done
done

