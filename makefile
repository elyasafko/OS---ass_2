CC = g++
CFLAGS = -Wall -Wextra -std=c++11
TARGET = mync
SRCS = mync.cpp ttt.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET) ttt

$(TARGET): mync.o ttt.o
	$(CC) $(CFLAGS) -o $(TARGET) mync.o

ttt: ttt.o
	$(CC) $(CFLAGS) -o ttt ttt.o

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) ttt
