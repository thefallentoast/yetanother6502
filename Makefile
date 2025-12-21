CC   ?= gcc
TASS := 64tass

ROOT_PATH    := .

INCLUDE_PATH := $(ROOT_PATH)/include
SOURCE_PATH  := $(ROOT_PATH)/src
OBJ_PATH     := $(ROOT_PATH)/obj
BIN_PATH     := $(ROOT_PATH)/bin
ASM_PATH     := $(ROOT_PATH)/asm

ifneq (,$(findstring mingw,$(CC)))
    EXE := .exe
else
    EXE :=
endif

SOURCES    := $(wildcard $(SOURCE_PATH)/*.c)
OBJECTS    := $(patsubst $(SOURCE_PATH)/%.c,$(OBJ_PATH)/%.o,$(SOURCES))
ASSEMBLIES := $(wildcard $(ASM_PATH)/*.asm)
TARGET     := $(BIN_PATH)/ya6502$(EXE)

CFLAGS   := -Wall -Wextra -O3 -I$(INCLUDE_PATH) -std=c2x
ASMFLAGS := --flat -Wall --mw65c02

run: $(TARGET) assemble
ifeq ($(EXE),)
	$(TARGET) sample.bin
else
	@echo "Built Windows executable ($(TARGET)); skipping run"
endif

assemble: $(ASSEMBLIES)
	$(TASS) $^ -o $(ROOT_PATH)/sample.bin $(ASMFLAGS)

$(OBJ_PATH)/%.o: $(SOURCE_PATH)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(OBJECTS)
	rm -rf $(TARGET)
	rm -rf $(ROOT_PATH)/sample.bin