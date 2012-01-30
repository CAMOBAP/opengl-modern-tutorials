/**
 * This file is in the public domain.
 * Contributors: Sylvain Beucler, Guus Sliepen
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
/* Use glew.h instead of gl.h to get all the GL prototypes declared */
#ifdef NOGLEW
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GL/glew.h>
#endif
/* Using the GLUT library for the base windowing setup */
#include <GL/glut.h>
/* GLM */
// #define GLM_MESSAGES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include "../common/shader_utils.h"

#include "res_texture.c"

int screen_width=800, screen_height=600;
GLuint vbo_cube_vertices, vbo_cube_texcoords;
GLuint ibo_cube_elements;
GLuint program;
GLuint texture_id;
GLint attribute_coord3d, attribute_texcoord;
GLint uniform_mvp, uniform_mytexture, uniform_color;

#define NCUBES 10
glm::vec3 positions[3 * NCUBES];
glm::vec3 rotspeeds[3 * NCUBES];
bool highlight[NCUBES];

float angle = 0;
float camera_angle = 0;

int init_resources() {
	GLfloat cube_vertices[] = {
		// front
		-1.0, -1.0,	1.0,
		 1.0, -1.0,	1.0,
		 1.0,	1.0,	1.0,
		-1.0,	1.0,	1.0,
		// top
		-1.0,	1.0,	1.0,
		 1.0,	1.0,	1.0,
		 1.0,	1.0, -1.0,
		-1.0,	1.0, -1.0,
		// back
		 1.0, -1.0, -1.0,
		-1.0, -1.0, -1.0,
		-1.0,	1.0, -1.0,
		 1.0,	1.0, -1.0,
		// bottom
		-1.0, -1.0, -1.0,
		 1.0, -1.0, -1.0,
		 1.0, -1.0,	1.0,
		-1.0, -1.0,	1.0,
		// left
		-1.0, -1.0, -1.0,
		-1.0, -1.0,	1.0,
		-1.0,	1.0,	1.0,
		-1.0,	1.0, -1.0,
		// right
		 1.0, -1.0,	1.0,
		 1.0, -1.0, -1.0,
		 1.0,	1.0, -1.0,
		 1.0,	1.0,	1.0,
	};
	glGenBuffers(1, &vbo_cube_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_cube_vertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

	GLfloat cube_texcoords[2*4*6] = {
		// front
		0.0, 0.0,
		1.0, 0.0,
		1.0, 1.0,
		0.0, 1.0,
	};
	for (int i = 1; i < 6; i++)
		memcpy(&cube_texcoords[i*4*2], &cube_texcoords[0], 2*4*sizeof(GLfloat));
	glGenBuffers(1, &vbo_cube_texcoords);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_cube_texcoords);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_texcoords), cube_texcoords, GL_STATIC_DRAW);

	GLushort cube_elements[] = {
		// front
		0,	1,	2,
		2,	3,	0,
		// top
		4,	5,	6,
		6,	7,	4,
		// back
		8,	9, 10,
		10, 11,	8,
		// bottom
		12, 13, 14,
		14, 15, 12,
		// left
		16, 17, 18,
		18, 19, 16,
		// right
		20, 21, 22,
		22, 23, 20,
	};
	glGenBuffers(1, &ibo_cube_elements);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_cube_elements);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_elements), cube_elements, GL_STATIC_DRAW);

	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, // target
				 0,	// level, 0 = base, no minimap,
				 GL_RGB, // internalformat
				 res_texture.width,	// width
				 res_texture.height,	// height
				 0,	// border, always 0 in OpenGL ES
				 GL_RGB,	// format
				 GL_UNSIGNED_BYTE, // type
				 res_texture.pixel_data);

	GLint link_ok = GL_FALSE;

	GLuint vs, fs;
	if ((vs = create_shader("cube.v.glsl", GL_VERTEX_SHADER))	 == 0) return 0;
	if ((fs = create_shader("cube.f.glsl", GL_FRAGMENT_SHADER)) == 0) return 0;

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
	attribute_name = "texcoord";
	attribute_texcoord = glGetAttribLocation(program, attribute_name);
	if (attribute_texcoord == -1) {
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
	uniform_name = "color";
	uniform_color = glGetUniformLocation(program, uniform_name);
	if (uniform_color == -1) {
		fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
		return 0;
	}

	srand48(time(NULL));

	for(int i = 0; i < NCUBES; i++) {
		positions[i] = glm::vec3((drand48() - 0.5) * 2, (drand48() - 0.5) * 2, (drand48() - 0.5) * 2);
		rotspeeds[i] = glm::vec3(drand48() * 5, drand48() * 5, drand48() * 5);
	}

	return 1;
}

void onIdle() {
	angle = glutGet(GLUT_ELAPSED_TIME) / 1000.0 * 15;	// base 15° per second
	glutPostRedisplay();
}

