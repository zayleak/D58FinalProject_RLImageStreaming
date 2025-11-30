CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99
LDFLAGS = 

# Targets
all: server client

server: server.o rtp_utils.o time_utils.o
	$(CC) $(CFLAGS) -o server server.o rtp_utils.o time_utils.o  $(LDFLAGS)

client: client.o rtp_utils.o stats.o jitter_buffer.o reorder_buffer.o time_utils.o
	$(CC) $(CFLAGS) -o client client.o rtp_utils.o stats.o jitter_buffer.o reorder_buffer.o time_utils.o $(LDFLAGS)

jitter_buffer.o: jitter_buffer.c jitter_buffer.h 
	$(CC) $(CFLAGS) -c jitter_buffer.c

reorder_buffer.o: reorder_buffer.c reorder_buffer.h 
	$(CC) $(CFLAGS) -c reorder_buffer.c

server.o: server.c rtp.h
	$(CC) $(CFLAGS) -c server.c

client.o: client.c rtp.h
	$(CC) $(CFLAGS) -c client.c

rtp_utils.o: rtp_utils.c rtp.h
	$(CC) $(CFLAGS) -c rtp_utils.c

stats.o: stats.c stats.h
	$(CC) $(CFLAGS) -c stats.c

time_utils.o: time_utils.c time_utils.h
	$(CC) $(CFLAGS) -c time_utils.c

clean:
	rm -f *.o server client frames/received_frame_*.jpg

test: all
	@echo "Build successful! Run the following to test:"
	@echo "Terminal 1: ./client 5004"
	@echo "Terminal 2: ./server 127.0.0.1 5004 test_image.jpg"

.PHONY: all clean test