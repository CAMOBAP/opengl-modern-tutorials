/**
 * This file is in the public domain.
 * Contributors: Sylvain Beucler
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
/* Use glew.h instead of gl.h to get all the GL prototypes declared */
#include <GL/glew.h>
/* Using the GLUT library for the base windowing setup */
#include <GL/glut.h>
/* GLM */
// #define GLM_MESSAGES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GROUND_SIZE 20

int screen_width=800, screen_height=600;
GLuint program;
GLint attribute_v_coord;
GLint attribute_v_normal;
GLint uniform_m, uniform_v, uniform_p;
GLint uniform_m_3x3_inv_transp, uniform_v_inv;
bool compute_arcball;
int last_mx = 0, last_my = 0, cur_mx = 0, cur_my = 0;
int arcball_on = false;

GLuint fbo, fbo_texture;
GLuint vbo_fbo_vertices;
GLuint program_postproc, attribute_v_coord_postproc, uniform_fbo_texture, uniform_offset;

using namespace std;

enum MODES { MODE_OBJECT, MODE_CAMERA, MODE_LIGHT, MODE_LAST } view_mode;
int rotY_direction = 0, rotX_direction = 0, transZ_direction = 0;
glm::mat4 transforms[MODE_LAST];
int last_ticks = 0;

struct mesh {
  vector<glm::vec4> vertices;
  vector<glm::vec3> normals;
  vector<GLushort> elements;
  glm::mat4 object2world;
  GLuint vbo_vertices, vbo_normals, ibo_elements;
};
struct mesh ground, mesh, light_bbox;


/**
 * Store all the file's contents in memory, useful to pass shaders
 * source code to OpenGL
 */
char* file_read(const char* filename)
{
  FILE* in = fopen(filename, "rb");
  if (in == NULL) return NULL;

  int res_size = BUFSIZ;
  char* res = (char*)malloc(res_size);
  int nb_read_total = 0;

  while (!feof(in) && !ferror(in)) {
    if (nb_read_total + BUFSIZ > res_size) {
      if (res_size > 10*1024*1024) break;
      res_size = res_size * 2;
      res = (char*)realloc(res, res_size);
    }
    char* p_res = res + nb_read_total;
    nb_read_total += fread(p_res, 1, BUFSIZ, in);
  }
  
  fclose(in);
  res = (char*)realloc(res, nb_read_total + 1);
  res[nb_read_total] = '\0';
  return res;
}

/**
 * Display compilation errors from the OpenGL shader compiler
 */
void print_log(GLuint object)
{
  GLint log_length = 0;
  if (glIsShader(object))
    glGetShaderiv(object, GL_INFO_LOG_LENGTH, &log_length);
  else if (glIsProgram(object))
    glGetProgramiv(object, GL_INFO_LOG_LENGTH, &log_length);
  else {
    fprintf(stderr, "printlog: Not a shader or a program\n");
    return;
  }

  char* log = (char*)malloc(log_length);

  if (glIsShader(object))
    glGetShaderInfoLog(object, log_length, NULL, log);
  else if (glIsProgram(object))
    glGetProgramInfoLog(object, log_length, NULL, log);

  fprintf(stderr, "%s", log);
  free(log);
}

/**
 * Compile the shader from file 'filename', with error handling
 */
GLint create_shader(const char* filename, GLenum type)
{
  const GLchar* source = file_read(filename);
  if (source == NULL) {
    fprintf(stderr, "Error opening %s: ", filename); perror("");
    return 0;
  }
  GLuint res = glCreateShader(type);
  glShaderSource(res, 1, &source, NULL);
  free((void*)source);

  glCompileShader(res);
  GLint compile_ok = GL_FALSE;
  glGetShaderiv(res, GL_COMPILE_STATUS, &compile_ok);
  if (compile_ok == GL_FALSE) {
    fprintf(stderr, "%s:", filename);
    print_log(res);
    glDeleteShader(res);
    return 0;
  }

  return res;
}

