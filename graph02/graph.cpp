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
#include "../common/shader_utils.h"

GLuint program;
GLint attribute_coord1d;
GLint uniform_offset_x;
GLint uniform_scale_x;
GLuint texture_id;
GLint uniform_mytexture;

float offset_x = 0.0;
float scale_x = 1.0;

bool interpolate = false;
bool clamp = false;
bool showpoints = false;

GLuint vbo;

int init_resources()
{
  int vertex_texture_units;
  glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &vertex_texture_units);
  if(!vertex_texture_units) {
    fprintf(stderr, "Your graphics cards does not support texture lookups in the vertex shader!\n");
    return 0;
  }

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
  attribute_name = "coord1d";
  attribute_coord1d = glGetAttribLocation(program, attribute_name);
  if (attribute_coord1d == -1) {
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

  // Create our datapoints, store it as bytes
  GLbyte graph[2048];

  for(int i = 0; i < 2048; i++) {
    float x = (i - 1024.0) / 100.0;
    float y = sin(x * 10.0) / (1.0 + x * x);
    graph[i] = roundf(y * 128 + 128);
  }

  /* Upload the texture with our datapoints */
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexImage2D(GL_TEXTURE_2D,      // target
	       0,                  // level, 0 = base, no minimap,
	       GL_LUMINANCE,       // internalformat
	       2048,               // width
	       1,                  // height
	       0,                  // border, always 0 in OpenGL ES
	       GL_LUMINANCE,       // format
	       GL_UNSIGNED_BYTE,   // type
	       graph);

  // Create the vertex buffer object
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  // Create an array with only x values.
  GLfloat line[101];

  // Fill it in just like an array
  for(int i = 0; i < 101; i++) {
    line[i] = (i - 50) / 50.0;
  }

  // Tell OpenGL to copy our array to the buffer object
  glBufferData(GL_ARRAY_BUFFER, sizeof line, line, GL_STATIC_DRAW);

  // Enable point size control in vertex shader
#ifndef GL_ES_VERSION_2_0
  glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif

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

  /* Set texture wrapping mode */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);

  /* Set texture interpolation mode */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, interpolate ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, interpolate ? GL_LINEAR : GL_NEAREST);

  /* Draw using the vertices in our vertex buffer object */
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
 
  glEnableVertexAttribArray(attribute_coord1d);
  glVertexAttribPointer(
    attribute_coord1d,   // attribute
    1,                   // number of elements per vertex, here just x
    GL_FLOAT,            // the type of each element
    GL_FALSE,            // take our values as-is
    0,                   // no space between values
    0                    // use the vertex buffer object
  );

  /* Draw the line */
  glDrawArrays(GL_LINE_STRIP, 0, 101);

  /* Draw points as well, if requested */
  if(showpoints)
    glDrawArrays(GL_POINTS, 0, 101);

  /* Stop using the vertex buffer object */
  glDisableVertexAttribArray(attribute_coord1d);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glutSwapBuffers();
}

void special(int key, int x, int y)
{
	switch(key) {
		case GLUT_KEY_F1:
			interpolate = !interpolate;
			break;
		case GLUT_KEY_F2:
			clamp = !clamp;
			break;
		case GLUT_KEY_F3:
			showpoints = !showpoints;
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

  GLint max_units;
  glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &max_units);
  if(max_units < 1) {
	  fprintf(stderr, "Your GPU does not have any vertex texture image units\n");
	  return 1;
  }

  GLfloat range[2];
  glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
  if(range[1] < 5.0)
	  fprintf(stderr, "WARNING: point sprite range (%f, %f) too small\n", range[0], range[1]);

  if (init_resources()) {
    glutDisplayFunc(display);
    glutSpecialFunc(special);
    glutMainLoop();
  }

  free_resources();
  return 0;
}
