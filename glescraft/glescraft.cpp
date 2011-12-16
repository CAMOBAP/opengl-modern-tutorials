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
/* Using GLM for our transformation matrices */
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <time.h>

#include "noise.cpp"
#include "textures.c"

GLuint program;
GLint attribute_coord;
GLint uniform_mvp;
GLuint texture;
GLint uniform_texture;
GLuint ground_vbo;
GLuint cursor_vbo;

glm::vec3 position;
glm::vec3 forward;
glm::vec3 right;
glm::vec3 lookat;
glm::vec3 angle;

int ww, wh;
int mx, my, mz;
int face;
uint8_t buildtype = 1;

static time_t now;
static unsigned int keys;

// Size of one chunk in blocks
#define CX 16
#define CY 32
#define CZ 16

// Number of chunks in the world
#define SCX 32
#define SCY 2
#define SCZ 32

// Sea level
#define SEALEVEL 4

// Number of VBO slots for chunks
#define CHUNKSLOTS (SCX * SCY * SCZ)

static const int transparent[16] = {2, 0, 0, 0, 1, 0, 0, 0, 3, 4, 0, 0, 0, 0, 0, 0}; 
static const char *blocknames[16] = {
	"air", "dirt", "topsoil", "grass", "leaves", "wood", "stone", "sand",
	"water", "glass", "brick", "ore", "woodrings", "white", "black", "x-y"
};

typedef glm::detail::tvec4<GLbyte> byte4;

static struct chunk *chunk_slot[CHUNKSLOTS] = {0};

struct chunk {
	uint8_t blk[CX][CY][CZ];
	struct chunk *left, *right, *below, *above, *front, *back;
	int slot;
	GLuint vbo;
	int elements;
	time_t lastused;
	bool changed;
	bool perlinated;
	bool initialized;
	int ax;
	int ay;
	int az;

	chunk(): ax(0), ay(0), az(0) {
		memset(blk, 0, sizeof blk);
		left = right = below = above = front = back = 0;
		lastused = now;
		slot = 0;
		changed = true;
		initialized = false;
		perlinated = false;
	}

	chunk(int x, int y, int z): ax(x), ay(y), az(z) {
		memset(blk, 0, sizeof blk);
		left = right = below = above = front = back = 0;
		lastused = now;
		slot = 0;
		changed = true;
		initialized = false;
		perlinated = false;
	}

	uint8_t get(int x, int y, int z) const {
		if(x < 0)
			return left ? left->blk[x + CX][y][z] : 0;
		if(x >= CX)
			return right ? right->blk[x - CX][y][z] : 0;
		if(y < 0)
			return below ? below->blk[x][y + CY][z] : 0;
		if(y >= CY)
			return above ? above->blk[x][y - CY][z] : 0;
		if(z < 0)
			return front ? front->blk[x][y][z + CZ] : 0;
		if(z >= CZ)
			return back ? back->blk[x][y][z - CZ] : 0;
		return blk[x][y][z];
	}

	bool isblocked(int x1, int y1, int z1, int x2, int y2, int z2) {
		// Invisible blocks are always "blocked"
		if(!blk[x1][y1][z1])
			return true;

		// Leaves do not block any other block, including themselves
		if(transparent[get(x2, y2, z2)] == 1)
			return false;

		// Non-transparent blocks always block line of sight
		if(!transparent[get(x2, y2, z2)])
			return true;

		// Otherwise, LOS is only blocked by blocks if the same transparency type
		return transparent[get(x2, y2, z2)] == transparent[blk[x1][y1][z1]];
	}

