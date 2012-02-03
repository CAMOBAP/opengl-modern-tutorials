CXXFLAGS=-ggdb
LDLIBS=-lglut -lGLEW -lGL -lm
all: teapot
clean:
	rm -f *.o teapot
teapot: ../common/shader_utils.o teapot.o
.PHONY: all clean
