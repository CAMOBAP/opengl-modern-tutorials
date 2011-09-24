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
GLuint vbo_mesh_vertices, vbo_mesh_normals, vbo_mesh_elements,
  vbo_ground_vertices, vbo_ground_normals,
  vbo_light_bbox_vertices, vbo_light_bbox_elements;
GLuint program;
GLint attribute_v_coord;
GLint attribute_v_normal;
GLint uniform_m, uniform_v, uniform_p;
GLint uniform_m_3x3_inv_transp, uniform_v_inv;
bool compute_arcball;
int last_mx = 0, last_my = 0, cur_mx = 0, cur_my = 0;
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
} mesh;

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
  for (int i = 0; i < mesh->elements.size(); i+=3) {
    GLushort ia = mesh->elements[i];
    GLushort ib = mesh->elements[i+1];
    GLushort ic = mesh->elements[i+2];
    glm::vec3 normal = glm::normalize(glm::cross(
      glm::vec3(mesh->vertices[ib]) - glm::vec3(mesh->vertices[ia]),
      glm::vec3(mesh->vertices[ic]) - glm::vec3(mesh->vertices[ia])));
    mesh->normals[ia] = mesh->normals[ib] = mesh->normals[ic] = normal;
  }
}

int init_resources(char* model_filename, char* vshader_filename, char* fshader_filename)
{
  load_obj(model_filename, &mesh);

  glGenBuffers(1, &vbo_mesh_vertices);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_mesh_vertices);
  glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(mesh.vertices[0]),
	       mesh.vertices.data(), GL_STATIC_DRAW);
  
  glGenBuffers(1, &vbo_mesh_normals);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_mesh_normals);
  glBufferData(GL_ARRAY_BUFFER, mesh.normals.size() * sizeof(mesh.normals[0]),
	       mesh.normals.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &vbo_mesh_elements);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_mesh_elements);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.elements.size() * sizeof(mesh.elements[0]),
	       mesh.elements.data(), GL_STATIC_DRAW);


  vector<glm::vec4> ground_vertices;
  vector<glm::vec3> ground_normals;
  for (int i = -GROUND_SIZE/2; i < GROUND_SIZE/2; i++) {
    for (int j = -GROUND_SIZE/2; j < GROUND_SIZE/2; j++) {
      ground_vertices.push_back(glm::vec4(i,   0.0,  j+1, 1.0));
      ground_vertices.push_back(glm::vec4(i+1, 0.0,  j+1, 1.0));
      ground_vertices.push_back(glm::vec4(i,   0.0,  j,   1.0));
      ground_vertices.push_back(glm::vec4(i,   0.0,  j,   1.0));
      ground_vertices.push_back(glm::vec4(i+1, 0.0,  j+1, 1.0));
      ground_vertices.push_back(glm::vec4(i+1, 0.0,  j,   1.0));
      for (int k = 0; k < 6; k++)
	ground_normals.push_back(glm::vec3(0.0, 1.0, 0.0));
    }
  }

  glGenBuffers(1, &vbo_ground_vertices);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_ground_vertices);
  glBufferData(GL_ARRAY_BUFFER, ground_vertices.size() * sizeof(ground_vertices[0]),
	       ground_vertices.data(), GL_STATIC_DRAW);
  
  glGenBuffers(1, &vbo_ground_normals);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_ground_normals);
  glBufferData(GL_ARRAY_BUFFER, ground_normals.size() * sizeof(ground_normals[0]),
	       ground_normals.data(), GL_STATIC_DRAW);

  glm::vec4 light_position = glm::vec4(0.0,  1.0,  2.0, 1.0);
  vector<glm::vec4> light_bbox_vertices;
  light_bbox_vertices.push_back(light_position - glm::vec4(-0.1, -0.1, -0.1, 0.0));
  light_bbox_vertices.push_back(light_position - glm::vec4( 0.1, -0.1, -0.1, 0.0));
  light_bbox_vertices.push_back(light_position - glm::vec4( 0.1,  0.1, -0.1, 0.0));
  light_bbox_vertices.push_back(light_position - glm::vec4(-0.1,  0.1, -0.1, 0.0));
  light_bbox_vertices.push_back(light_position - glm::vec4(-0.1, -0.1,  0.1, 0.0));
  light_bbox_vertices.push_back(light_position - glm::vec4( 0.1, -0.1,  0.1, 0.0));
  light_bbox_vertices.push_back(light_position - glm::vec4( 0.1,  0.1,  0.1, 0.0));
  light_bbox_vertices.push_back(light_position - glm::vec4(-0.1,  0.1,  0.1, 0.0));

  GLushort light_bbox_elements[] = {
    0, 1, 2, 3,
    4, 5, 6, 7,
    0, 4, 1, 5, 2, 6, 3, 7
  };

  glGenBuffers(1, &vbo_light_bbox_vertices);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_light_bbox_vertices);
  glBufferData(GL_ARRAY_BUFFER, light_bbox_vertices.size() * sizeof(light_bbox_vertices[0]),
	       light_bbox_vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &vbo_light_bbox_elements);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_light_bbox_elements);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(light_bbox_elements),
	       light_bbox_elements, GL_STATIC_DRAW);
  

  GLint link_ok = GL_FALSE;

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

  // Model
  // Set in onDisplay() - cf. mesh.object2world

  // View
  glm::mat4 world2camera = transforms[MODE_CAMERA];

  // Projection
  glm::mat4 camera2screen = glm::perspective(45.0f, 1.0f*screen_width/screen_height, 0.1f, 100.0f);

  glUniformMatrix4fv(uniform_v, 1, GL_FALSE, glm::value_ptr(world2camera));
  glUniformMatrix4fv(uniform_p, 1, GL_FALSE, glm::value_ptr(camera2screen));

  glm::mat4 v_inv = glm::inverse(world2camera);
  glUniformMatrix4fv(uniform_v_inv, 1, GL_FALSE, glm::value_ptr(v_inv));
  glutPostRedisplay();
}