	void set(int x, int y, int z, uint8_t type) {
		// If coordinates are outside this chunk, find the right one.
		if(x < 0) {
			if(left)
				left->set(x + CX, y, z, type);
			return;
		}
		if(x >= CX) {
			if(right)
				right->set(x - CX, y, z, type);
			return;
		}
		if(y < 0) {
			if(below)
				below->set(x, y + CY, z, type);
			return;
		}
		if(y >= CY) {
			if(above)
				above->set(x, y - CY, z, type);
			return;
		}
		if(z < 0) {
			if(front)
				front->set(x, y, z + CZ, type);
			return;
		}
		if(z >= CZ) {
			if(back)
				back->set(x, y, z - CZ, type);
			return;
		}

		// Change the block
		blk[x][y][z] = type;
		changed = true;

		// When updating blocks at the edge of this chunk,
		// visibility of blocks in the neighbouring chunk might change.
		if(x == 0 && left)
			left->changed = true;
		if(x == CX - 1 && right)
			right->changed = true;
		if(y == 0 && below)
			below->changed = true;
		if(y == CY - 1 && above)
			above->changed = true;
		if(z == 0 && front)
			front->changed = true;
		if(z == CZ - 1 && back)
			back->changed = true;
	}

	void perlin(int seed) {
		if(perlinated)
			return;
		else
			perlinated = true;

		for(int x = 0; x < CX; x++) {
			for(int z = 0; z < CZ; z++) {
				// Land height
				float n = noise2d_perlin((x + ax * CX) / 256.0, (z + az * CZ) / 256.0, seed, 4, 2.38);
				int h = n * 2;
				int y = 0;

				// Land blocks
				for(y = 0; y < CY; y++) {
					// Are we above "ground" level?
					if(y + ay * CY >= h) {
						// If we are not yet up to sea level, fill with water blocks
						if(y + ay * CY < SEALEVEL) {
							blk[x][y][z] = 8;
							continue;
						// Otherwise, we are in the air
						} else {
							// A tree!
							if(get(x, y - 1, z) == 3 && (rand() & 0xff) == 0) {
								// Trunk
								h = (rand() & 0x3) + 3;
								for(int i = 0; i < h; i++)
									set(x, y + i, z, 5);

								// Leaves
								for(int ix = -3; ix <= 3; ix++) { 
									for(int iy = -3; iy <= 3; iy++) { 
										for(int iz = -3; iz <= 3; iz++) { 
											if(ix * ix + iy * iy + iz * iz < 8 + (rand() & 1) && !get(x + ix, y + h + iy, z + iz))
												set(x + ix, y + h + iy, z + iz, 4);
										}
									}
								}
							}
							break;
						}
					}

					// Random value used to determine land type
					float r = noise3d_perlin_abs((x + ax * CX) / 16.0, (y + ay * CY) / 16.0, (z + az * CZ) / 16.0, -seed, 2, 1);

					// Sand layer
					if(n + r * 5 < 4)
						blk[x][y][z] = 7;
					// Dirt layer, but use grass blocks for the top
					else if(n + r * 5 < 8)
						blk[x][y][z] = (h < SEALEVEL || y + ay * CY < h - 1) ? 1 : 3;
					// Rock layer
					else if(r < 1.25)
						blk[x][y][z] = 6;
					// Sometimes, ores!
					else
						blk[x][y][z] = 11;
				}
			}
		}
		changed = true;
	}

