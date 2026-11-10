# PC test build of CasioDoom (head-less, see i_host.c)
CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wno-unused-but-set-variable -Wno-parentheses \
           -Werror=int-conversion -Werror=incompatible-pointer-types \
           -Werror=implicit-function-declaration -fno-strict-aliasing
SRC     := $(wildcard ../*.c) i_host.c
OBJ     := $(patsubst %.c,build/%.o,$(notdir $(SRC)))
VPATH   := ..:.

doomhost: $(OBJ)
	$(CC) -o $@ $(OBJ) -lm

build/%.o: %.c | build
	$(CC) $(CFLAGS) -MMD -c $< -o $@

build:
	mkdir -p build

-include $(OBJ:.o=.d)

clean:
	rm -rf build doomhost