void display()
{
  glm::mat3 m_3x3_inv_transp;

  glUseProgram(program);

  glClearColor(0.45, 0.45, 0.45, 1.0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glEnableVertexAttribArray(attribute_v_coord);
  glEnableVertexAttribArray(attribute_v_normal);

  // Describe our vertices array to OpenGL (it can't guess its format automatically)
  glBindBuffer(GL_ARRAY_BUFFER, vbo_mesh_vertices);
  glVertexAttribPointer(
    attribute_v_coord,  // attribute
    4,                  // number of elements per vertex, here (x,y,z,w)
    GL_FLOAT,           // the type of each element
    GL_FALSE,           // take our values as-is
    0,                  // no extra data between each position
    0                   // offset of first element
  );

  glBindBuffer(GL_ARRAY_BUFFER, vbo_mesh_normals);
  glVertexAttribPointer(
    attribute_v_normal, // attribute
    3,                  // number of elements per vertex, here (x,y,z)
    GL_FLOAT,           // the type of each element
    GL_FALSE,           // take our values as-is
    0,                  // no extra data between each position
    0                   // offset of first element
  );

  /* Apply object's transformation matrix */
  glUniformMatrix4fv(uniform_m, 1, GL_FALSE, glm::value_ptr(mesh.object2world));
  m_3x3_inv_transp = glm::transpose(glm::inverse(glm::mat3(mesh.object2world)));
  glUniformMatrix3fv(uniform_m_3x3_inv_transp, 1, GL_FALSE, glm::value_ptr(m_3x3_inv_transp));

  /* Push each element in buffer_vertices to the vertex shader */
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_mesh_elements);
  int size;  glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);  
  glDrawElements(GL_TRIANGLES, size/sizeof(GLushort), GL_UNSIGNED_SHORT, 0);


  glBindBuffer(GL_ARRAY_BUFFER, vbo_ground_vertices);
  glVertexAttribPointer(
    attribute_v_coord,  // attribute
    4,                  // number of elements per vertex, here (x,y,z,w)
    GL_FLOAT,           // the type of each element
    GL_FALSE,           // take our values as-is
    0,                  // no extra data between each position
    0                   // offset of first element
  );
  glBindBuffer(GL_ARRAY_BUFFER, vbo_ground_normals);
  glVertexAttribPointer(
    attribute_v_normal, // attribute
    3,                  // number of elements per vertex, here (x,y,z)
    GL_FLOAT,           // the type of each element
    GL_FALSE,           // take our values as-is
    0,                  // no extra data between each position
    0                   // offset of first element
  );

  glUniformMatrix4fv(uniform_m, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0)));
  m_3x3_inv_transp = glm::transpose(glm::inverse(glm::mat3(1.0)));
  glUniformMatrix3fv(uniform_m_3x3_inv_transp, 1, GL_FALSE, glm::value_ptr(m_3x3_inv_transp));

  glDrawArrays(GL_TRIANGLES, 0, 6*GROUND_SIZE*GROUND_SIZE);


  glDisableVertexAttribArray(attribute_v_normal);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_light_bbox_vertices);
  glVertexAttribPointer(
    attribute_v_coord,  // attribute
    4,                  // number of elements per vertex, here (x,y,z,w)
    GL_FLOAT,           // the type of each element
    GL_FALSE,           // take our values as-is
    0,                  // no extra data between each position
    0                   // offset of first element
  );

  glUniformMatrix4fv(uniform_m, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0)));
  m_3x3_inv_transp = glm::transpose(glm::inverse(glm::mat3(1.0)));
  glUniformMatrix3fv(uniform_m_3x3_inv_transp, 1, GL_FALSE, glm::value_ptr(m_3x3_inv_transp));

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_light_bbox_elements);
  glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_SHORT, 0);
  glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_SHORT, (GLvoid*)(4*sizeof(GLushort)));
  glDrawElements(GL_LINES, 8, GL_UNSIGNED_SHORT, (GLvoid*)(8*sizeof(GLushort)));


  glDisableVertexAttribArray(attribute_v_coord);
  glutSwapBuffers();
}

void onMouse(int button, int state, int x, int y) {
  if (button == GLUT_LEFT_BUTTON) {
    last_mx = cur_mx = x;
    last_my = cur_my = y;
  }
}

void onMotion(int x, int y) {
  cur_mx = x;
  cur_my = y;
}

void onReshape(int width, int height) {
  screen_width = width;
  screen_height = height;
  glViewport(0, 0, screen_width, screen_height);
}

void free_resources()
{
  glDeleteProgram(program);
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