	void update() {
		byte4 vertex[CX * CY * CZ * 18];
		int i = 0;
		int merged = 0;
		bool vis = false;;

		// View from negative x

		for(int x = CX - 1; x >= 0; x--) {
			for(int y = 0; y < CY; y++) {
				for(int z = 0; z < CZ; z++) {
					// Line of sight blocked?
					if(isblocked(x, y, z, x - 1, y, z)) {
						vis = false;
						continue;
					}

					uint8_t top = blk[x][y][z];
					uint8_t bottom = blk[x][y][z];
					uint8_t side = blk[x][y][z];

					// Grass block has dirt sides and bottom
					if(top == 3) {
						bottom = 1;
						side = 2;
					// Wood blocks have rings on top and bottom
					} else if(top == 5) {
						top = bottom = 12;
					}

					// Same block as previous one? Extend it.
					if(vis && z != 0 && blk[x][y][z] == blk[x][y][z - 1]) {
						vertex[i - 5] = byte4(x, y, z + 1, side);
						vertex[i - 2] = byte4(x, y, z + 1, side);
						vertex[i - 1] = byte4(x, y + 1, z + 1, side);
						merged++;
					// Otherwise, add a new quad.
					} else {
						vertex[i++] = byte4(x, y, z, side);
						vertex[i++] = byte4(x, y, z + 1, side);
						vertex[i++] = byte4(x, y + 1, z, side);
						vertex[i++] = byte4(x, y + 1, z, side);
						vertex[i++] = byte4(x, y, z + 1, side);
						vertex[i++] = byte4(x, y + 1, z + 1, side);
					}
					
					vis = true;
				}
			}
		}

		// View from positive x

		for(int x = 0; x < CX; x++) {
			for(int y = 0; y < CY; y++) {
				for(int z = 0; z < CZ; z++) {
					if(isblocked(x, y, z, x + 1, y, z)) {
						vis = false;
						continue;
					}

					uint8_t top = blk[x][y][z];
					uint8_t bottom = blk[x][y][z];
					uint8_t side = blk[x][y][z];

					if(top == 3) {
						bottom = 1;
						side = 2;
					} else if(top == 5) {
						top = bottom = 12;
					}

					if(vis && z != 0 && blk[x][y][z] == blk[x][y][z - 1]) {
						vertex[i - 4] = byte4(x + 1, y, z + 1, side);
						vertex[i - 2] = byte4(x + 1, y + 1, z + 1, side);
						vertex[i - 1] = byte4(x + 1, y, z + 1, side);
						merged++;
					} else {
						vertex[i++] = byte4(x + 1, y, z, side);
						vertex[i++] = byte4(x + 1, y + 1, z, side);
						vertex[i++] = byte4(x + 1, y, z + 1, side);
						vertex[i++] = byte4(x + 1, y + 1, z, side);
						vertex[i++] = byte4(x + 1, y + 1, z + 1, side);
						vertex[i++] = byte4(x + 1, y, z + 1, side);
					}
					vis = true;
				}
			}
		}

		// View from negative y

		for(int x = 0; x < CX; x++) {
			for(int y = CY - 1; y >= 0; y--) {
				for(int z = 0; z < CZ; z++) {
					if(isblocked(x, y, z, x, y - 1, z)) {
						vis = false;
						continue;
					}

					uint8_t top = blk[x][y][z];
					uint8_t bottom = blk[x][y][z];

					if(top == 3) {
						bottom = 1;
					} else if(top == 5) {
						top = bottom = 12;
					}

					if(vis && z != 0 && blk[x][y][z] == blk[x][y][z - 1]) {
						vertex[i - 4] = byte4(x, y, z + 1, bottom + 128);
						vertex[i - 2] = byte4(x + 1, y, z + 1, bottom + 128);
						vertex[i - 1] = byte4(x, y, z + 1, bottom + 128);
						merged++;
					} else {
						vertex[i++] = byte4(x, y, z, bottom + 128);
						vertex[i++] = byte4(x + 1, y, z, bottom + 128);
						vertex[i++] = byte4(x, y, z + 1, bottom + 128);
						vertex[i++] = byte4(x + 1, y, z, bottom + 128);
						vertex[i++] = byte4(x + 1, y, z + 1, bottom + 128);
						vertex[i++] = byte4(x, y, z + 1, bottom + 128);
					}
					vis = true;
				}
			}
		}

		// View from positive y

		for(int x = 0; x < CX; x++) {
			for(int y = 0; y < CY; y++) {
				for(int z = 0; z < CZ; z++) {
					if(isblocked(x, y, z, x, y + 1, z)) {
						vis = false;
						continue;
					}

					uint8_t top = blk[x][y][z];
					uint8_t bottom = blk[x][y][z];

					if(top == 3) {
						bottom = 1;
					} else if(top == 5) {
						top = bottom = 12;
					}

					if(vis && z != 0 && blk[x][y][z] == blk[x][y][z - 1]) {
						vertex[i - 5] = byte4(x, y + 1, z + 1, top + 128);
						vertex[i - 2] = byte4(x, y + 1, z + 1, top + 128);
						vertex[i - 1] = byte4(x + 1, y + 1, z + 1, top + 128);
						merged++;
					} else {
						vertex[i++] = byte4(x, y + 1, z, top + 128);
						vertex[i++] = byte4(x, y + 1, z + 1, top + 128);
						vertex[i++] = byte4(x + 1, y + 1, z, top + 128);
						vertex[i++] = byte4(x + 1, y + 1, z, top + 128);
						vertex[i++] = byte4(x, y + 1, z + 1, top + 128);
						vertex[i++] = byte4(x + 1, y + 1, z + 1, top + 128);
					}
					vis = true;
				}
			}
		}

		// View from negative z

		for(int x = 0; x < CX; x++) {
			for(int z = CZ - 1; z >= 0; z--) {
				for(int y = 0; y < CY; y++) {
					if(isblocked(x, y, z, x, y, z - 1)) {
						vis = false;
						continue;
					}

					uint8_t top = blk[x][y][z];
					uint8_t bottom = blk[x][y][z];
					uint8_t side = blk[x][y][z];

					if(top == 3) {
						bottom = 1;
						side = 2;
					} else if(top == 5) {
						top = bottom = 12;
					}

					if(vis && y != 0 && blk[x][y][z] == blk[x][y - 1][z]) {
						vertex[i - 5] = byte4(x, y + 1, z, side);
						vertex[i - 3] = byte4(x, y + 1, z, side);
						vertex[i - 2] = byte4(x + 1, y + 1, z, side);
						merged++;
					} else {
						vertex[i++] = byte4(x, y, z, side);
						vertex[i++] = byte4(x, y + 1, z, side);
						vertex[i++] = byte4(x + 1, y, z, side);
						vertex[i++] = byte4(x, y + 1, z, side);
						vertex[i++] = byte4(x + 1, y + 1, z, side);
						vertex[i++] = byte4(x + 1, y, z, side);
					}
					vis = true;
				}
			}
		}

		// View from positive z

		for(int x = 0; x < CX; x++) {
			for(int z = 0; z < CZ; z++) {
				for(int y = 0; y < CY; y++) {
					if(isblocked(x, y, z, x, y, z + 1)) {
						vis = false;
						continue;
					}

					uint8_t top = blk[x][y][z];
					uint8_t bottom = blk[x][y][z];
					uint8_t side = blk[x][y][z];

					if(top == 3) {
						bottom = 1;
						side = 2;
					} else if(top == 5) {
						top = bottom = 12;
					}

					if(vis && y != 0 && blk[x][y][z] == blk[x][y - 1][z]) {
						vertex[i - 4] = byte4(x, y + 1, z + 1, side);
						vertex[i - 3] = byte4(x, y + 1, z + 1, side);
						vertex[i - 1] = byte4(x + 1, y + 1, z + 1, side);
						merged++;
					} else {
						vertex[i++] = byte4(x, y, z + 1, side);
						vertex[i++] = byte4(x + 1, y, z + 1, side);
						vertex[i++] = byte4(x, y + 1, z + 1, side);
						vertex[i++] = byte4(x, y + 1, z + 1, side);
						vertex[i++] = byte4(x + 1, y, z + 1, side);
						vertex[i++] = byte4(x + 1, y + 1, z + 1, side);
					}
					vis = true;
				}
			}
		}

		changed = false;
		elements = i;

		// If this chunk is empty, no need to allocate a chunk slot.
		if(!elements)
			return;

		// If we don't have an active slot, find one
		if(chunk_slot[slot] != this) {
			int lru = 0;
			for(int i = 0; i < CHUNKSLOTS; i++) {
				// If there is an empty slot, use it
				if(!chunk_slot[i]) {
					lru = i;
					break;
				}
				// Otherwise try to find the least recently used slot
				if(chunk_slot[i]->lastused < chunk_slot[lru]->lastused)
					lru = i;
			}

			// If the slot is empty, create a new VBO
			if(!chunk_slot[lru]) {
				glGenBuffers(1, &vbo);
			// Otherwise, steal it from the previous slot owner
			} else {
				vbo = chunk_slot[lru]->vbo;
				chunk_slot[lru]->changed = true;
			}

			slot = lru;
			chunk_slot[slot] = this;
		}

		// Upload vertices

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, i * sizeof *vertex, vertex, GL_STATIC_DRAW);