void load_obj(const char* filename, struct mesh* mesh) {
  ifstream in(filename, ios::in);
  if (!in) { cerr << "Cannot open " << filename << endl; exit(1); }
  vector<int> nb_seen;

  string line;
  while (getline(in, line)) {
    if (line.substr(0,2) == "v ") {
      istringstream s(line.substr(2));
      glm::vec4 v; s >> v.x; s >> v.y; s >> v.z; v.w = 1.0;
      mesh->vertices.push_back(v);
    }  else if (line.substr(0,2) == "f ") {
      istringstream s(line.substr(2));
      GLushort a,b,c;
      s >> a; s >> b; s >> c;
      a--; b--; c--;
      mesh->elements.push_back(a); mesh->elements.push_back(b); mesh->elements.push_back(c);
    }
    else if (line[0] == '#') { /* ignoring this line */ }
    else { /* ignoring this line */ }
  }

  mesh->normals.resize(mesh->vertices.size(), glm::vec3(0.0, 0.0, 0.0));
  nb_seen.resize(mesh->vertices.size(), 0);
  for (int i = 0; i < mesh->elements.size(); i+=3) {
    GLushort ia = mesh->elements[i];
    GLushort ib = mesh->elements[i+1];
    GLushort ic = mesh->elements[i+2];
    glm::vec3 normal = glm::normalize(glm::cross(
      glm::vec3(mesh->vertices[ib]) - glm::vec3(mesh->vertices[ia]),
      glm::vec3(mesh->vertices[ic]) - glm::vec3(mesh->vertices[ia])));

    int v[3];  v[0] = ia;  v[1] = ib;  v[2] = ic;
    for (int j = 0; j < 3; j++) {
      GLushort cur_v = v[j];
      nb_seen[cur_v]++;
      if (nb_seen[cur_v] == 1) {
	mesh->normals[cur_v] = normal;
      } else {
	// average
	mesh->normals[cur_v].x = mesh->normals[cur_v].x * (1.0 - 1.0/nb_seen[cur_v]) + normal.x * 1.0/nb_seen[cur_v];
	mesh->normals[cur_v].y = mesh->normals[cur_v].y * (1.0 - 1.0/nb_seen[cur_v]) + normal.y * 1.0/nb_seen[cur_v];
	mesh->normals[cur_v].z = mesh->normals[cur_v].z * (1.0 - 1.0/nb_seen[cur_v]) + normal.z * 1.0/nb_seen[cur_v];
	mesh->normals[cur_v] = glm::normalize(mesh->normals[cur_v]);
      }
    }
  }
}

void upload_mesh(struct mesh* mesh) {
  if (mesh->vertices.size() > 0) {
    glGenBuffers(1, &mesh->vbo_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo_vertices);
    glBufferData(GL_ARRAY_BUFFER, mesh->vertices.size() * sizeof(mesh->vertices[0]),
		 mesh->vertices.data(), GL_STATIC_DRAW);
  }
  
  if (mesh->normals.size() > 0) {
    glGenBuffers(1, &mesh->vbo_normals);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo_normals);
    glBufferData(GL_ARRAY_BUFFER, mesh->normals.size() * sizeof(mesh->normals[0]),
		 mesh->normals.data(), GL_STATIC_DRAW);
  }

  if (mesh->elements.size() > 0) {
    glGenBuffers(1, &mesh->ibo_elements);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo_elements);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->elements.size() * sizeof(mesh->elements[0]),
		 mesh->elements.data(), GL_STATIC_DRAW);
  }
}

