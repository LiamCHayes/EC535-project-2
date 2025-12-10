CROSS_COMPILE := arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
#Samuel Gossett spgosse
CFLAGS := -Wall -static

TARGET := meteor
SOURCES := meteor.c imu_driver.c
OBJECTS := $(SOURCES:.c=.o)
HEADERS := imu_driver.h

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f meteor imu_driver.o meteor.o
