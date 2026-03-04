EXEC        := tetris
SERVER_EXEC := tetris_server

ROOT    := ..
SRC_DIR := $(ROOT)/src

CLIENT_DIR := $(SRC_DIR)/client
SERVER_DIR := $(SRC_DIR)/server
COMMON_DIR := $(SRC_DIR)/common

BUILD_DIR      := .
OBJ_DIR_CLIENT := $(BUILD_DIR)/objClient
OBJ_DIR_SERVER := $(BUILD_DIR)/objServer

CXX := g++

CXXFLAGS := -std=c++23 -O2 -g
CXXFLAGS += -Wall -Wextra -Wpedantic -pedantic
CXXFLAGS += -MMD -MP
CXXFLAGS += -Wpedantic -Walloc-zero -Wcast-align -Wconversion -Wctad-maybe-unsupported \
            -Wctor-dtor-privacy -Wdeprecated-copy-dtor -Wduplicated-branches -Wduplicated-cond \
            -Wextra-semi -Wfloat-equal -Wformat-signedness -Wformat=2 -Winit-self -Wlogical-op \
            -Wmismatched-tags -Wnoexcept -Wnon-virtual-dtor -Wnull-dereference -Wold-style-cast \
            -Woverloaded-virtual -Wredundant-tags -Wshadow -Wsign-promo -Wstrict-null-sentinel \
            -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wsuggest-final-methods \
            -Wsuggest-final-types -Wsuggest-override -Wswitch-default -Wswitch-enum -Wundef \
            -Wuseless-cast -Wvolatile -Wzero-as-null-pointer-constant -fmax-errors=2
CXXFLAGS += -Wno-unused-command-line-argument -Wno-unknown-warning-option

SANITIZE := -fsanitize=address -fsanitize=leak -fsanitize=undefined

CPPINCLUDES := -I$(SRC_DIR)

LDLIBS_CLIENT := -lmenu -lncurses -lpqxx -lpq -pthread
LDLIBS_SERVER := -lpqxx -lpq -pthread

CLIENT_SRCS := $(shell find $(CLIENT_DIR) $(COMMON_DIR) -type f -name "*.cpp")
SERVER_SRCS := $(shell find $(SERVER_DIR) $(COMMON_DIR) -type f -name "*.cpp")

# Map to objects (keep directory structure)
CLIENT_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR_CLIENT)/%.o,$(CLIENT_SRCS))
SERVER_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR_SERVER)/%.o,$(SERVER_SRCS))

CLIENT_DEPS := $(CLIENT_OBJS:.o=.d)
SERVER_DEPS := $(SERVER_OBJS:.o=.d)

.PHONY: all client server sanitize clean reset

all: client server

client: $(BUILD_DIR)/$(EXEC)

server: $(BUILD_DIR)/$(SERVER_EXEC)

sanitize: $(BUILD_DIR)/$(EXEC)_sanitize $(BUILD_DIR)/$(SERVER_EXEC)_sanitize


$(BUILD_DIR)/$(EXEC): $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS_CLIENT)

$(BUILD_DIR)/$(SERVER_EXEC): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS_SERVER)

$(BUILD_DIR)/$(EXEC)_sanitize: CXXFLAGS += $(SANITIZE)
$(BUILD_DIR)/$(EXEC)_sanitize: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS_CLIENT)

$(BUILD_DIR)/$(SERVER_EXEC)_sanitize: CXXFLAGS += $(SANITIZE)
$(BUILD_DIR)/$(SERVER_EXEC)_sanitize: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS_SERVER)


$(OBJ_DIR_CLIENT)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPINCLUDES) -c -o $@ $<


$(OBJ_DIR_SERVER)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPINCLUDES) -c -o $@ $<


clean:
	rm -rf \
		$(BUILD_DIR)/$(EXEC) $(BUILD_DIR)/$(SERVER_EXEC) \
		$(BUILD_DIR)/$(EXEC)_sanitize $(BUILD_DIR)/$(SERVER_EXEC)_sanitize \
		$(OBJ_DIR_CLIENT) $(OBJ_DIR_SERVER)

reset: clean


-include $(CLIENT_DEPS) $(SERVER_DEPS)