OPT_FLAGS = -O3 -msse3 -ipo -no-prec-div -fp-model fast=2 -funroll-loops -fomit-frame-pointer -fno-stack-protector
COMPILER = /opt/intel/bin/icpc
NAME = repatcher_amxx_i386.so

OBJECTS = amxxmodule.cpp cfunction.cpp chldsprocess.cpp chookmanager.cpp jit.cpp cmodule.cpp config.cpp sdk/interface.cpp meta_api.cpp mod_rehlds_api.cpp natives.cpp repatcher.cpp utils.cpp libudis86.a

LINK = -lm -ldl -static-intel -static-libgcc -no-intel-extensions

INCLUDE = -I. -I./sdk

BIN_DIR = Release
CFLAGS = $(OPT_FLAGS)

CFLAGS += -g0 -fvisibility-inlines-hidden -DNDEBUG -std=c++11 -shared -wd147,274 -fasm-blocks

OBJ_LINUX := $(OBJECTS:%.c=$(BIN_DIR)/%.o)

$(BIN_DIR)/%.o: %.c
	$(COMPILER) $(INCLUDE) $(CFLAGS) -o $@ -c $<

all:
	mkdir -p $(BIN_DIR)
	mkdir -p $(BIN_DIR)/sdk

	$(MAKE) repatcher_amxx_i386

repatcher_amxx_i386: $(OBJ_LINUX)
	$(COMPILER) $(INCLUDE) $(CFLAGS) -c precompiled.h -o$(BIN_DIR)/precompiled.h.gch
	$(COMPILER) $(INCLUDE) $(CFLAGS) $(OBJ_LINUX) $(LINK) -o$(BIN_DIR)/$(NAME)
	strip --strip-unneeded $(BIN_DIR)/$(NAME)

debug:	
	$(MAKE) all DEBUG=false

default: all

clean:
	rm -rf Release/sdk/*.o
	rm -rf Release/*.o
	rm -rf Release/$(NAME)
	rm -rf Debug/sdk/*.o
	rm -rf Debug/*.o
	rm -rf Debug/$(NAME)
