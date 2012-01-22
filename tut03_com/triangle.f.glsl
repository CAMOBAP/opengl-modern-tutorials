// #ifdef GLES2
// varying lowp vec3 f_color;
// uniform lowp float fade;
// #else
// varying vec3 f_color;
// uniform float fade;
// #endif

// or:
// #ifdef GLES2
// precision lowp float;
// #endif
// varying vec3 f_color;
// uniform float fade;

// or:
#ifdef GLES2
precision lowp float;
uniform mediump float fade;
#else
uniform float fade;
#endif
varying vec3 f_color;

void main(void) {
  gl_FragColor = vec4(f_color.x, f_color.y, f_color.z, fade);
}
