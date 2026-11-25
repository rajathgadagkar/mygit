# MyGit — Content-Addressable Version Control System in C++17
# Author: Rajath Gadagkar <rajathgadagkar@gmail.com>

CXX       ?= g++
CXXFLAGS  := -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS   := -lz

BUILD_DIR := build
TARGET    := mygit
UNIT_BIN  := $(BUILD_DIR)/unit_tests

LIB_SRCS  := \
	src/sha1.cpp \
	src/compression.cpp \
	src/fs_utils.cpp \
	src/object_store.cpp \
	src/index.cpp \
	src/diff.cpp \
	src/ui.cpp \
	src/repository.cpp

MAIN_SRC  := src/main.cpp
TEST_SRC  := tests/unit_tests.cpp

LIB_OBJS  := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(LIB_SRCS))
MAIN_OBJ  := $(BUILD_DIR)/main.o
TEST_OBJ  := $(BUILD_DIR)/unit_tests.o

.PHONY: all test unit-test e2e-test clean install

all: $(TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_OBJ): $(TEST_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(LIB_OBJS) $(MAIN_OBJ)
	$(CXX) $(LIB_OBJS) $(MAIN_OBJ) -o $@ $(LDFLAGS)

$(UNIT_BIN): $(LIB_OBJS) $(TEST_OBJ)
	$(CXX) $(LIB_OBJS) $(TEST_OBJ) -o $@ $(LDFLAGS)

unit-test: $(UNIT_BIN)
	./$(UNIT_BIN)

e2e-test: $(TARGET)
	bash tests/e2e_tests.sh

test: unit-test e2e-test

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
