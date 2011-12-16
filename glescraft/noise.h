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


float easeCurve(float t);
float noise2d(int x, int y, int seed);
float noise3d(int x, int y, int z, int seed);
float noise2d_gradient(float x, float y, int seed);
float noise3d_gradient(float x, float y, float z, int seed);
float noise2d_perlin(float x, float y, int seed, int octaves, float persistence);
float noise2d_perlin_abs(float x, float y, int seed, int octaves, float persistence);
float noise3d_perlin(float x, float y, float z, int seed, int octaves, float persistence);
float noise3d_perlin_abs(float x, float y, float z, int seed, int octaves, float persistence);
