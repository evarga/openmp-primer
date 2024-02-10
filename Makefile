# Compiler to use
CC = g++

# Compiler flags
CFLAGS = -Wall -std=c++14

# Directories
SRC_DIR = src
BUILD_DIR = build

# Name of the output file
OUTFILE = $(BUILD_DIR)/solver

# Source files
SRC = $(SRC_DIR)/solver.cpp

all: $(OUTFILE)

$(OUTFILE): $(SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(OUTFILE) $(SRC)

clean:
	rm -rf $(BUILD_DIR)