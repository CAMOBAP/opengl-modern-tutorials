CC=g++
LDLIBS=-lglut -lGLEW -lGL -lm
all: select
clean:
	rm -f *.o select
select.o: res_texture.c
select: ../common/shader_utils.o
.PHONY: all clean
