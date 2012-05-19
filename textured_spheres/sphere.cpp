/**
 * From the OpenGL Programming wikibook: http://en.wikibooks.org/wiki/OpenGL_Programming
 * This file is in the public domain.
 * Contributors: Sylvain Beucler
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
/* Use glew.h instead of gl.h to get all the GL prototypes declared */
#include <GL/glew.h>
/* Using the GLUT library for the base windowing setup */
#include <GL/freeglut.h>
/* GLM */
// #define GLM_MESSAGES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <SOIL/SOIL.h>
#include "../common/shader_utils.h"

#include <iostream>
using namespace std;

int screen_width=800, screen_height=600;
GLuint vbo_sphere_vertices, vbo_sphere_texcoords;
GLuint ibo_sphere_elements;
GLuint program;
GLuint texture_id;
GLint attribute_coord3d, attribute_texcoord;
GLint uniform_mvp, uniform_mytexture, uniform_mytexture_ST;

char* vshader_filename = (char*) "sphere.v.glsl";
char* fshader_filename = (char*) "sphere.f.glsl";

int init_resources()
{
  glActiveTexture(GL_TEXTURE0);
  GLuint texture_id = SOIL_load_OGL_texture
    (
     "Earthmap720x360_grid.jpg",
     SOIL_LOAD_AUTO,
     SOIL_CREATE_NEW_ID,
     SOIL_FLAG_INVERT_Y | SOIL_FLAG_TEXTURE_REPEATS
     );
  if(texture_id == 0)
    cerr << "SOIL loading error: '" << SOIL_last_result() << "' (" << "Earthmap720x360_grid.jpg" << ")" << endl;

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
  attribute_name = "coord3d";
  attribute_coord3d = glGetAttribLocation(program, attribute_name);
  if (attribute_coord3d == -1) {
    fprintf(stderr, "Could not bind attribute %s\n", attribute_name);
    return 0;
  }
  const char* uniform_name;
  uniform_name = "mvp";
  uniform_mvp = glGetUniformLocation(program, uniform_name);
  if (uniform_mvp == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }
  uniform_name = "mytexture";
  uniform_mytexture = glGetUniformLocation(program, uniform_name);
  if (uniform_mytexture == -1) {
    fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
    return 0;
  }
  if (strstr(fshader_filename, "_ST")) {
      uniform_name = "mytexture_ST";
      uniform_mytexture_ST = glGetUniformLocation(program, uniform_name);
      if (uniform_mytexture_ST == -1) {
          fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
          return 0;
      }
  }

  return 1;
}

void logic() {
  float angle = glutGet(GLUT_ELAPSED_TIME) / 1000.0 * 30;  // 30° per second
  glm::mat4 anim = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1, 0));

  glm::mat4 fix_orientation = glm::rotate(glm::mat4(1.0f), -90.f, glm::vec3(1, 0, 0));
  glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0, 0.0, -2.0));
  glm::mat4 view = glm::lookAt(glm::vec3(0.0, 2.0, 0.0), glm::vec3(0.0, 0.0, -2.0), glm::vec3(0.0, 1.0, 0.0));
  glm::mat4 projection = glm::perspective(45.0f, 1.0f*screen_width/screen_height, 0.1f, 10.0f);

  glm::mat4 mvp = projection * view * model * anim * fix_orientation;
  glUseProgram(program);
  glUniformMatrix4fv(uniform_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

  // tiling and offset
  glUniform4f(uniform_mytexture_ST, 2,1, 0,-.05);

  glutPostRedisplay();
}

void draw()
{
  glClearColor(1.0, 1.0, 1.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glUseProgram(program);
  glUniform1i(uniform_mytexture, /*GL_TEXTURE*/0);

  glutSetVertexAttribCoord3(attribute_coord3d);
  glutSolidSphere(1.0,30,30);
}

void onDisplay() {
  logic();
  draw();
  glutSwapBuffers();
}

void onReshape(int width, int height) {
  screen_width = width;
  screen_height = height;
  glViewport(0, 0, screen_width, screen_height);
}

void free_resources()
{
  glDeleteProgram(program);
  glDeleteBuffers(1, &vbo_sphere_vertices);
  glDeleteBuffers(1, &vbo_sphere_texcoords);
  glDeleteBuffers(1, &ibo_sphere_elements);
  glDeleteTextures(1, &texture_id);
}


int main(int argc, char* argv[]) {
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA|GLUT_ALPHA|GLUT_DOUBLE|GLUT_DEPTH);
  glutInitWindowSize(screen_width, screen_height);
  glutCreateWindow("Textured Spheres");

  if (argc != 3) {
    fprintf(stderr, "Usage: %s vertex_shader.v.glsl fragment_shader.f.glsl\n", argv[0]);
  } else {
    vshader_filename = argv[1];
    fshader_filename = argv[2];
  }

  GLenum glew_status = glewInit();
  if (glew_status != GLEW_OK) {
    fprintf(stderr, "Error: %s\n", glewGetErrorString(glew_status));
    return 1;
  }

  if (!GLEW_VERSION_2_0) {
    fprintf(stderr, "Error: your graphic card does not support OpenGL 2.0\n");
    return 1;
  }

  if (init_resources()) {
    glutDisplayFunc(onDisplay);
    glutReshapeFunc(onReshape);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glutMainLoop();
  }

  free_resources();
  return 0;
}
