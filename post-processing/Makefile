CXXFLAGS=-ggdb
LDLIBS=-lglut -lGLEW -lGL -lm
all: post-processing
post-processing: ../common/shader_utils.o
clean:
	rm -f *.o post-processing

.PHONY: all clean