int init_resources(char* model_filename, char* vshader_filename, char* fshader_filename)
{
  load_obj(model_filename, &mesh);

  for (int i = -GROUND_SIZE/2; i < GROUND_SIZE/2; i++) {
    for (int j = -GROUND_SIZE/2; j < GROUND_SIZE/2; j++) {
      ground.vertices.push_back(glm::vec4(i,   0.0,  j+1, 1.0));
      ground.vertices.push_back(glm::vec4(i+1, 0.0,  j+1, 1.0));
      ground.vertices.push_back(glm::vec4(i,   0.0,  j,   1.0));
      ground.vertices.push_back(glm::vec4(i,   0.0,  j,   1.0));
      ground.vertices.push_back(glm::vec4(i+1, 0.0,  j+1, 1.0));
      ground.vertices.push_back(glm::vec4(i+1, 0.0,  j,   1.0));
      for (int k = 0; k < 6; k++)
	ground.normals.push_back(glm::vec3(0.0, 1.0, 0.0));
    }
  }

  glm::vec4 light_position = glm::vec4(0.0,  1.0,  2.0, 1.0);
  light_bbox.vertices.push_back(light_position - glm::vec4(-0.1, -0.1, -0.1, 0.0));
  light_bbox.vertices.push_back(light_position - glm::vec4( 0.1, -0.1, -0.1, 0.0));
  light_bbox.vertices.push_back(light_position - glm::vec4( 0.1,  0.1, -0.1, 0.0));
  light_bbox.vertices.push_back(light_position - glm::vec4(-0.1,  0.1, -0.1, 0.0));
  light_bbox.vertices.push_back(light_position - glm::vec4(-0.1, -0.1,  0.1, 0.0));
  light_bbox.vertices.push_back(light_position - glm::vec4( 0.1, -0.1,  0.1, 0.0));
  light_bbox.vertices.push_back(light_position - glm::vec4( 0.1,  0.1,  0.1, 0.0));
  light_bbox.vertices.push_back(light_position - glm::vec4(-0.1,  0.1,  0.1, 0.0));

  GLushort light_bbox_elements[] = {
    0, 1, 2, 3,
    4, 5, 6, 7,
    0, 4, 1, 5, 2, 6, 3, 7
  };
  for (int i = 0; i < sizeof(light_bbox_elements)/sizeof(light_bbox_elements[0]); i++)
    light_bbox.elements.push_back(light_bbox_elements[i]);

  upload_mesh(&mesh);
  upload_mesh(&ground);
  upload_mesh(&light_bbox);
 


  /* Create back-buffer, used for post-processing */

  /* Texture */
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &fbo_texture);
  glBindTexture(GL_TEXTURE_2D, fbo_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

  /* Depth buffer */
  GLuint rbo;
  glGenRenderbuffers(1, &rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, screen_width, screen_height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  /* Framebuffer to link everything together */
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
  GLenum status;
  if ((status = glCheckFramebufferStatus(GL_FRAMEBUFFER)) != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "glCheckFramebufferStatus: error %p", status);
    return 0;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  GLfloat fbo_vertices[] = {
    -1, -1,
     1, -1,
    -1,  1,
     1,  1,
  };
  glGenBuffers(1, &vbo_fbo_vertices);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_fbo_vertices);
  glBufferData(GL_ARRAY_BUFFER, sizeof(fbo_vertices), fbo_vertices, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);


  /* Compile and link shaders */
  GLint link_ok = GL_FALSE;
  GLint validate_ok = GL_FALSE;

  GLuint vs, fs;
  if ((vs = create_shader(vshader_filename, GL_VERTEX_SHADER))   == 0) return 0;
  if ((fs = create_shader(fshader_filename, GL_FRAGMENT_SHADER)) == 0) return 0;

  program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
  if (!link_ok) {
    fprintf(stderr, "glLinkProgram:");
    print_log(program);
    return 0;
  }
  glValidateProgram(program);
  glGetProgramiv(program, GL_VALIDATE_STATUS, &validate_ok);
  if (!validate_ok) {
    fprintf(stderr, "glValidateProgram:");
    print_log(program);
  }

  const char* attribute_name;
  attribute_name = "v_coord";
  attribute_v_coord = glGetAttribLocation(program, attribute_name);
  if (attribute_v_coord == -1) {
    fprintf(stderr, "Could not bind attribute %s\n", attribute_name);
    return 0;
  }
  attribute_name = "v_normal";
  attribute_v_normal = glGetAttribLocation(program, attribute_name);
  if (attribute_v_normal == -1) {
    fprintf(stderr, "Could not bind attribute %s\n", attribute_name);
    return 0;
  }
  const char* uniform_name;
  uniform_name = "m";
  uniform_m = glGetUniformLocation(program, uniform_name);
  if (uniform_m == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }
  uniform_name = "v";
  uniform_v = glGetUniformLocation(program, uniform_name);
  if (uniform_v == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }
  uniform_name = "p";
  uniform_p = glGetUniformLocation(program, uniform_name);
  if (uniform_p == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }
  uniform_name = "m_3x3_inv_transp";
  uniform_m_3x3_inv_transp = glGetUniformLocation(program, uniform_name);
  if (uniform_m_3x3_inv_transp == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }
  uniform_name = "v_inv";
  uniform_v_inv = glGetUniformLocation(program, uniform_name);
  if (uniform_v_inv == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }


  /* Post-processing */
  if ((vs = create_shader("postproc.v.glsl", GL_VERTEX_SHADER))   == 0) return 0;
  if ((fs = create_shader("postproc.f.glsl", GL_FRAGMENT_SHADER)) == 0) return 0;

  program_postproc = glCreateProgram();
  glAttachShader(program_postproc, vs);
  glAttachShader(program_postproc, fs);
  glLinkProgram(program_postproc);
  glGetProgramiv(program_postproc, GL_LINK_STATUS, &link_ok);
  if (!link_ok) {
    fprintf(stderr, "glLinkProgram:");
    print_log(program_postproc);
    return 0;
  }
  glValidateProgram(program_postproc);
  glGetProgramiv(program_postproc, GL_VALIDATE_STATUS, &validate_ok);
  if (!validate_ok) {
    fprintf(stderr, "glValidateProgram:");
    print_log(program_postproc);
  }

  attribute_name = "v_coord";
  attribute_v_coord_postproc = glGetAttribLocation(program_postproc, attribute_name);
  if (attribute_v_coord_postproc == -1) {
    fprintf(stderr, "Could not bind attribute %s\n", attribute_name);
    return 0;
  }

  uniform_name = "fbo_texture";
  uniform_fbo_texture = glGetUniformLocation(program_postproc, uniform_name);
  if (uniform_fbo_texture == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }

  uniform_name = "offset";
  uniform_offset = glGetUniformLocation(program_postproc, uniform_name);
  if (uniform_offset == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }

  return 1;
}