		//fprintf(stderr, "Updated chunk, %i vertices (%i kb)\n", i, (i * 4) / 1024);
		//fprintf(stderr, "Merged %d faces (%i vertices, %i kb saved)\n", merged, merged * 6, (merged * 24) / 1024);
	}

	void render() {
		if(changed)
			update();

		lastused = now;

		if(!elements)
			return;

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glVertexAttribPointer(attribute_coord, 4, GL_BYTE, GL_FALSE, 0, 0);
		glDrawArrays(GL_TRIANGLES, 0, elements);
	}
};

struct superchunk {
	chunk *c[SCX][SCY][SCZ];
	time_t seed;

	superchunk() {
		seed = time(NULL);
		for(int x = 0; x < SCX; x++)
			for(int y = 0; y < SCY; y++)
				for(int z = 0; z < SCZ; z++)
					c[x][y][z] = new chunk(x - SCX / 2, y - SCY / 2, z - SCZ / 2);

		for(int x = 0; x < SCX; x++)
			for(int y = 0; y < SCY; y++)
				for(int z = 0; z < SCZ; z++) {
					if(x > 0)
						c[x][y][z]->left = c[x - 1][y][z];
					if(x < SCX - 1)
						c[x][y][z]->right = c[x + 1][y][z];
					if(y > 0)
						c[x][y][z]->below = c[x][y - 1][z];
					if(y < SCY - 1)
						c[x][y][z]->above = c[x][y + 1][z];
					if(z > 0)
						c[x][y][z]->front = c[x][y][z - 1];
					if(z < SCZ - 1)
						c[x][y][z]->back = c[x][y][z + 1];
				}
	}

