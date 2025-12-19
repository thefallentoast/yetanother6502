CC := gcc

INCLUDE_PATH := ./include
SOURCE_PATH  := ./src
OBJ_PATH     := ./obj
BIN_PATH     := ./bin

SOURCES := $(wildcard $(SOURCE_PATH)/*.c)
OBJECTS := $(patsubst $(SOURCE_PATH)/%.c,$(OBJ_PATH)/%.o,$(SOURCES))
TARGET  := $(BIN_PATH)/ya6502

CFLAGS := -Wall -Wextra -O3 -I$(INCLUDE_PATH)

run: $(TARGET)
	$(TARGET) sample.bin

$(OBJ_PATH)/%.o: $(SOURCE_PATH)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@