uniform sampler2D mytexture;
varying vec4 f_color;
uniform bool sprite;

void main(void) {
  if(sprite)
    gl_FragColor = texture2D(mytexture, gl_PointCoord);
  else
    gl_FragColor = f_color;
}
