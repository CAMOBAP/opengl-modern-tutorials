LDLIBS=-lglut -lSOIL -lGLEW -lGL -lm
all: sphere
clean:
	rm -f *.o sphere
sphere: ../common/shader_utils.o
.PHONY: all clean

# Tests:
# ./sphere sphere.v.glsl sphere.f.glsl
# ./sphere sphere.v.glsl sphere_ST.f.glsl
# ./sphere sphere-gouraud.v.glsl sphere-gouraud.f.glsl
