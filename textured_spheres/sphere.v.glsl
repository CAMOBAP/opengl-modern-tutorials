attribute vec3 coord3d;
attribute vec2 texcoord;
varying vec4 texCoords;
uniform mat4 mvp;

void main(void) {
    gl_Position = mvp * vec4(coord3d, 1.0);
    texCoords = vec4(coord3d, 1.0);
}
