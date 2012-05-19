attribute vec3 v_coord;
attribute vec2 texcoord;
varying vec4 texCoords;
uniform mat4 m,v,p;

void main(void) {
    mat4 mvp = p*v*m;
    gl_Position = mvp * vec4(v_coord, 1.0);
    texCoords = vec4(v_coord, 1.0);
}
