#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef NOGLEW
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GL/glew.h>
#endif
#include <GL/glut.h>
/* Using GLM for our transformation matrix */
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

GLuint program;
GLint attribute_coord2d;
GLint uniform_color;
GLint uniform_transform;

float offset_x = 0;
float scale_x = 1;

struct point {
  GLfloat x;
  GLfloat y;
};

GLuint vbo[3];

const int border = 10;
const int ticksize = 10;

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
  uniform_name = "transform";
  uniform_transform = glGetUniformLocation(program, uniform_name);
  if (uniform_transform == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }

  uniform_name = "color";
  uniform_color = glGetUniformLocation(program, uniform_name);
  if (uniform_color == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }

  // Create the vertex buffer object
  glGenBuffers(3, vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);

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

  // Create a VBO for the border
  static const point border[4] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
  glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
  glBufferData(GL_ARRAY_BUFFER, sizeof border, border, GL_STATIC_DRAW);

  return 1;
}

// Create a projection matrix that has the same effect as glViewport().
// Optionally return scaling factors to easily convert normalized device coordinates to pixels.
//
glm::mat4 viewport_transform(float x, float y, float width, float height, float *pixel_x = 0, float *pixel_y = 0) {
  // Map OpenGL coordinates (-1,-1) to window coordinates (x,y),
  // (1,1) to (x + width, y + height).

  // First, we need to know the real window size:
  float window_width = glutGet(GLUT_WINDOW_WIDTH);
  float window_height = glutGet(GLUT_WINDOW_HEIGHT);

  // Calculate how to translate the x and y coordinates:
  float offset_x = (2.0 * x + (width - window_width)) / window_width;
  float offset_y = (2.0 * y + (height - window_height)) / window_height;

  // Calculate how to rescale the x and y coordinates:
  float scale_x = width / window_width;
  float scale_y = height / window_height;

  // Calculate size of pixels in OpenGL coordinates
  if(pixel_x)
    *pixel_x = 2.0 / width;
  if(pixel_y)
    *pixel_y = 2.0 / height;

  return glm::scale(glm::translate(glm::mat4(1), glm::vec3(offset_x, offset_y, 0)), glm::vec3(scale_x, scale_y, 1));
}

void display()
{
  int window_width = glutGet(GLUT_WINDOW_WIDTH);
  int window_height = glutGet(GLUT_WINDOW_HEIGHT);

  glUseProgram(program);

  glClearColor(1, 1, 1, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  /* ----------------------------------------------------------------*/
  /* Draw the graph */

  // Set our viewport, this will clip geometry
  glViewport(
    border + ticksize,
    border + ticksize,
    window_width - border * 2 - ticksize,
    window_height - border * 2 - ticksize
  );

  // Set the scissor rectangle,this will clip fragments
  glScissor(
    border + ticksize,
    border + ticksize,
    window_width - border * 2 - ticksize,
    window_height - border * 2 - ticksize
  );

  glEnable(GL_SCISSOR_TEST);

  // Set our coordinate transformation matrix
  glm::mat4 transform = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(scale_x, 1, 1)), glm::vec3(offset_x, 0, 0));
  glUniformMatrix4fv(uniform_transform, 1, GL_FALSE, glm::value_ptr(transform));

  // Set the color to red
  GLfloat red[4] = {1, 0, 0, 1};
  glUniform4fv(uniform_color, 1, red);

  // Draw using the vertices in our vertex buffer object
  glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
 
  glEnableVertexAttribArray(attribute_coord2d);
  glVertexAttribPointer(attribute_coord2d, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glDrawArrays(GL_LINE_STRIP, 0, 2000);

  // Stop clipping
  glViewport(0, 0, window_width, window_height);
  glDisable(GL_SCISSOR_TEST);

  /* ----------------------------------------------------------------*/
  /* Draw the borders */

  float pixel_x, pixel_y;

  // Calculate a transformation matrix that gives us the same normalized device coordinates as above
  transform = viewport_transform(
    border + ticksize,
    border + ticksize,
    window_width - border * 2 - ticksize,
    window_height - border * 2 - ticksize,
    &pixel_x, &pixel_y
  );

  // Tell our vertex shader about it
  glUniformMatrix4fv(uniform_transform, 1, GL_FALSE, glm::value_ptr(transform));

  // Set the color to black
  GLfloat black[4] = {0, 0, 0, 1};
  glUniform4fv(uniform_color, 1, black);

  // Draw a border around our graph
  glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
  glVertexAttribPointer(attribute_coord2d, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glDrawArrays(GL_LINE_LOOP, 0, 4);

  /* ----------------------------------------------------------------*/
  /* Draw the y tick marks */

  point ticks[42];

  for(int i = 0; i <= 20; i++) {
	  float y = -1 + i * 0.1;
	  float tickscale = (i % 10) ? 0.5 : 1; 
	  ticks[i * 2].x = -1;
	  ticks[i * 2].y = y; 
	  ticks[i * 2 + 1].x = -1 - ticksize * tickscale * pixel_x;
	  ticks[i * 2 + 1].y = y; 
  }

  glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
  glBufferData(GL_ARRAY_BUFFER, sizeof ticks, ticks, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(attribute_coord2d, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glDrawArrays(GL_LINES, 0, 42);

  /* ----------------------------------------------------------------*/
  /* Draw the x tick marks */
  
  float tickspacing = 0.1 * powf(10, -floor(log10(scale_x))); // desired space between ticks, in graph coordinates
  float left = -1.0 / scale_x - offset_x;                     // left edge, in graph coordinates
  float right = 1.0 / scale_x - offset_x;                     // right edge, in graph coordinates
  int left_i = ceil(left / tickspacing);                      // index of left tick, counted from the origin
  int right_i = floor(right / tickspacing);                   // index of right tick, counted from the origin
  float rem = left_i * tickspacing - left;                    // space between left edge of graph and the first tick

  float firsttick = -1.0 + rem * scale_x;                     // first tick in device coordinates

  int nticks = right_i - left_i + 1;                          // number of ticks to show
  if(nticks > 21)
	  nticks = 21; // should not happen

  for(int i = 0; i < nticks; i++) {
	  float x = firsttick + i * tickspacing * scale_x;
	  float tickscale = ((i + left_i) % 10) ? 0.5 : 1; 
	  ticks[i * 2].x = x; 
	  ticks[i * 2].y = -1;
	  ticks[i * 2 + 1].x = x; 
	  ticks[i * 2 + 1].y = -1 - ticksize * tickscale * pixel_y;
  }

  glBufferData(GL_ARRAY_BUFFER, sizeof ticks, ticks, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(attribute_coord2d, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glDrawArrays(GL_LINES, 0, nticks * 2);

  // And we are done.

  glDisableVertexAttribArray(attribute_coord2d);
  glutSwapBuffers();
}

void special(int key, int x, int y)
{
	switch(key) {
		case GLUT_KEY_LEFT:
			offset_x -= 0.03;
			break;
		case GLUT_KEY_RIGHT:
			offset_x += 0.03;
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

#ifndef NOGLEW
  GLenum glew_status = glewInit();
  if (GLEW_OK != glew_status) {
    fprintf(stderr, "Error: %s\n", glewGetErrorString(glew_status));
    return 1;
  }

  if (!GLEW_VERSION_2_0) {
    fprintf(stderr, "No support for OpenGL 2.0 found\n");
    return 1;
  }
#endif

  if (init_resources()) {
    glutDisplayFunc(display);
    glutSpecialFunc(special);
    glutMainLoop();
  }

  free_resources();
  return 0;
}
