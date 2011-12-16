/*
Minetest-c55
Copyright (C) 2010-2011 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <math.h>
#include "noise.h"

#define NOISE_MAGIC_X 1619
#define NOISE_MAGIC_Y 31337
#define NOISE_MAGIC_Z 52591
#define NOISE_MAGIC_SEED 1013

float linearInterpolation(float x0, float x1, float t){
    return x0 + (x1 - x0) * t;
}
 
float biLinearInterpolation(float x0y0, float x1y0, float x0y1, float x1y1, float x, float y){
    float u = linearInterpolation(x0y0, x1y0, x);
    float v = linearInterpolation(x0y1, x1y1, x);
    return linearInterpolation(u, v, y);
}

float triLinearInterpolation(float v000, float v100, float v010, float v110, float v001, float v101, float v011, float v111, float x, float y, float z) {
	return v000 * (1 - x) * (1 - y) * (1 - z) +
		v100 * x * (1 - y) * (1 - z) +
		v010 * (1 - x) * y * (1 - z) +
		v110 * x * y * (1 - z) +
		v001 * (1 - x) * (1 - y) * z +
		v101 * x * (1 - y) * z +
		v011 * (1 - x) * y * z +
		v111 * x * y * z;
}

float noise2d(int x, int y, int seed) {
	int n = (NOISE_MAGIC_X * x + NOISE_MAGIC_Y * y + NOISE_MAGIC_SEED * seed) & 0x7fffffff;
	n = (n >> 13) ^ n;
	n = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
	return 1.0 - (float)n / 1073741824;
}

float noise3d(int x, int y, int z, int seed) {
	int n = (NOISE_MAGIC_X * x + NOISE_MAGIC_Y * y + NOISE_MAGIC_Z * z + NOISE_MAGIC_SEED * seed) & 0x7fffffff;
	n = (n >> 13) ^ n;
	n = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
	return 1.0 - (float)n / 1073741824;
}

float noise2d_gradient(float x, float y, int seed) {
	// Calculate the integer coordinates
	int x0 = (x > 0.0 ? (int)x : (int)x - 1);
	int y0 = (y > 0.0 ? (int)y : (int)y - 1);

	// Calculate the remaining part of the coordinates
	float xl = x - (float)x0;
	float yl = y - (float)y0;

	// Get values for corners of cube
	float v00 = noise2d(x0, y0, seed);
	float v10 = noise2d(x0 + 1, y0, seed);
	float v01 = noise2d(x0, y0 + 1, seed);
	float v11 = noise2d(x0 + 1, y0 + 1, seed);

	// Interpolate
	return biLinearInterpolation(v00, v10, v01, v11, xl, yl);
}

float noise3d_gradient(float x, float y, float z, int seed) {
	// Calculate the integer coordinates
	int x0 = (x > 0.0 ? (int)x : (int)x - 1);
	int y0 = (y > 0.0 ? (int)y : (int)y - 1);
	int z0 = (z > 0.0 ? (int)z : (int)z - 1);

	// Calculate the remaining part of the coordinates
	float xl = x - (float)x0;
	float yl = y - (float)y0;
	float zl = z - (float)z0;

	// Get values for corners of cube
	float v000 = noise3d(x0, y0, z0, seed);
	float v100 = noise3d(x0 + 1, y0, z0, seed);
	float v010 = noise3d(x0, y0 + 1, z0, seed);
	float v110 = noise3d(x0 + 1, y0 + 1, z0, seed);
	float v001 = noise3d(x0, y0, z0 + 1, seed);
	float v101 = noise3d(x0 + 1, y0, z0 + 1, seed);
	float v011 = noise3d(x0, y0+1, z0 + 1, seed);
	float v111 = noise3d(x0 + 1, y0 + 1, z0 + 1, seed);

	// Interpolate
	return triLinearInterpolation(v000, v100, v010, v110, v001, v101, v011, v111, xl, yl, zl);
}

float noise2d_perlin(float x, float y, int seed, int octaves, float persistence) {
	float a = 0;
	float f = 1.0;
	float g = 1.0;

	for(int i = 0; i < octaves; i++)
	{
		a += g * noise2d_gradient(x * f, y * f, seed + i);
		f *= 2.0;
		g *= persistence;
	}

	return a;
}

float noise2d_perlin_abs(float x, float y, int seed, int octaves, float persistence) {
	float a = 0;
	float f = 1.0;
	float g = 1.0;

	for(int i = 0; i < octaves; i++) {
		a += g * fabs(noise2d_gradient(x * f, y * f, seed + i));
		f *= 2.0;
		g *= persistence;
	}

	return a;
}

float noise3d_perlin(float x, float y, float z, int seed, int octaves, float persistence) {
	float a = 0;
	float f = 1.0;
	float g = 1.0;

	for(int i = 0; i < octaves; i++) {
		a += g * noise3d_gradient(x * f, y * f, z * f, seed + i);
		f *= 2.0;
		g *= persistence;
	}

	return a;
}

float noise3d_perlin_abs(float x, float y, float z, int seed, int octaves, float persistence) {
	float a = 0;
	float f = 1.0;
	float g = 1.0;

	for(int i = 0; i < octaves; i++) {
		a += g * fabs(noise3d_gradient(x * f, y * f, z * f, seed + i));
		f *= 2.0;
		g *= persistence;
	}
	return a;
}