	uint8_t get(int x, int y, int z) const {
		int cx = (x + CX * (SCX / 2)) / CX;
		int cy = (y + CY * (SCY / 2)) / CY;
		int cz = (z + CZ * (SCZ / 2)) / CZ;

		if(cx < 0 || cx >= SCX || cy < 0 || cy >= SCY || cz <= 0 || cz >= SCZ)
			return 0;

		return c[cx][cy][cz]->get(x & (CX - 1), y & (CY - 1), z & (CZ - 1));
	}

	void set(int x, int y, int z, uint8_t type) {
		int cx = (x + CX * (SCX / 2)) / CX;
		int cy = (y + CY * (SCY / 2)) / CY;
		int cz = (z + CZ * (SCZ / 2)) / CZ;

		if(cx < 0 || cx >= SCX || cy < 0 || cy >= SCY || cz <= 0 || cz >= SCZ)
			return;

		c[cx][cy][cz]->set(x & (CX - 1), y & (CY - 1), z & (CZ - 1), type);
	}

	void render(const glm::mat4 &pv) {
		int ux = -1;
		int uy = -1;
		int uz = -1;

		for(int x = 0; x < SCX; x++) {
			for(int y = 0; y < SCY; y++) {
				for(int z = 0; z < SCZ; z++) {
					// If this chunk is not initialized, skip it
					if(!c[x][y][z]->initialized) {
						// But if it is the closest to the center, mark it for initialization
						if(ux < 0 || hypotf(x - 16, z - 16) + y < hypotf(ux - 16, uz - 16) + uy) {
							ux = x;
							uy = y;
							uz = z;
						}
						continue;
					}

					glm::mat4 model = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(1.0/8, 1.0/8, 1.0/8)), glm::vec3(c[x][y][z]->ax * CX, c[x][y][z]->ay * CY, c[x][y][z]->az * CZ));
					glm::mat4 mvp = pv * model;
					glUniformMatrix4fv(uniform_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

					c[x][y][z]->render();
				}
			}
		}

		if(ux >= 0) {
			c[ux][uy][uz]->perlin(seed);
			if(c[ux][uy][uz]->left)
				c[ux][uy][uz]->left->perlin(seed);
			if(c[ux][uy][uz]->right)
				c[ux][uy][uz]->right->perlin(seed);
			if(c[ux][uy][uz]->below)
				c[ux][uy][uz]->below->perlin(seed);
			if(c[ux][uy][uz]->above)
				c[ux][uy][uz]->above->perlin(seed);
			if(c[ux][uy][uz]->front)
				c[ux][uy][uz]->front->perlin(seed);
			if(c[ux][uy][uz]->back)
				c[ux][uy][uz]->back->perlin(seed);
			c[ux][uy][uz]->initialized = true;
		}
	}
};

