CC=g++
CXXFLAGS=-ggdb
LDLIBS=-lglut -lGLEW -lGL -lm
all: suzanne
suzanne: ../common/shader_utils.o

test:
	./suzanne suzanne.obj phong-shading.v.glsl phong-shading.f.glsl
