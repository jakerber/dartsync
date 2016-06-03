# c-men
# Makefile for Dartsync

CC=gcc
CFLAGS=-ggdb -Wall -pedantic -pthread
DEPS=common/constants.h


all: peer/DartSync_peer tracker/DartSync_tracker

peer/DartSync_peer: peer/peer.o common/filetable.o common/seg.o common/fileMonitor.o
	$(CC) $(CFLAGS) peer/peer.o common/filetable.o common/seg.o common/fileMonitor.o -o peer/DartSync_peer

tracker/DartSync_tracker: tracker/tracker.o common/seg.o common/filetable.o
	$(CC) $(CFLAGS) tracker/tracker.o common/seg.o common/filetable.o -o tracker/DartSync_tracker

common/seg.o: common/seg.c common/seg.h
	$(CC) $(CFLAGS) -c common/seg.c -o common/seg.o
common/filetable.o: common/filetable.c common/filetable.h
	$(CC) $(CFLAGS) -c common/filetable.c -o common/filetable.o
common/fileMonitor.o: common/fileMonitor.c common/fileMonitor.h
	$(CC) $(CFLAGS) -c common/fileMonitor.c -o common/fileMonitor.o
peer/peer.o: peer/peer.c peer/peer.h $(DEPS)
	$(CC) $(CFLAGS) -c peer/peer.c -o peer/peer.o
tracker/tracker.o: tracker/tracker.c tracker/tracker.h
	$(CC) $(CFLAGS) -c tracker/tracker.c -o tracker/tracker.o

clean:
	rm -f peer/*.o
	rm -f tracker/*.o
	rm -f common/*.o
	rm -f peer/DartSync_peer
	rm -f tracker/DartSync_tracker

