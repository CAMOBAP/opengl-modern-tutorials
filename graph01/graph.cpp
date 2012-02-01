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
#ifndef GL_ES_VERSION_2_0
  glEnable(GL_POINT_SPRITE);
  glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif

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

  GLfloat range[2];
  glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
  if(range[1] < res_texture.width)
	  fprintf(stderr, "WARNING: point sprite range (%f, %f) too small\n", range[0], range[1]);

  if (init_resources()) {
    glutDisplayFunc(display);
    glutSpecialFunc(special);
    glutMainLoop();
  }

  free_resources();
  return 0;
}
