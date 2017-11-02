# program executable name
TARGET = /export/nfs/root/miniproject

# GCC compiler
CC = /home/student/Desktop/Ex8/buildroot-avr32-v3.0.0_TTK4147/output/staging/usr/bin/avr32-linux-gcc

# compiler flags
CFLAGS = -g -Wall

# linker flags
LDFLAGS = -g -pthread 

# list of sources
SOURCES = $(shell find -name "*.c")

# default rule, to compile everything
all: $(TARGET)

# define object files
OBJECTS = $(SOURCES:.c=.o)



# link programs
$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

# compile
%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# cleaning
clean:
	rm -f $(TARGET) $(OBJECTS)
