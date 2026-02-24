# Nom de l'exécutable
EXEC = tetris
SERVER_EXEC = tetris_server

# Répertoire des fichiers source
SRC_DIR = ../src
TEST_DIR = ../test
CLIENT_DIR = $(SRC_DIR)/client
SERVER_DIR = $(SRC_DIR)/server

# Répertoire pour l'exécutable
BUILD_DIR = .
OBJ_DIR_CLIENT = $(BUILD_DIR)/objClient
OBJ_DIR_SERVER = $(BUILD_DIR)/objServer

# Fichier source
SRC_FILES = $(wildcard $(CLIENT_DIR)/*.cpp)
SRC_FILES_NO_MAIN := $(filter-out $(CLIENT_DIR)/main.cpp, $(wildcard $(CLIENT_DIR)/*.cpp))
SERVER_SRC_FILES = $(wildcard $(SERVER_DIR)/*.cpp)
TEST_FILES = $(wildcard $(TEST_DIR)/*.cpp)

OBJFILES := $(patsubst $(CLIENT_DIR)/%.cpp, $(OBJ_DIR_CLIENT)/%.o, $(SRC_FILES))
SERVER_OBJFILES := $(patsubst $(SERVER_DIR)/%.cpp, $(OBJ_DIR_SERVER)/%.o, $(SERVER_SRC_FILES))
OBJTEST := $(patsubst $(CLIENT_DIR)/%.cpp, $(OBJ_DIR_CLIENT)/%.o, $(SRC_FILES_NO_MAIN))
OBJTEST += $(patsubst $(TEST_DIR)/%.cpp, $(OBJ_DIR_CLIENT)/%.o, $(TEST_FILES))

# Options du compilateur
CXXFLAGS += -std=c++23
CXXFLAGS += -pedantic -Wall -Wextra -g
CXXFLAGS += -O2
CXXFLAGS += -Wpedantic -Walloc-zero -Wcast-align -Wconversion -Wctad-maybe-unsupported -Wctor-dtor-privacy -Wdeprecated-copy-dtor -Wduplicated-branches -Wduplicated-cond -Wextra-semi -Wfloat-equal -Wformat-signedness -Wformat=2 -Winit-self -Wlogical-op -Wmismatched-tags -Wnoexcept -Wnon-virtual-dtor -Wnull-dereference -Wold-style-cast -Woverloaded-virtual -Wredundant-tags -Wshadow -Wsign-promo -Wstrict-null-sentinel -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wsuggest-final-methods -Wsuggest-final-types -Wsuggest-override -Wswitch-default -Wswitch-enum -Wundef -Wuseless-cast -Wvolatile -Wzero-as-null-pointer-constant -fmax-errors=2 -MMD
CXXFLAGS += -Wno-unused-command-line-argument -Wno-unknown-warning-option -lpqxx -lpq

SANITIZE = -fsanitize=address -fsanitize=leak -fsanitize=undefined
TESTFLAGS = -I/usr/include/cppunit -L/usr/lib -lcppunit -ldl -pthread -lncurses

all: $(BUILD_DIR)/$(EXEC) $(BUILD_DIR)/$(SERVER_EXEC)
sanitize: $(BUILD_DIR)/$(EXEC)_sanitize
test: $(BUILD_DIR)/$(EXEC)_test
client: $(BUILD_DIR)/$(EXEC)

# Compilation
$(BUILD_DIR)/$(EXEC): $(OBJFILES)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lmenu -lncurses

$(BUILD_DIR)/$(SERVER_EXEC): $(SERVER_OBJFILES)
	$(CXX) $(CXXFLAGS) $(SANITIZE) -o $@ $^ -lsqlite3 -pthread -lncurses  # Added -lsqlite3 and -pthread

# Compilation avec les sanitizers
$(BUILD_DIR)/$(EXEC)_sanitize: $(OBJFILES)
	$(CXX) $(CXXFLAGS) $(SANITIZE) -o $@ $^ -lncurses

# Compilation des tests
$(BUILD_DIR)/$(EXEC)_test: $(OBJTEST)
	$(CXX) $(CXXFLAGS) $(SANITIZE) -o $@ $^ $(TESTFLAGS)

# Compilation des fichiers sources en objets dans le dossier obj/
$(OBJ_DIR_CLIENT)/%.o: $(CLIENT_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR_CLIENT)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJ_DIR_SERVER)/%.o: $(SERVER_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR_SERVER)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Compilation des fichiers tests en objets dans le dossier obj/
$(OBJ_DIR_CLIENT)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR_CLIENT)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Nettoyage des fichiers générés
clean:
	rm -f $(BUILD_DIR)/$(EXEC) $(BUILD_DIR)/$(EXEC)_sanitize $(OBJ_DIR_CLIENT)/* $(OBJ_DIR_SERVER)/*

reset: clean

# Inclusion des fichiers de dépendances générés
-include $(OBJFILES:.o=.d) $(SERVER_OBJFILES:.o=.d) $(OBJTEST:.o=.d)
