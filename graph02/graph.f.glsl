#ifdef GLES2
varying lowp vec4 f_color;
# else
varying vec4 f_color;
#endif

void main(void) {
  gl_FragColor = f_color;
}