void init_view() {
  mesh.object2world = glm::mat4(1.0);
  transforms[MODE_CAMERA] = glm::lookAt(
    glm::vec3(0.0,  2.0, 4.0),   // eye
    glm::vec3(0.0,  0.0, 0.0),   // direction
    glm::vec3(0.0,  1.0, 0.0));  // up
}

void onSpecial(int key, int x, int y) {
  switch (key) {
  case GLUT_KEY_F1:
    view_mode = MODE_OBJECT;
    break;
  case GLUT_KEY_F2:
    view_mode = MODE_CAMERA;
    break;
  case GLUT_KEY_F3:
    view_mode = MODE_LIGHT;
    break;
  case GLUT_KEY_LEFT:
    rotY_direction = 1;
    break;
  case GLUT_KEY_RIGHT:
    rotY_direction = -1;
    break;
  case GLUT_KEY_UP:
    transZ_direction = 1;
    break;
  case GLUT_KEY_DOWN:
    transZ_direction = -1;
    break;
  case GLUT_KEY_PAGE_UP:
    rotX_direction = -1;
    break;
  case GLUT_KEY_PAGE_DOWN:
    rotX_direction = 1;
    break;
  case GLUT_KEY_HOME:
    init_view();
    break;
  }
}

void onSpecialUp(int key, int x, int y) {
  switch (key) {
  case GLUT_KEY_LEFT:
  case GLUT_KEY_RIGHT:
    rotY_direction = 0;
    break;
  case GLUT_KEY_UP:
  case GLUT_KEY_DOWN:
    transZ_direction = 0;
    break;
  case GLUT_KEY_PAGE_UP:
  case GLUT_KEY_PAGE_DOWN:
    rotX_direction = 0;
    break;
  }
}

/**
 * Get a normalized vector from the center of the virtual ball O to a
 * point P on the virtual ball surface, such that P is aligned on
 * screen's (X,Y) coordinates.  If (X,Y) is too far away from the
 * sphere, return the nearest point on the virtual ball surface.
 */
glm::vec3 get_arcball_vector(int x, int y) {
  glm::vec3 P = glm::vec3(1.0*x/screen_width*2 - 1.0,
			  1.0*y/screen_height*2 - 1.0,
			  0);
  P.y = -P.y;
  float OP_squared = P.x * P.x + P.y * P.y;
  if (OP_squared <= 1*1)
    P.z = sqrt(1*1 - OP_squared);  // Pythagore
  else
    P = glm::normalize(P);  // nearest point
  return P;
}

