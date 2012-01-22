#ifdef GLES2
varying lowp vec3 f_color;
#else
varying vec3 f_color;
#endif
void main(void) {
  gl_FragColor = vec4(f_color.x, f_color.y, f_color.z, 1.0);
}
