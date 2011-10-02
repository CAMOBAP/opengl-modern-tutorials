#version 120

uniform sampler2D fbo_texture;
varying vec2 f_texcoord;

void main(void) {
  vec2 texcoord = f_texcoord;
  texcoord.x += sin(texcoord.y * 20) / 50;
  gl_FragColor = texture2D(fbo_texture, texcoord);
}
