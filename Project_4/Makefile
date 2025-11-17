# Compile with gcc
CC = gcc
# Compile flags, Wall=warnings, 02=code optimization, for gettaddrinfo
CFLAGS = -Wall -O2 -std=gnu1x
# Linker flags for external libraries, lssl=OpenSSL, lcrypto=Cryptography, lpthread=Threading (JK)
LDFLAGS = -lssl -lcrypto -lpthread

# Define the target executable name and source C file
TARGET = tcp_traceroute
SRC = tcp_traceroute.c

# Define build target for compilation
all: $(TARGET)

# Let make know that TARGET depends on SRC, also add arguments
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

# Define executable deletion
clean:
	rm -f $(TARGET)
