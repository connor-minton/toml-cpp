# Adapted from Job Vranish's Makefile
# (https://spin.atomicobject.com/2016/08/26/makefile-c-projects/)

LIB_TARGET := libtoml++.a
LIB_NAME := $(LIB_TARGET:lib%.a=%)

BUILD_DIR := ./build
SRC_DIRS := ./src

CXX := g++
CXXFLAGS := -std=c++17 -g

# Find all the C++ files we want to compile
# Note the single quotes around the * expressions. The shell will incorrectly
# expand these otherwise, but we want to send the * directly to the find
# command.
SRCS := $(shell find $(SRC_DIRS) -name '*.cpp')

# Prepends BUILD_DIR and appends .o to every src file
# As an example, ./your_dir/hello.cpp turns into ./build/./your_dir/hello.o
OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)

# String substitution (suffix version without %).
# As an example, ./build/hello.o turns into ./build/hello.d
DEPS := $(OBJS:.o=.d)

# Every folder in ./src will need to be passed to GCC so that it can find header
# files
INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_DIRS += include

# Add a prefix to INC_DIRS. So moduleA would become -ImoduleA. GCC understands
# this -I flag
INC_FLAGS = $(addprefix -I,$(INC_DIRS))

# The -MMD and -MP flags together generate Makefiles for us!
# These files will have .d instead of .o as the output. The .o files will also
# be generated.
CPPFLAGS = $(INC_FLAGS) -MMD -MP

TEST_TARGET := test-runner
TEST_SRCS := $(shell find test -name '*.cpp')
TEST_OBJS := $(TEST_SRCS:%.cpp=$(BUILD_DIR)/%.o)
TEST_DEPS := $(TEST_OBJS:.o=.d)
LDFLAGS := -L$(BUILD_DIR)
LDLIBS := -l$(LIB_NAME)

.PHONY: all
all: $(BUILD_DIR)/$(LIB_TARGET) $(BUILD_DIR)/$(TEST_TARGET)

# Build step for C++ source
$(BUILD_DIR)/test/%.o: INC_FLAGS += -Itest
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Build step for the library
$(BUILD_DIR)/$(LIB_TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	rm -f $@
	ar crf $@ $^

# Build step for the tests
$(BUILD_DIR)/$(TEST_TARGET): $(BUILD_DIR)/$(LIB_TARGET) $(TEST_OBJS) $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(TEST_OBJS) $(OBJS) $(LDLIBS) 

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)

.PHONY: test
test: $(BUILD_DIR)/$(TEST_TARGET)
	cd $(BUILD_DIR); ./$(TEST_TARGET)

# Include the .d makefiles. The - at the front suppresses the errors of missing
# Makefiles. Initially, all the .d files will be missing, and we don't want
# those errors to show up.
-include $(DEPS) $(TEST_DEPS)