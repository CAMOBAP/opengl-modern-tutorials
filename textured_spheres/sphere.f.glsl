varying vec4 texCoords;
uniform sampler2D mytexture;
uniform vec4 mytexture_ST; // tiling and offset parameters

void main(void) {
    vec2 longitudeLatitude = vec2((atan(texCoords.y, texCoords.x) / 3.1415926 + 1.0) * 0.5,
                                  (asin(texCoords.z) / 3.1415926 + 0.5));
        // processing of the texture coordinates;
        // this is unnecessary if correct texture coordinates are specified by the application

    // Apply tiling and offset
    vec2 texCoordsTransformed = longitudeLatitude * mytexture_ST.xy + mytexture_ST.zw;

    gl_FragColor = texture2D(mytexture, texCoordsTransformed);
        // look up the color of the texture image specified by the uniform "textureUnit"
        // at the position specified by "longitudeLatitude.x" and 
        // "longitudeLatitude.y" and return it in "gl_FragColor"
}
