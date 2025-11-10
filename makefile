CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99
LDFLAGS = 

# Targets
all: server client

server: server.o rtp_utils.o
	$(CC) $(CFLAGS) -o server server.o rtp_utils.o $(LDFLAGS)

client: client.o rtp_utils.o stats.o
	$(CC) $(CFLAGS) -o client client.o rtp_utils.o stats.o $(LDFLAGS)

server.o: server.c rtp.h
	$(CC) $(CFLAGS) -c server.c

client.o: client.c rtp.h
	$(CC) $(CFLAGS) -c client.c

rtp_utils.o: rtp_utils.c rtp.h
	$(CC) $(CFLAGS) -c rtp_utils.c

stats.o: stats.c stats.h
	$(CC) $(CFLAGS) -c stats.c

clean:
	rm -f *.o server client received_frame_*.jpg

test: all
	@echo "Build successful! Run the following to test:"
	@echo "Terminal 1: ./client 5004"
	@echo "Terminal 2: ./server 127.0.0.1 5004 test_image.jpg"

.PHONY: all clean test