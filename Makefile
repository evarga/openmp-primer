# Compiler to use
CC = g++

# Standard compiler flags with code optimization turned on
CFLAGS = -Wall -std=c++14 -O3

# OpenMP library activation on macOS (clang compiler)
OPENMP_LIB = -Xpreprocessor -fopenmp -lomp

# Directories
SRC_DIR = src
BUILD_DIR = build

# Name of the output file
OUTFILE = $(BUILD_DIR)/solver

# Source files
SRC = $(SRC_DIR)/solver.cpp

# Blank value for engine mode means default (regular) regime.
ENGINE_MODE =

all:
	mkdir -p $(BUILD_DIR)

build-sequential: all
	$(CC) $(CFLAGS) -o $(OUTFILE) $(SRC)

build-parallel: all
	$(CC) $(CFLAGS) $(OPENMP_LIB) -o $(OUTFILE) $(SRC)

# The number of threads used inside an OpenMP parallel region is only relevant
# when the program was previously built for parallel execution.
run:
	@OMP_DYNAMIC=true OMP_NUM_THREADS=8 ./$(OUTFILE) $(ENGINE_MODE)	

run-fast: 
	@$(MAKE) run ENGINE_MODE=--fast

clean:
	rm -rf $(BUILD_DIR)