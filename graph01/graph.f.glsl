uniform sampler2D mytexture;
#ifdef GLES2
varying lowp vec4 f_color;
uniform lowp float sprite;
#else
varying vec4 f_color;
uniform float sprite;
#endif

void main(void) {
  if(sprite > 1.0)
    gl_FragColor = texture2D(mytexture, gl_PointCoord) * f_color;
  else
    gl_FragColor = f_color;
}
