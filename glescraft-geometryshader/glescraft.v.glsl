#version 120

attribute vec4 coord;

void main(void) {
	gl_Position = coord;
}
