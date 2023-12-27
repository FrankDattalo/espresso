SHELL := /bin/bash

build: prepare
	cd build && cmake -DCMAKE_BUILD_TYPE=Debug ..
	cd build && cmake --build .

debugger: prepare
	cd build && cmake -DESPRESSO_DEBUGGER=ON -DCMAKE_BUILD_TYPE=Debug ..
	cd build && cmake --build .

gc: prepare
	cd build && cmake -DCMAKE_BUILD_TYPE=Debug ..
	cd build && cmake --build .

release: prepare
	cd build && cmake -DCMAKE_BUILD_TYPE=Release ..
	cd build && cmake --build .

prepare:
	mkdir -p ./build/

clean:
	rm -rf ./build

output_tests: build asm
	diff <( ./build/espresso ./lib/helloworld.bc ) <( cat ./test/output/helloworld.txt )
	diff <( ./build/espresso ./lib/factorial.bc ) <( cat ./test/output/factorial.txt )
	diff <( ./build/espresso ./lib/try.bc ) <( cat ./test/output/try.txt )
	diff <( ./build/espresso ./lib/doublemath.bc ) <( cat ./test/output/doublemath.txt )
	diff <( ./build/espresso ./lib/recursiveprint.bc ) <( cat ./test/output/recursiveprint.txt )
	diff <( ./build/espresso ./lib/helloworld.espresso ) <( cat ./test/output/helloworld.txt )
	diff <( ./build/espresso ./lib/factorial.espresso ) <( cat ./test/output/factorial.txt )
	diff <( ./build/espresso ./lib/fibonacci.espresso ) <( cat ./test/output/fibonacci.txt )
	diff <( ./build/espresso ./lib/add3.espresso ) <( cat ./test/output/add3.txt )

test: build output_tests
#cd build && CTEST_OUTPUT_ON_FAILURE=TRUE make test

run:
	./build/espresso

asm:
	python3 ./asm/assembler.py < ./lib/helloworld.easm > ./lib/helloworld.bc
	python3 ./asm/assembler.py < ./lib/factorial.easm > ./lib/factorial.bc
	python3 ./asm/assembler.py < ./lib/invalid.easm > ./lib/invalid.bc
	python3 ./asm/assembler.py < ./lib/try.easm > ./lib/try.bc
	python3 ./asm/assembler.py < ./lib/doublemath.easm > ./lib/doublemath.bc
	python3 ./asm/assembler.py < ./lib/bench1.easm > ./lib/bench1.bc
	python3 ./asm/assembler.py < ./lib/recursiveprint.easm > ./lib/recursiveprint.bc
	python3 ./asm/assembler.py < ./lib/gcbench.easm > ./lib/gcbench.bc

stats:
	cat ./src/* | wc

.PHONY: test clean prepare build flex stats asm
