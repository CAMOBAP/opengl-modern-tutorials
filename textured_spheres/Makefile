LDLIBS=-lglut -lSOIL -lGLEW -lGL -lm
all: sphere
clean:
	rm -f *.o sphere
sphere: ../common/shader_utils.o
.PHONY: all clean

# Tests:
# ./sphere Earthmap720x360_grid.jpg sphere.v.glsl sphere.f.glsl
# ./sphere Earthmap720x360_grid.jpg sphere.v.glsl sphere_ST.f.glsl
# ./sphere Earthmap720x360_grid.jpg sphere-gouraud.v.glsl sphere-gouraud.f.glsl
# ./sphere Land_shallow_topo_alpha_2048.png sphere-phong.v.glsl sphere-phong.f.glsl
