LDLIBS=-lglut -lSOIL -lGLEW -lGL -lm
all: sphere
clean:
	rm -f *.o sphere
sphere: ../common/shader_utils.o
.PHONY: all clean

# - Textures Spheres
#   ./sphere Earthmap720x360_grid.jpg sphere.v.glsl sphere.f.glsl
#   ./sphere Earthmap720x360_grid.jpg sphere.v.glsl sphere_ST.f.glsl
# - Lighting Textured Surfaces
#   ./sphere Earthmap720x360_grid.jpg sphere-gouraud.v.glsl sphere-gouraud.f.glsl
# - Glossy Textures
#   ./sphere Land_shallow_topo_alpha_2048.png sphere-phong.v.glsl sphere-phong.f.glsl
#   ./sphere Land_shallow_topo_alpha_2048.png sphere-gouraud.v.glsl sphere-gouraud-glossy.f.glsl
# - Transparent Textures
#   ./sphere Land_shallow_topo_alpha_2048.png sphere.v.glsl sphere_discard.f.glsl
#   ./sphere Land_shallow_topo_alpha_2048.png sphere.v.glsl sphere.f.glsl
#   ./sphere Land_shallow_topo_alpha_2048.png sphere.v.glsl sphere_oceans.f.glsl
# - Layers of Textures
#   ./sphere Earth_lights_lrg.jpg sphere-sunlit.v.glsl sphere-sunlit.f.glsl
