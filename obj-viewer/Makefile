CXXFLAGS=-ggdb
LDLIBS=-lglut -lGLEW -lGL -lm
all: suzanne
suzanne: ../common/shader_utils.o
clean:
	rm -f *.o suzanne

test:
	./suzanne suzanne.obj phong-shading.v.glsl phong-shading.f.glsl
.PHONY: all clean test
