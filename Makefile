# p-net IO Device Manager
# Makefile wrapper for CMake

BUILD_DIR = build

.PHONY: all test clean mkdir

all: mkdir
	@cd $(BUILD_DIR) && cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug && make -j$$(sysctl -n hw.ncpu 2>/dev/null || nproc)

mkdir:
	@mkdir -p $(BUILD_DIR)

test: all
	@cd $(BUILD_DIR) && ./pnet-tests

clean:
	rm -rf $(BUILD_DIR)

install: all
	@cd $(BUILD_DIR) && cmake --install . --prefix /usr/local

run: all
	@cd $(BUILD_DIR) && ./pnet-manager -v
