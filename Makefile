build: prepare
	cd build && cmake -DCMAKE_BUILD_TYPE=Debug ..
	cd build && cmake --build .

release: prepare
	cd build && cmake -DCMAKE_BUILD_TYPE=Release ..
	cd build && cmake --build .

prepare:
	mkdir -p ./build/

clean:
	rm -rf ./build

test: build
	cd build && CTEST_OUTPUT_ON_FAILURE=TRUE make test

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

.PHONY: test clean prepare build flex sourceStats asm