void idle() {
  /* Handle keyboard-based transformations */
  int delta_t = glutGet(GLUT_ELAPSED_TIME) - last_ticks;
  last_ticks = glutGet(GLUT_ELAPSED_TIME);
  float delta_rotY = rotY_direction * delta_t / 1000.0 * 120;  // 120° per second
  float delta_rotX = rotX_direction * delta_t / 1000.0 * 120;  // 120° per second;
  float delta_transZ = transZ_direction * delta_t / 1000.0 * 5;  // 5 units per second;
  
  if (view_mode == MODE_OBJECT) {
    mesh.object2world = glm::rotate(mesh.object2world, delta_rotY, glm::vec3(0.0, 1.0, 0.0));
    mesh.object2world = glm::rotate(mesh.object2world, delta_rotX, glm::vec3(1.0, 0.0, 0.0));
    mesh.object2world = glm::translate(mesh.object2world, glm::vec3(0.0, 0.0, delta_transZ));
  } else if (view_mode == MODE_CAMERA) {
    // Camera is reverse-facing, so reverse Z translation and X rotation.
    // Plus, the View matrix is the inverse of the camera2world (it's
    // world->camera), so we'll reverse the transformations.
    // Alternatively, imagine that you transform the world, instead of positioning the camera.
    transforms[MODE_CAMERA] = glm::rotate(glm::mat4(1.0), -delta_rotY, glm::vec3(0.0, 1.0, 0.0)) * transforms[MODE_CAMERA];
    transforms[MODE_CAMERA] = glm::rotate(glm::mat4(1.0), delta_rotX, glm::vec3(1.0, 0.0, 0.0)) * transforms[MODE_CAMERA];
    transforms[MODE_CAMERA] = glm::translate(glm::mat4(1.0), glm::vec3(0.0, 0.0, delta_transZ)) * transforms[MODE_CAMERA];
  }

  /* Handle arcball */
  if (cur_mx != last_mx || cur_my != last_my) {
    glm::vec3 va = get_arcball_vector(last_mx, last_my);
    glm::vec3 vb = get_arcball_vector( cur_mx,  cur_my);
    float angle = acos(min(1.0f, glm::dot(va, vb)));
    glm::vec3 axis_in_camera_coord = glm::cross(va, vb);
    glm::mat3 camera2object = glm::inverse(glm::mat3(transforms[MODE_CAMERA]) * glm::mat3(mesh.object2world));
    glm::vec3 axis_in_object_coord = camera2object * axis_in_camera_coord;
    mesh.object2world = glm::rotate(mesh.object2world, glm::degrees(angle), axis_in_object_coord);
    last_mx = cur_mx;
    last_my = cur_my;
  }

  // Model
  // Set in onDisplay() - cf. mesh.object2world

  // View
  glm::mat4 world2camera = transforms[MODE_CAMERA];

  // Projection
  glm::mat4 camera2screen = glm::perspective(45.0f, 1.0f*screen_width/screen_height, 0.1f, 100.0f);

  glUseProgram(program);
  glUniformMatrix4fv(uniform_v, 1, GL_FALSE, glm::value_ptr(world2camera));
  glUniformMatrix4fv(uniform_p, 1, GL_FALSE, glm::value_ptr(camera2screen));

  glm::mat4 v_inv = glm::inverse(world2camera);
  glUniformMatrix4fv(uniform_v_inv, 1, GL_FALSE, glm::value_ptr(v_inv));

  glUseProgram(program_postproc);
  GLfloat move = glutGet(GLUT_ELAPSED_TIME) / 1000.0 * 2*3.14159 * .75;  // 3/4 of a wave cycle per second
  glUniform1f(uniform_offset, move);

  glutPostRedisplay();
}

void draw_mesh(struct mesh* mesh) {
  // Describe our vertices array to OpenGL (it can't guess its format automatically)
  glEnableVertexAttribArray(attribute_v_coord);
  glEnableVertexAttribArray(attribute_v_normal);

  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo_vertices);
  glVertexAttribPointer(
    attribute_v_coord,  // attribute
    4,                  // number of elements per vertex, here (x,y,z,w)
    GL_FLOAT,           // the type of each element
    GL_FALSE,           // take our values as-is
    0,                  // no extra data between each position
    0                   // offset of first element
  );

  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo_normals);
  glVertexAttribPointer(
    attribute_v_normal, // attribute
    3,                  // number of elements per vertex, here (x,y,z)
    GL_FLOAT,           // the type of each element
    GL_FALSE,           // take our values as-is
    0,                  // no extra data between each position
    0                   // offset of first element
  );

  /* Apply object's transformation matrix */
  glUniformMatrix4fv(uniform_m, 1, GL_FALSE, glm::value_ptr(mesh->object2world));
  glm::mat3 m_3x3_inv_transp = glm::transpose(glm::inverse(glm::mat3(mesh->object2world)));
  glUniformMatrix3fv(uniform_m_3x3_inv_transp, 1, GL_FALSE, glm::value_ptr(m_3x3_inv_transp));

  /* Push each element in buffer_vertices to the vertex shader */
  if (mesh->ibo_elements != 0) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo_elements);
    int size;  glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
    glDrawElements(GL_TRIANGLES, size/sizeof(GLushort), GL_UNSIGNED_SHORT, 0);
  } else {
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertices.size());
  }

  glDisableVertexAttribArray(attribute_v_coord);
  glDisableVertexAttribArray(attribute_v_normal);
}

