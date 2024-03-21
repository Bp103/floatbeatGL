# Makefile for floatbeatGL 

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11

# Libraries (common for both Linux and Windows)
COMMON_LIBS = -lfreeglut -lportaudio
# Additional libraries for Windows
WINDOWS_LIBS = -lglu32 -lopengl32
# Executable name
EXEC = floatbeatGL

# Determine the operating system
ifeq ($(OS),Windows_NT)
    # If running on Windows
    EXEC := $(EXEC).exe
    LIBS := $(COMMON_LIBS) $(WINDOWS_LIBS)
else
    # If running on Linux or any other Unix-like system
    LIBS := $(COMMON_LIBS)
endif

# Source file
SRC = floatbeatGL.c

all: $(EXEC)

$(EXEC): $(SRC)
    $(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
    rm -f $(EXEC)