superchunk *world;

/**
 * Store all the file's contents in memory, useful to pass shaders
 * source code to OpenGL
 */
char* file_read(const char* filename) {
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
			if(!res)
				abort();
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
void print_log(GLuint object) {
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
GLint create_shader(const char* filename, GLenum type) {
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

// Calculate the forward, right and lookat vectors from the angle vector
void update_vectors() {
	forward.x = sinf(angle.x);
	forward.y = 0;
	forward.z = cosf(angle.x);

	right.x = -cosf(angle.x);
	right.y = 0;
	right.z = sinf(angle.x);

	lookat.x = sinf(angle.x) * cosf(angle.y);
	lookat.y = sinf(angle.y);
	lookat.z = cosf(angle.x) * cosf(angle.y);
}

int init_resources() {
	int vertex_texture_units;
	glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &vertex_texture_units);
	if(!vertex_texture_units) {
		fprintf(stderr, "Your graphics cards does not support texture lookups in the vertex shader!\n");
		return 0;
	}

	GLint link_ok = GL_FALSE;

	GLuint vs, fs;
	if ((vs = create_shader("glescraft.v.glsl", GL_VERTEX_SHADER))	 == 0) return 0;
	if ((fs = create_shader("glescraft.f.glsl", GL_FRAGMENT_SHADER)) == 0) return 0;

	program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
	if (!link_ok) {
		fprintf(stderr, "glLinkProgram:");
		return 0;
	}

	const char* attribute_name;
	attribute_name = "coord";
	attribute_coord = glGetAttribLocation(program, attribute_name);
	if (attribute_coord == -1) {
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

	/* Upload the texture with our datapoints */
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textures.width, textures.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, textures.pixel_data);
	glGenerateMipmap(GL_TEXTURE_2D);

	world = new superchunk;

	position = glm::vec3(0, 4, 0);
	angle = glm::vec3(0, -0.5, 0);
	update_vectors();

	glGenBuffers(1, &ground_vbo);
	float ground[4][4] = {
		{-256, 0, -256, 1 - 128},
		{-256, 0, +256, 1 - 128},
		{+256, 0, +256, 1 - 128},
		{+256, 0, -256, 1 - 128},
	};

	glBindBuffer(GL_ARRAY_BUFFER, ground_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof ground, ground, GL_STATIC_DRAW);

	glGenBuffers(1, &cursor_vbo);

	return 1;
}

void reshape(int w, int h) {
	ww = w;
	wh = h;
	glViewport(0, 0, w, h);
}

// Not really GLSL fract(), but the absolute distance to the nearest integer value
float fract(float value) {
	float f = value - floorf(value);
	if(f > 0.5)
		return 1 - f;
	else
		return f;
}

void display() {
	glUseProgram(program);
	glUniform1i(uniform_texture, 0);

	glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0/8, 1.0/8, 1.0/8));
	glm::mat4 view = glm::lookAt(position, position + lookat, glm::vec3(0.0, 1.0, 0.0));
	glm::mat4 projection = glm::perspective(45.0f, 1.0f*ww/wh, 0.01f, 100.0f);

	glm::mat4 mvp = projection * view * model;

	glUniformMatrix4fv(uniform_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

	glClearColor(0.6, 0.8, 1.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/* Set texture interpolation mode */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Use GL_NEAREST_MIPMAP_LINEAR if you want to use mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glPolygonOffset(2, 2);
	glEnable(GL_POLYGON_OFFSET_FILL);

	glEnableVertexAttribArray(attribute_coord);

	/* Enable blending? Only works correctly when rendering in the correct order. */

	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* Draw the ground if we only have one layer of chunks */

	if(SCY <= 1) {
		glBindBuffer(GL_ARRAY_BUFFER, ground_vbo);
		glVertexAttribPointer(attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	/* Then draw chunks */

	world->render(projection * view);

	/* Find out coordinates of the center pixel */

	float depth;
	glReadPixels(ww / 2, wh / 2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

	glm::vec4 viewport = glm::vec4(0, 0, ww, wh);
	glm::vec3 wincoord = glm::vec3(ww / 2, wh / 2, depth);
	glm::vec3 objcoord = glm::unProject(wincoord, view, projection, viewport);
	objcoord *= 8;

	/* Find out which block it belongs to */

	mx = objcoord.x;
	my = objcoord.y;
	mz = objcoord.z;
	if(objcoord.x < 0)
		mx--;
	if(objcoord.y < 0)
		my--;
	if(objcoord.z < 0)
		mz--;
	float bx = mx;
	float by = my;
	float bz = mz;

	/* Find out which face of the block we are looking at */

	if(fract(objcoord.x) < fract(objcoord.y))
		if(fract(objcoord.x) < fract(objcoord.z))
			face = 0; // X
		else
			face = 2; // Z
	else
		if(fract(objcoord.y) < fract(objcoord.z))
			face = 1; // Y
		else
			face = 2; // Z

	if(face == 0 && lookat.x > 0)
		face += 3;
	if(face == 1 && lookat.y > 0)
		face += 3;
	if(face == 2 && lookat.z > 0)
		face += 3;


	/* Render a box around the block we are pointing at */

	float box[24][4] = {
		{bx + 0, by + 0, bz + 0, 14},
		{bx + 1, by + 0, bz + 0, 14},
		{bx + 0, by + 1, bz + 0, 14},
		{bx + 1, by + 1, bz + 0, 14},
		{bx + 0, by + 0, bz + 1, 14},
		{bx + 1, by + 0, bz + 1, 14},
		{bx + 0, by + 1, bz + 1, 14},
		{bx + 1, by + 1, bz + 1, 14},

		{bx + 0, by + 0, bz + 0, 14},
		{bx + 0, by + 1, bz + 0, 14},
		{bx + 1, by + 0, bz + 0, 14},
		{bx + 1, by + 1, bz + 0, 14},
		{bx + 0, by + 0, bz + 1, 14},
		{bx + 0, by + 1, bz + 1, 14},
		{bx + 1, by + 0, bz + 1, 14},
		{bx + 1, by + 1, bz + 1, 14},

		{bx + 0, by + 0, bz + 0, 14},
		{bx + 0, by + 0, bz + 1, 14},
		{bx + 1, by + 0, bz + 0, 14},
		{bx + 1, by + 0, bz + 1, 14},
		{bx + 0, by + 1, bz + 0, 14},
		{bx + 0, by + 1, bz + 1, 14},
		{bx + 1, by + 1, bz + 0, 14},
		{bx + 1, by + 1, bz + 1, 14},
	};

	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_CULL_FACE);
	glUniformMatrix4fv(uniform_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
	glBindBuffer(GL_ARRAY_BUFFER, cursor_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof box, box, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_LINES, 0, 36);

	/* Draw a cross in the center of the screen */

	float cross[4][4] = {
		{-0.05, 0, 0, 13},
		{+0.05, 0, 0, 13},
		{0, -0.05, 0, 13},
		{0, +0.05, 0, 13},
	};

	glDisable(GL_DEPTH_TEST);
	glm::mat4 one(1);
	glUniformMatrix4fv(uniform_mvp, 1, GL_FALSE, glm::value_ptr(one));
	glBindBuffer(GL_ARRAY_BUFFER, cursor_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof cross, cross, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_LINES, 0, 36);

	/* And we are done */

	glutSwapBuffers();
}

void special(int key, int x, int y) {
	switch(key) {
		case GLUT_KEY_LEFT:
			keys |= 1;
			break;
		case GLUT_KEY_RIGHT:
			keys |= 2;
			break;
		case GLUT_KEY_UP:
			keys |= 4;
			break;
		case GLUT_KEY_DOWN:
			keys |= 8;
			break;
		case GLUT_KEY_PAGE_UP:
			keys |= 16;
			break;
		case GLUT_KEY_PAGE_DOWN:
			keys |= 32;
			break;
		case GLUT_KEY_HOME:
			position = glm::vec3(0, 2, 0);
			angle = glm::vec3(0, 0, 0);
			update_vectors();
			break;
		case GLUT_KEY_END:
			position = glm::vec3(0, 80, 0);
			angle = glm::vec3(0, -M_PI * 0.49, 0);
			update_vectors();
			break;
	}
}

void specialup(int key, int x, int y) {
	switch(key) {
		case GLUT_KEY_LEFT:
			keys &= ~1;
			break;
		case GLUT_KEY_RIGHT:
			keys &= ~2;
			break;
		case GLUT_KEY_UP:
			keys &= ~4;
			break;
		case GLUT_KEY_DOWN:
			keys &= ~8;
			break;
		case GLUT_KEY_PAGE_UP:
			keys &= ~16;
			break;
		case GLUT_KEY_PAGE_DOWN:
			keys &= ~32;
			break;
	}
}

void idle() {
	static int pt = 0;
	static const float movespeed = 1;

	now = time(0);
	int t = glutGet(GLUT_ELAPSED_TIME);
	float dt = (t - pt) * 1.0e-3;
	pt = t;
	
	if(keys & 1)
		position -= right * movespeed * dt;
	if(keys & 2)
		position += right * movespeed * dt;
	if(keys & 4)
		position += forward * movespeed * dt;
	if(keys & 8)
		position -= forward * movespeed * dt;
	if(keys & 16)
		position.y += movespeed * dt;
	if(keys & 32)
		position.y -= movespeed * dt;

	glutPostRedisplay();
}

void motion(int x, int y) {
	static bool warp = false;
	static const float mousespeed = 0.001;

	if(!warp) {
		angle.x -= (x - ww / 2) * mousespeed;
		angle.y -= (y - wh / 2) * mousespeed;

		if(angle.x < -M_PI)
			angle.x += M_PI * 2;
		if(angle.x > M_PI)
			angle.x -= M_PI * 2;
		if(angle.y < -M_PI * 0.49)
			angle.y = -M_PI * 0.49;
		if(angle.y > M_PI * 0.49)
			angle.y = M_PI * 0.49;

		update_vectors();

		// Force the mouse pointer back to the center of the screen.
		// This causes another call to motion(), which we need to ignore.
		warp = true;
		glutWarpPointer(ww / 2, wh / 2);
	} else {
		warp = false;
	}
}

void mouse(int button, int state, int x, int y) {
	if(state != GLUT_DOWN)
		return;

	// Scrollwheel
	if(button == 3 || button == 4) {
		if(button == 3)
			buildtype--;
		else
			buildtype++;

		buildtype &= 0xf;
		fprintf(stderr, "Building blocks of type %u (%s)\n", buildtype, blocknames[buildtype]);
		return;
	}

	fprintf(stderr, "Clicked on %d, %d, %d, face %d, button %d\n", mx, my, mz, face, button);

	if(button == 0) {
		if(face == 0)
			mx++;
		if(face == 3)
			mx--;
		if(face == 1)
			my++;
		if(face == 4)
			my--;
		if(face == 2)
			mz++;
		if(face == 5)
			mz--;
		world->set(mx, my, mz, buildtype);
	} else {
		world->set(mx, my, mz, 0);
	}
}

void free_resources() {
	glDeleteProgram(program);
}

int main(int argc, char* argv[]) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA|GLUT_DEPTH|GLUT_DOUBLE);
	glutInitWindowSize(640, 480);
	glutCreateWindow("GLEScraft");

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

	printf("Use the mouse to look around.\n");
	printf("Use cursor keys, pageup and pagedown to move around.\n");
	printf("Use home and end to go to two predetermined positions.\n");
	printf("Press the left mouse button to build a block.\n");
	printf("Press the right mouse button to remove a block.\n");
	printf("Use the scrollwheel to select different types of blocks.\n");

	if (init_resources()) {
		glutSetCursor(GLUT_CURSOR_NONE);
		glutWarpPointer(320, 240);
		glutDisplayFunc(display);
		glutReshapeFunc(reshape);
		glutIdleFunc(display);
		glutSpecialFunc(special);
		glutSpecialUpFunc(specialup);
		glutIdleFunc(idle);
		glutPassiveMotionFunc(motion);
		glutMotionFunc(motion);
		glutMouseFunc(mouse);
		glutMainLoop();
	}

	free_resources();
	return 0;
}
