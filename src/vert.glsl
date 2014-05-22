uniform vec3 sun_pos;

varying vec2 texCoord;
varying float diffVal;

void main() {
	gl_FrontColor = gl_Color;
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	texCoord = gl_MultiTexCoord0.xy;
	
	diffVal = max(dot(normalize(gl_NormalMatrix*gl_Normal), normalize(sun_pos)), 0.0);
}
