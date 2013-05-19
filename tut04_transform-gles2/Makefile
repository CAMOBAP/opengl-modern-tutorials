CPP_FLAGS=-DFREEGLUT_GLES2 -I /tmp/freeglut-native-gles2/include
LDLIBS=-L/tmp/freeglut-native-gles2/lib -lfreeglut-gles2 -lGLESv2 -lm
all: triangle
clean:
	rm -f *.o triangle
triangle: shader_utils.o
run:
	LD_LIBRARY_PATH=/tmp/freeglut-native-gles2/lib/ ./triangle

.PHONY: all clean run
