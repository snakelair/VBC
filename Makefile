CC ?= clang
CFLAGS ?= -O3 -std=c11 -Wall -Wno-unused-function
INCLUDES = -I runtime/include

SRC = bin/vbc-stage3.exe.c runtime/src/vbc_socket.c runtime/src/vbc_http.c
TARGET = bin/vbc

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows_NT)

ifeq ($(UNAME_S),Darwin)
    LDFLAGS = -lpthread
else ifeq ($(OS),Windows_NT)
    LDFLAGS = -lws2_32 -lkernel32
    TARGET = bin/vbc.exe
else
    LDFLAGS = -lpthread -lm
endif

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
