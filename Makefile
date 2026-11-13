all:
	mkdir -p build && cd build && cmake ../ -DCMAKE_BUILD_TYPE=Debug
	cd build && cmake --build . --parallel
