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

.PHONY: test clean prepare build flex sourceStats
