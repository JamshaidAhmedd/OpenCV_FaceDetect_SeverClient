CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread `pkg-config --cflags opencv4`
LIBS   = `pkg-config --libs opencv4` -lpthread

# If pkg-config above doesn’t work or OpenCV 3.4.6 isn't in pkg-config, you can manually specify:
# CFLAGS = -Wall -Wextra -std=c99 -I/usr/local/include/opencv4 -pthread
# LIBS   = -L/usr/local/lib -lopencv_core -lopencv_imgcodecs -lopencv_objdetect -lopencv_imgproc -lpthread

OBJ_SERVER = uqfacedetect.o protocol.o
OBJ_CLIENT = uqfaceclient.o protocol.o

# Default target
all: uqfacedetect uqfaceclient

# Build server binary
uqfacedetect: $(OBJ_SERVER)
	$(CC) $(CFLAGS) -o $@ $(OBJ_SERVER) $(LIBS)

# Build client binary
uqfaceclient: $(OBJ_CLIENT)
	$(CC) $(CFLAGS) -o $@ $(OBJ_CLIENT) $(LIBS)

# Compile .c to .o (pattern rule)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f *.o uqfacedetect uqfaceclient
