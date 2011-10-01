#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
/* Using the GLUT library for the base windowing setup */
#include <GL/glut.h>

#include "res_texture.c"

GLuint program;
GLint attribute_coord2d;
GLint uniform_offset_x;
GLint uniform_scale_x;
GLint uniform_sprite;
GLuint texture_id;
GLint uniform_mytexture;

float offset_x = 0.0;
float scale_x = 1.0;
int mode = 0;

struct point {
  GLfloat x;
  GLfloat y;
};

GLuint vbo;

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

int init_resources()
{
  GLint link_ok = GL_FALSE;

  GLuint vs, fs;
  if ((vs = create_shader("graph.v.glsl", GL_VERTEX_SHADER))   == 0) return 0;
  if ((fs = create_shader("graph.f.glsl", GL_FRAGMENT_SHADER)) == 0) return 0;

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
  attribute_name = "coord2d";
  attribute_coord2d = glGetAttribLocation(program, attribute_name);
  if (attribute_coord2d == -1) {
    fprintf(stderr, "Could not bind attribute %s\n", attribute_name);
    return 0;
  }

  const char* uniform_name;
  uniform_name = "offset_x";
  uniform_offset_x = glGetUniformLocation(program, uniform_name);
  if (uniform_offset_x == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }

  uniform_name = "scale_x";
  uniform_scale_x = glGetUniformLocation(program, uniform_name);
  if (uniform_scale_x == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }

  uniform_name = "sprite";
  uniform_sprite = glGetUniformLocation(program, uniform_name);
  if (uniform_sprite == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }

  /* Enable blending */
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  /* Enable point sprites (not necessary for true OpenGL ES 2.0) */
  glEnable(GL_POINT_SPRITE);
  glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

  /* Upload the texture for our point sprites */
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D,      // target
	       0,                  // level, 0 = base, no minimap,
	       GL_RGBA,            // internalformat
	       res_texture.width,  // width
	       res_texture.height, // height
	       0,                  // border, always 0 in OpenGL ES
	       GL_RGBA,            // format
	       GL_UNSIGNED_BYTE,   // type
	       res_texture.pixel_data);

  // Create the vertex buffer object
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  // Create our own temporary buffer
  point graph[2000];

  // Fill it in just like an array
  for(int i = 0; i < 2000; i++) {
    float x = (i - 1000.0) / 100.0;
    graph[i].x = x;
    graph[i].y = sin(x * 10.0) / (1.0 + x * x);
  }

  // Tell OpenGL to copy our array to the buffer object
  glBufferData(GL_ARRAY_BUFFER, sizeof graph, graph, GL_STATIC_DRAW);

  return 1;
}

void display()
{
  glUseProgram(program);
  glUniform1i(uniform_mytexture, 0);

  glUniform1f(uniform_offset_x, offset_x);
  glUniform1f(uniform_scale_x, scale_x);

  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  /* Draw using the vertices in our vertex buffer object */
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
 
  glEnableVertexAttribArray(attribute_coord2d);
  glVertexAttribPointer(
    attribute_coord2d,   // attribute
    2,                   // number of elements per vertex, here (x,y)
    GL_FLOAT,            // the type of each element
    GL_FALSE,            // take our values as-is
    0,                   // no space between values
    0                    // use the vertex buffer object
  );

  /* Push each element in buffer_vertices to the vertex shader */
  switch(mode) {
    case 0:
      glUniform1f(uniform_sprite, 0);
      glDrawArrays(GL_LINE_STRIP, 0, 2000);
      break;
    case 1:
      glUniform1f(uniform_sprite, 1);
      glDrawArrays(GL_POINTS, 0, 2000);
      break;
    case 2:
      glUniform1f(uniform_sprite, res_texture.width);
      glDrawArrays(GL_POINTS, 0, 2000);
      break;
  }

  /* Stop using the vertex buffer object */
  glDisableVertexAttribArray(attribute_coord2d);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glutSwapBuffers();
}

void special(int key, int x, int y)
{
	switch(key) {
		case GLUT_KEY_F1:
			mode = 0;
			break;
		case GLUT_KEY_F2:
			mode = 1;
			break;
		case GLUT_KEY_F3:
			mode = 2;
			break;
		case GLUT_KEY_LEFT:
			offset_x -= 0.1;
			break;
		case GLUT_KEY_RIGHT:
			offset_x += 0.1;
			break;
		case GLUT_KEY_UP:
			scale_x *= 1.5;
			break;
		case GLUT_KEY_DOWN:
			scale_x /= 1.5;
			break;
		case GLUT_KEY_HOME:
			offset_x = 0.0;
			scale_x = 1.0;
			break;
	}

	glutPostRedisplay();
}

void free_resources()
{
  glDeleteProgram(program);
}

int main(int argc, char* argv[]) {
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA|GLUT_ALPHA|GLUT_DOUBLE|GLUT_DEPTH);
  glutInitWindowSize(640, 480);
  glutCreateWindow("My Graph");

  if (init_resources()) {
    glutDisplayFunc(display);
    glutSpecialFunc(special);
    glutMainLoop();
  }

  free_resources();
  return 0;
}
