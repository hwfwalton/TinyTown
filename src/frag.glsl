uniform sampler2D textures[6];
uniform float tex_flag;

varying vec2 texCoord;
varying float diffVal;

void main() {
	if (tex_flag == 0.0) gl_FragColor = gl_Color * diffVal;
    else if (tex_flag == 1.0) gl_FragColor = texture2D(textures[0], texCoord)*diffVal;
    else if (tex_flag == 2.0) gl_FragColor = texture2D(textures[1], texCoord)*diffVal;
    else if (tex_flag == 3.0) gl_FragColor = texture2D(textures[2], texCoord)*diffVal;
    else if (tex_flag == 4.0) gl_FragColor = texture2D(textures[3], texCoord)*diffVal;
    else if (tex_flag == 5.0) gl_FragColor = texture2D(textures[4], texCoord)*diffVal;
    else if (tex_flag == 6.0) gl_FragColor = texture2D(textures[5], texCoord)*diffVal;
}
