Debug = n

CC = gcc

LDFLAGS = -L/usr/local/lib/SDL2 -lSDL2 -lSDL2_image -pthread 
CFLAGS = -Wall
INC_DIR = $(shell pkg-config --cflags sdl2)

TARGET = camviewer

SRCS = main.c sdl_monitor.c v4l2_process.c common.c

ifeq ($(Debug), y)
CFLAGS += -g -DDEBUG
endif

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $^ $(CFLAGS) $(INC_DIR) $(LDFLAGS) -o $@ 

%.o: %.cpp
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY = all clean
clean:
	rm -f *.o
	find -type f -executable -exec rm {} \;