void onDisplay() {
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

	glUseProgram(program);
	glUniform1i(uniform_mytexture, /*GL_TEXTURE*/0);
	glEnableVertexAttribArray(attribute_coord3d);
	// Describe our vertices array to OpenGL (it can't guess its format automatically)
	glBindBuffer(GL_ARRAY_BUFFER, vbo_cube_vertices);
	glVertexAttribPointer(
		attribute_coord3d, // attribute
		3,								 // number of elements per vertex, here (x,y,z)
		GL_FLOAT,					// the type of each element
		GL_FALSE,					// take our values as-is
		0,								 // no extra data between each position
		0									// offset of first element
	);

	glEnableVertexAttribArray(attribute_texcoord);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_cube_texcoords);
	glVertexAttribPointer(
		attribute_texcoord, // attribute
		2,									// number of elements per vertex, here (x,y)
		GL_FLOAT,					 // the type of each element
		GL_FALSE,					 // take our values as-is
		0,									// no extra data between each position
		0									 // offset of first element
	);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_cube_elements);
	int size;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);

	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	glm::mat4 view = glm::lookAt(glm::rotateY(glm::vec3(0.0, 2.0, 4.0), camera_angle), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0));
	glm::mat4 projection = glm::perspective(45.0f, 1.0f*screen_width/screen_height, 0.1f, 10.0f);

	GLfloat color_normal[4] = {1, 1, 1, 1};
	GLfloat color_highlight[4] = {2, 2, 2, 1};

	for(int i = 0; i < NCUBES; i++) {
		glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), positions[i]), glm::vec3(0.2, 0.2, 0.2));

		glm::mat4 anim = \
		glm::rotate(glm::mat4(1.0f), angle * rotspeeds[i].x, glm::vec3(1, 0, 0)) *	// X axis
		glm::rotate(glm::mat4(1.0f), angle * rotspeeds[i].y, glm::vec3(0, 1, 0)) *	// Y axis
		glm::rotate(glm::mat4(1.0f), angle * rotspeeds[i].z, glm::vec3(0, 0, 1));	 // Z axis

		glm::mat4 mvp = projection * view * model * anim;
		glUniformMatrix4fv(uniform_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

		if(highlight[i])
			glUniform4fv(uniform_color, 1, color_highlight);
		else
			glUniform4fv(uniform_color, 1, color_normal);

		glStencilFunc(GL_ALWAYS, i + 1, -1);

		/* Push each element in buffer_vertices to the vertex shader */
		glDrawElements(GL_TRIANGLES, size/sizeof(GLushort), GL_UNSIGNED_SHORT, 0);
	}

	glDisableVertexAttribArray(attribute_coord3d);
	glDisableVertexAttribArray(attribute_texcoord);
	glutSwapBuffers();
}

void onReshape(int width, int height) {
	screen_width = width;
	screen_height = height;
	glViewport(0, 0, screen_width, screen_height);
}

void onSpecial(int key, int x, int y) {
	switch(key) {
		case GLUT_KEY_LEFT:
			camera_angle -= 5;
			break;
		case GLUT_KEY_RIGHT:
			camera_angle += 5;
			break;
	}
}

void onMouse(int button, int state, int x, int y) {
	if(state != GLUT_DOWN)
		return;

	/* Read color, depth and stencil index from the framebuffer */
	GLbyte color[4];
	GLfloat depth;
	GLuint index;
	
	glReadPixels(x, screen_height - y - 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color);
	glReadPixels(x, screen_height - y - 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
	glReadPixels(x, screen_height - y - 1, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_INT, &index);

	printf("Clicked on pixel %d, %d, color %02hhx%02hhx%02hhx%02hhx, depth %f, stencil index %u\n",
			x, y, color[0], color[1], color[2], color[3], depth, index);

	/* Convert from window coordinates to object coordinates */
	glm::mat4 view = glm::lookAt(glm::rotateY(glm::vec3(0.0, 2.0, 4.0), camera_angle), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0));
	glm::mat4 projection = glm::perspective(45.0f, 1.0f*screen_width/screen_height, 0.1f, 10.0f);
	glm::vec4 viewport = glm::vec4(0, 0, screen_width, screen_height);

	glm::vec3 wincoord = glm::vec3(x, screen_height - y - 1, depth);
	glm::vec3 objcoord = glm::unProject(wincoord, view, projection, viewport);

	/* Which box is nearest to those object coordinates? */
	int closest_i = 0;

	for(int i = 1; i < NCUBES; i++) {
		if(glm::distance(objcoord, positions[i]) < glm::distance(objcoord, positions[closest_i]))
			closest_i = i;
	}

	printf("Coordinates in object space: %f, %f, %f, closest to center of box %d\n",
			objcoord.x, objcoord.y, objcoord.z, closest_i + 1);

	/* Toggle highlighting of the selected object */
	if(index)
		highlight[index - 1] = !highlight[index - 1];
}

void free_resources() {
	glDeleteProgram(program);
	glDeleteBuffers(1, &vbo_cube_vertices);
	glDeleteBuffers(1, &vbo_cube_texcoords);
	glDeleteBuffers(1, &ibo_cube_elements);
	glDeleteTextures(1, &texture_id);
}


int main(int argc, char* argv[]) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA|GLUT_ALPHA|GLUT_DOUBLE|GLUT_DEPTH|GLUT_STENCIL);
	glutInitWindowSize(screen_width, screen_height);
	glutCreateWindow("Object selection");

#ifndef NOGLEW
	GLenum glew_status = glewInit();
	if (glew_status != GLEW_OK) {
		fprintf(stderr, "Error: %s\n", glewGetErrorString(glew_status));
		return 1;
	}

	if (!GLEW_VERSION_2_0) {
		fprintf(stderr, "Error: your graphic card does not support OpenGL 2.0\n");
		return 1;
	}
#endif

	if (init_resources()) {
		glutDisplayFunc(onDisplay);
		glutReshapeFunc(onReshape);
		glutIdleFunc(onIdle);
		glutMouseFunc(onMouse);
		glutSpecialFunc(onSpecial);
		glEnable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_STENCIL_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glutMainLoop();
	}

	free_resources();
	return 0;
}
