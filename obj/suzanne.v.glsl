attribute vec3 coord3d;
attribute vec3 v_normal;
uniform mat4 m, v, p;
uniform mat4 m_inv;
varying vec3 f_color;

vec3 light_direction_world = vec3(-1.0, 1.0, -1.0);
vec3 light_diffuse = vec3(1.0, 1.0, 1.0);
vec3 material_diffuse = vec3(1.0, 0.8, 0.8);

void main(void) {
  mat4 mvp = p*v*m;
  gl_Position = mvp * vec4(coord3d, 1.0);
  vec3 v_normal_world = (vec4(v_normal, 0.0) * m_inv).xyz;
  vec3 diffuse_reflection = light_diffuse * material_diffuse * max(0.0, dot(v_normal_world, light_direction_world));
  f_color = diffuse_reflection;
}
