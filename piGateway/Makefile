CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-g
SOURCES = GatewayBase.c
OBJECTS=$(SOURCES:.c=.o) 
INCLUDE_OBJECTS=-lpthread -lmosquitto
EXECUTABLE=piGatewayBase
INSTALL=install

prefix = /usr/local
# The directory to install in.
bindir = $(prefix)/bin

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(SOURCES) $(INCLUDE_OBJECTS) -o $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

install:
	$(INSTALL) $(EXECUTABLE) $(bindir)/$(EXECUTABLE)
