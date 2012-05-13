varying vec4 textureCoordinates;
uniform sampler2D mytexture;

void main(void) {
    vec2 longitudeLatitude = vec2((atan(textureCoordinates.y, textureCoordinates.x) / 3.1415926 + 1.0) * 0.5,
                                  (asin(textureCoordinates.z) / 3.1415926 + 0.5));
        // processing of the texture coordinates;
        // this is unnecessary if correct texture coordinates are specified by the application

    gl_FragColor = texture2D(mytexture, longitudeLatitude);
        // look up the color of the texture image specified by the uniform "textureUnit"
        // at the position specified by "longitudeLatitude.x" and 
        // "longitudeLatitude.y" and return it in "gl_FragColor"
}