void draw_light_bbox() {
  glEnableVertexAttribArray(attribute_v_coord);
  glBindBuffer(GL_ARRAY_BUFFER, light_bbox.vbo_vertices);
  glVertexAttribPointer(
    attribute_v_coord,  // attribute
    4,                  // number of elements per vertex, here (x,y,z,w)
    GL_FLOAT,           // the type of each element
    GL_FALSE,           // take our values as-is
    0,                  // no extra data between each position
    0                   // offset of first element
  );

  glUniformMatrix4fv(uniform_m, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0)));
  glm::mat3 m_3x3_inv_transp = glm::transpose(glm::inverse(glm::mat3(1.0)));
  glUniformMatrix3fv(uniform_m_3x3_inv_transp, 1, GL_FALSE, glm::value_ptr(m_3x3_inv_transp));

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, light_bbox.ibo_elements);
  glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_SHORT, 0);
  glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_SHORT, (GLvoid*)(4*sizeof(GLushort)));
  glDrawElements(GL_LINES, 8, GL_UNSIGNED_SHORT, (GLvoid*)(8*sizeof(GLushort)));

  glDisableVertexAttribArray(attribute_v_coord);
}

void display()
{
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glClearColor(0.45, 0.45, 0.45, 1.0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glUseProgram(program);


  draw_mesh(&mesh);
  draw_mesh(&ground);
  draw_light_bbox();


  /* Post-processing */
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glUseProgram(program_postproc);
  glBindTexture(GL_TEXTURE_2D, fbo_texture);
  glUniform1i(uniform_fbo_texture, /*GL_TEXTURE*/0);
  glEnableVertexAttribArray(attribute_v_coord_postproc);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_fbo_vertices);
  glVertexAttribPointer(
    attribute_v_coord_postproc,  // attribute
    2,                  // number of elements per vertex, here (x,y)
    GL_FLOAT,           // the type of each element
    GL_FALSE,           // take our values as-is
    0,                  // no extra data between each position
    0                   // offset of first element
  );
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDisableVertexAttribArray(attribute_v_coord_postproc);

  glutSwapBuffers();
}

void onMouse(int button, int state, int x, int y) {
  if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
    arcball_on = true;
    last_mx = cur_mx = x;
    last_my = cur_my = y;
  } else {
    arcball_on = false;
  }
}

void onMotion(int x, int y) {
  if (arcball_on) {  // if left button is pressed
    cur_mx = x;
    cur_my = y;
  }
}

void onReshape(int width, int height) {
  screen_width = width;
  screen_height = height;
  glViewport(0, 0, screen_width, screen_height);
}

void free_resources()
{
  glDeleteProgram(program);
  glDeleteProgram(program_postproc);
}


int main(int argc, char* argv[]) {
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA|GLUT_ALPHA|GLUT_DOUBLE|GLUT_DEPTH);
  glutInitWindowSize(screen_width, screen_height);
  glutCreateWindow("OBJ viewer");

  GLenum glew_status = glewInit();
  if (glew_status != GLEW_OK) {
    fprintf(stderr, "Error: %s\n", glewGetErrorString(glew_status));
    return 1;
  }

  if (!GLEW_VERSION_2_0) {
    fprintf(stderr, "Error: your graphic card does not support OpenGL 2.0\n");
    return 1;
  }

  if (argc != 4) {
    fprintf(stderr, "Usage: %s model.obj vertex_shader.v.glsl fragment_shader.f.glsl\n", argv[0]);
    exit(0);
  }

  if (init_resources(argv[1], argv[2], argv[3])) {
    init_view();
    glutDisplayFunc(display);
    glutSpecialFunc(onSpecial);
    glutSpecialUpFunc(onSpecialUp);
    glutMouseFunc(onMouse);
    glutMotionFunc(onMotion);
    glutReshapeFunc(onReshape);
    glutIdleFunc(idle);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    last_ticks = glutGet(GLUT_ELAPSED_TIME);
    glutMainLoop();
  }

  free_resources();
  return 0;
}
