// main.cpp - graphics
// Authors: William Keller (#996964933)
//          Henry Walton   (#996634700)

#ifdef __APPLE__
//#include <GL/glew.h>
#include <OpenGL/gl.h> 
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#else
#include <GL/glew.h>
#include <GL/gl.h> 
#include <GL/glu.h>
#include <GL/glut.h>

#endif

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

#include <math.h>
#include <time.h>
#include <stdlib.h>

#include "bitmap.h"

using namespace std;

// some OGL globals
GLuint shader_program;
int viewport_width = 800;
int viewport_height = 600;

// program state
unsigned int frame = 0;
double rot_x = 0;
double eyeX, eyeY, eyeZ, tarX, tarY, tarZ, upX, upY, upZ;
double viewAngX, viewAngY;
double look_x = 0;
double look_y = 0;
int block_size = 30;
bool spins = false;
bool spins_pause = true;
bool follow_car = false;
GLuint textures[6];
GLint sun_unif;

// forward decs of some util funcs
string get_contents(const char* filename);
GLint setup_shader(const char* filename);
bool setup_graphics();
void setup_textures();
void setup_camera();
void setup_viewport();
GLuint load_texture(const char* filename);
void passive_motion(int x, int y);

// forward decs of handlers
void reshape_handler(GLint new_x, GLint new_y);
void display_handler();
void mouse_handler(GLint button, GLint state, GLint pos_x, GLint pos_y);
void keyboard_handler(unsigned char key, GLint pos_x, GLint pos_y);
void keyboard_special_handler(int key, GLint pos_x, GLint pos_y);
void idle_handler();

// forward decs of camera functions
void move(int dir);
void look(int dir);
void turn(int dir);
void updateView();
void track_car();
double rads(double degrees);

// constants
#define RIGHT 0
#define LEFT 1
#define UP 2
#define DOWN 3
#define STOP 4
#define PI 3.14159265

// forward decs of some graphics helpers
//void building(int height);
void building(int height, int tex);
void car_block(int height, float r, float g, float b);

// types and classes
typedef struct Point2D_struct {
	GLfloat x;
	GLfloat y;
	Point2D_struct(float xx, float yy) : x(xx), y(yy) {};
	Point2D_struct() {};
} Point2D_t;

typedef struct Point3D_struct {
	GLfloat x;
	GLfloat y;
	GLfloat z;
	Point3D_struct(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {};
	Point3D_struct() {};
} Point3D_t;

class BlockIterator {
	// Faux iterator.
	// This is supposed to give us the smallest (x, y) point in each block
	private:
		int m_state;
		int m_width, m_height;
		int m_num_x_blocks, m_num_y_blocks;

	public:
		BlockIterator(int width, int height, int block_size) : m_width(width), m_height(height), m_state(0) {
			// rounds down and throws out the extra space
			m_num_x_blocks = m_width / block_size;
			m_num_y_blocks = m_height / block_size;
		}
		
		Point2D_t next() {
			int x_offset = (m_width / m_num_x_blocks) * (m_state % m_num_x_blocks); // yes that's recreating block_size, but it's the trimmed one now
			int y_offset = (m_state / m_num_x_blocks) * (m_height / m_num_y_blocks); // integer division

			Point2D_t output = Point2D_t(x_offset, y_offset);// *output = new Point2D(x_offset, y_offset);
			m_state++;
			return output;
		}

		bool has_next() {
			return (m_state < m_num_x_blocks * m_num_y_blocks) ? true : false;
		}

		void reset() {
			m_state = 0;
		}
};

class Building {
	private:
		int m_height;
		int m_tex;
		GLint m_tex_unif;

		Point3D_t m_verts[8];
		Point2D_t m_texcoords[4];
		Point3D_t m_normals[8];

		void vert(int n) {
			Point3D_t normal = m_normals[n - 1];
			Point3D_t vertex = m_verts[n - 1];
			glNormal3f(normal.x, normal.y, normal.z);
			glVertex3f(vertex.x, vertex.y, vertex.z);
		}

		void texcoord(int n) {
			Point2D_t vertex = m_texcoords[n - 1];
			glTexCoord2f(vertex.x, vertex.y);
		}

		void tex(int n) {
			m_tex_unif = glGetUniformLocation(shader_program, "tex_flag");
			glUniform1f(m_tex_unif, n);
		}

		// one private method per textured face (as we can't change texture in an FFP block). 6 faces here.

		void emit_front() {
			// front face
			glBegin(GL_TRIANGLES);
				glColor3d(1.0, 0.0, 0.0);
				texcoord(4); vert(4);    texcoord(1); vert(1);    texcoord(2); vert(2); // LL tri
				texcoord(4); vert(4);    texcoord(2); vert(2);    texcoord(3); vert(3); // UR tri
			glEnd();
		}

		void emit_back() {
			// back face
			glBegin(GL_TRIANGLES);
				glColor3d(0.0, 1.0, 0.0);
				texcoord(4); vert(5);    texcoord(1); vert(7);    texcoord(2); vert(8); // LL tri
				texcoord(4); vert(5);    texcoord(2); vert(8);    texcoord(3); vert(6); // UR tri
			glEnd();			
		}

		void emit_left() {
			// left face
			glBegin(GL_TRIANGLES); 
				glColor3d(0.0, 0.0, 1.0);
				texcoord(4); vert(5);    texcoord(1); vert(7);    texcoord(2); vert(1); // LL tri
				texcoord(4); vert(5);    texcoord(2); vert(1);    texcoord(3); vert(4); // UR tri
			glEnd();
		}

		void emit_right() {
			// right face
			glBegin(GL_TRIANGLES);
				glColor3d(1.0, 1.0, 1.0);
				texcoord(4); vert(3);    texcoord(1); vert(2);    texcoord(2); vert(8); // LL tri
				texcoord(4); vert(3);    texcoord(2); vert(8);    texcoord(3); vert(6); // UR tri
			glEnd();
		}

		void emit_up() {
			// up face
			glBegin(GL_TRIANGLES);
				glColor3d(0.76, 0.76, 0.76);
				texcoord(4); vert(5);    texcoord(1); vert(4);    texcoord(2); vert(3); // LL tri
				texcoord(4); vert(5);    texcoord(2); vert(3);    texcoord(3); vert(6); // UR tri
			glEnd();
		}

		void emit_down() {
			// down face
			glBegin(GL_TRIANGLES);
				glColor3d(1.0, 1.0, 0.0);
				texcoord(4); vert(1);    texcoord(1); vert(7);    texcoord(2); vert(8); // LL tri
				texcoord(4); vert(1);    texcoord(2); vert(8);    texcoord(3); vert(2); // UR tri
			glEnd();
		}

	public:
		Building(int height, int tex) : m_height(height), m_tex(tex+1) {
			// uniform location to select texture
			GLint m_tex_unif = glGetUniformLocation(shader_program, "tex_flag");

			// geometry data
			m_verts[0] = Point3D_t(-1, -1, 1);
			m_verts[1] = Point3D_t( 1, -1, 1);
			m_verts[2] = Point3D_t( 1,  1*m_height,  1);
			m_verts[3] = Point3D_t(-1,  1*m_height,	 1);
			m_verts[4] = Point3D_t(-1,  1*m_height, -1);
			m_verts[5] = Point3D_t( 1,  1*m_height, -1);
			m_verts[6] = Point3D_t(-1, -1, -1);
			m_verts[7] = Point3D_t( 1, -1, -1);

			m_texcoords[0] = Point2D_t(0, 0);
			m_texcoords[1] = Point2D_t(1, 0);
			m_texcoords[2] = Point2D_t(1, 1);
			m_texcoords[3] = Point2D_t(0, 1);

			//m_normals[0] = Point3D_t(-.577, 0, .577);
			//m_normals[1] = Point3D_t(.577, 0, .577);
			//m_normals[2] = Point3D_t(.577, .577, .577);
			//m_normals[3] = Point3D_t(-.577, .577, .577);
			//m_normals[4] = Point3D_t(-.577, .577, -.577);
			//m_normals[5] = Point3D_t(.577, .577, -.577);
			//m_normals[6] = Point3D_t(-.577,0 , -.577);
			//m_normals[7] = Point3D_t(.577, 0, -.577);

			m_normals[0] = Point3D_t(-1, 0,  1);
			m_normals[1] = Point3D_t( 1, 0,  1);
			m_normals[2] = Point3D_t( 1, 1,  1);
			m_normals[3] = Point3D_t(-1, 1,  1);
			m_normals[4] = Point3D_t(-1, 1, -1);
			m_normals[5] = Point3D_t( 1, 1, -1);
			m_normals[6] = Point3D_t(-1, 0, -1);
			m_normals[7] = Point3D_t( 1, 0, -1);
		}

		void emit() {
			// we need to emit exactly 36 verts for 12 tris for 6 faces for one rectangular prism.
			m_tex_unif = glGetUniformLocation(shader_program, "tex_flag");

			tex(m_tex); emit_front();
			tex(m_tex); emit_back();
			tex(m_tex); emit_left();
			tex(m_tex); emit_right();
			tex(m_tex+1); emit_up();
			tex(m_tex+1); emit_down(); // can't see this texture anyways
			//glUniform1f(m_tex_unif, 0);
		}
};

class Car_Block {
	private:
		int m_height;
		float color_r, color_g, color_b;
		GLint m_tex_unif;

		Point3D_t m_verts[8];
		Point2D_t m_texcoords[4];
		Point3D_t m_normals[8];

		void vert(int n) {
			Point3D_t normal = m_normals[n - 1];
			Point3D_t vertex = m_verts[n - 1];
			glNormal3f(normal.x, normal.y, normal.z);
			glVertex3f(vertex.x, vertex.y, vertex.z);
		}

		void texcoord(int n) {
			Point2D_t vertex = m_texcoords[n - 1];
			glTexCoord2f(vertex.x, vertex.y);
		}

		void tex(int n) {
			m_tex_unif = glGetUniformLocation(shader_program, "tex_flag");
			glUniform1f(m_tex_unif, n);
		}

		// one private method per textured face (as we can't change texture in an FFP block). 6 faces here.

		void emit_front() {
			// front face
			glBegin(GL_TRIANGLES);
				glColor3d(color_r, color_g, color_b);
				texcoord(4); vert(4);    texcoord(1); vert(1);    texcoord(2); vert(2); // LL tri
				texcoord(4); vert(4);    texcoord(2); vert(2);    texcoord(3); vert(3); // UR tri
			glEnd();
		}

		void emit_back() {
			// back face
			glBegin(GL_TRIANGLES);
				glColor3d(color_r, color_g, color_b);
				texcoord(4); vert(5);    texcoord(1); vert(7);    texcoord(2); vert(8); // LL tri
				texcoord(4); vert(5);    texcoord(2); vert(8);    texcoord(3); vert(6); // UR tri
			glEnd();			
		}

		void emit_left() {
			// left face
			glBegin(GL_TRIANGLES); 
				glColor3d(color_r, color_g, color_b);
				texcoord(4); vert(5);    texcoord(1); vert(7);    texcoord(2); vert(1); // LL tri
				texcoord(4); vert(5);    texcoord(2); vert(1);    texcoord(3); vert(4); // UR tri
			glEnd();
		}

		void emit_right() {
			// right face
			glBegin(GL_TRIANGLES);
				glColor3d(color_r, color_g, color_b);
				texcoord(4); vert(3);    texcoord(1); vert(2);    texcoord(2); vert(8); // LL tri
				texcoord(4); vert(3);    texcoord(2); vert(8);    texcoord(3); vert(6); // UR tri
			glEnd();
		}

		void emit_up() {
			// up face
			glBegin(GL_TRIANGLES);
				glColor3d(color_r, color_g, color_b);
				texcoord(4); vert(5);    texcoord(1); vert(4);    texcoord(2); vert(3); // LL tri
				texcoord(4); vert(5);    texcoord(2); vert(3);    texcoord(3); vert(6); // UR tri
			glEnd();
		}

		void emit_down() {
			// down face
			glBegin(GL_TRIANGLES);
				glColor3d(color_r, color_g, color_b);
				texcoord(4); vert(1);    texcoord(1); vert(7);    texcoord(2); vert(8); // LL tri
				texcoord(4); vert(1);    texcoord(2); vert(8);    texcoord(3); vert(2); // UR tri
			glEnd();
		}

	public:
		Car_Block(int height, float r, float g, float b) : m_height(height), color_r(r), color_g(g), color_b(b) {
			// uniform location to select texture
			GLint m_tex_unif = glGetUniformLocation(shader_program, "tex_flag");

			// geometry data
			m_verts[0] = Point3D_t(-1, -1, 1);
			m_verts[1] = Point3D_t( 1, -1, 1);
			m_verts[2] = Point3D_t( 1,  1*m_height,  1);
			m_verts[3] = Point3D_t(-1,  1*m_height,	 1);
			m_verts[4] = Point3D_t(-1,  1*m_height, -1);
			m_verts[5] = Point3D_t( 1,  1*m_height, -1);
			m_verts[6] = Point3D_t(-1, -1, -1);
			m_verts[7] = Point3D_t( 1, -1, -1);

			m_texcoords[0] = Point2D_t(0, 0);
			m_texcoords[1] = Point2D_t(1, 0);
			m_texcoords[2] = Point2D_t(1, 1);
			m_texcoords[3] = Point2D_t(0, 1);

			//m_normals[0] = Point3D_t(-.577, 0, .577);
			//m_normals[1] = Point3D_t(.577, 0, .577);
			//m_normals[2] = Point3D_t(.577, .577, .577);
			//m_normals[3] = Point3D_t(-.577, .577, .577);
			//m_normals[4] = Point3D_t(-.577, .577, -.577);
			//m_normals[5] = Point3D_t(.577, .577, -.577);
			//m_normals[6] = Point3D_t(-.577,0 , -.577);
			//m_normals[7] = Point3D_t(.577, 0, -.577);

			m_normals[0] = Point3D_t(-1, -1,  1);
			m_normals[1] = Point3D_t( 1, -1,  1);
			m_normals[2] = Point3D_t( 1, 1,  1);
			m_normals[3] = Point3D_t(-1, 1,  1);
			m_normals[4] = Point3D_t(-1, 1, -1);
			m_normals[5] = Point3D_t( 1, 1, -1);
			m_normals[6] = Point3D_t(-1, -1, -1);
			m_normals[7] = Point3D_t( 1, -1, -1);
		}

		void emit() {
			// we need to emit exactly 36 verts for 12 tris for 6 faces for one rectangular prism.
			m_tex_unif = glGetUniformLocation(shader_program, "tex_flag");

			tex(0); emit_front();
			tex(0); emit_back();
			tex(0); emit_left();
			tex(0); emit_right();
			tex(0); emit_up();
			tex(0); emit_down(); // can't see this texture anyways
			//glUniform1f(m_tex_unif, 0);
		}
};

class Car {
	private:
		int m_color; // the color is 4. deal with it.
		int m_heading; // RIGHT, LEFT, UP, or DOWN
		float color_r, color_g, color_b;
		double m_speed; // inverse of the number of frames to traverse one block
		int m_ticks; // how many frames we are into the current movement

		bool can_move() {
			switch(m_heading) {
				case RIGHT:
					if(m_x_pos < 300 - 10 - 1)
						return true;
					break;
				case LEFT:
					if(m_x_pos > 0 + 10 + 1)
						return true;
					break;
				case UP:
					if (m_y_pos < 300 - 10 - 1)
						return true;
					break;
				case DOWN:
					if(m_y_pos > 0 + 10 + 1)
						return true;
					break;
				case STOP:
					return true;
					break;
			}
			return false;
		}

	public:
		double m_x_pos, m_y_pos; // center of the car

		Car(int x_start, int y_start) : m_x_pos(x_start), m_y_pos(y_start) {
			m_color = rand() % 6;
			switch (m_color) {
				case 0: // Red
				color_r = 0.8; color_g = 0; color_b = 0; break;

				case 1: // Green
				color_r = 0; color_g = 0.8; color_b = 0; break;

				case 2: // Blue
				color_r = 0; color_g = 0; color_b = 0.8; break;

				case 3: // Dark Grey
				color_r = 0.4; color_g = 0.4; color_b = 0.4; break;

				case 4: // White
				color_r = 0.9; color_g = 0.9; color_b = 0.9; break;

				case 5: // Yellow
				color_r = 0.8; color_g = 0.8; color_b = 0; break;
			}

		}

		int get_heading() {
			return m_heading;
		}

		void start_movement(int heading, double speed) {
			m_speed = speed;
			m_ticks = 0;	
			// Don't do U turns
			if (heading == LEFT && m_heading == RIGHT) {
			} else if (heading == RIGHT && m_heading == LEFT) {
			} else if (heading == UP && m_heading == DOWN) {
			} else if (heading == DOWN && m_heading == UP) {
			} else { m_heading = heading; 
			}
		}

		void tick() { // advance animation one frame
			if (m_ticks == 1.0 / m_speed) {
				do {
					start_movement((rand() % 9) % 5, .01);
				} while(!can_move());
			}

			m_ticks++;

			// negate the correct terms for different directions
			// we can move to true ipo easily if need be
			switch(m_heading) {
				case RIGHT:
					m_x_pos += block_size * m_speed;
					break;
				case LEFT:
					m_x_pos -= block_size * m_speed;
					break;
				case UP:
					m_y_pos += block_size * m_speed;
					break;
				case DOWN:
					m_y_pos -= block_size * m_speed;
					break;
				case STOP:
					break;
			}
		}

		bool is_stopped() {
			return m_heading == STOP;
		}
		void drawYoSelf() {
			
			GLint tex_flag_unif = glGetUniformLocation(shader_program, "tex_flag"); // send the resolution to the shader
			glUniform1f(tex_flag_unif, 0.0);	

			glPushMatrix();
			glTranslatef(m_x_pos, 0.0, -1 * m_y_pos);
			if(m_heading == RIGHT || m_heading == LEFT)
				glRotatef(90, 0, 1, 0);
			car_block(1, color_r, color_g, color_b);

			glPushMatrix();
			glTranslatef(0, 2, 0);
			car_block(1, color_r, color_g, color_b);
			glPopMatrix();

			glPushMatrix();
			glTranslatef(0, 0, -2);
			car_block(1, color_r, color_g, color_b);
			glPopMatrix();

			glPushMatrix();
			glTranslatef(0, 0, 2);
			car_block(1, color_r, color_g, color_b);
			glPopMatrix();

			/*glPushMatrix();
			glTranslatef(0, 4, 0);
			glRotatef(((m_ticks/m_speed)*360)*0.0174532925, 0, 1, 0);
			car_block(1, color_r, color_g, color_b);
			glPopMatrix();*/

			glPopMatrix();
		}
};

class RandomIterator {
	private:
		int m_state;
		int m_size;
		int m_range;
		double *m_values;

	public:
		RandomIterator(int size, int range) : m_size(size), m_range(range), m_state(0) {
			m_values = new double[m_size];
			for (int i = 0; i < m_size; i++){
				m_values[i] = rand() % m_range;
			}
		}

		double next() {
			return m_values[m_state++];
		}
		bool has_next() {
			return (m_state < m_size) ? true : false;
		}
		void reset() {
			m_state = 0;
		}
};

class TrafficConductor {
	private:
		int m_car_number;
	public:
		vector<Car*> cars;
		TrafficConductor(int car_number) : m_car_number(car_number) {
			for (int i = 0; i < m_car_number; i++) {
				cars.push_back(new Car( (rand() % 10)*30, (rand() % 10)*30));
				//cars.push_back(new Car(30, 30));
			}
		}

		void start_cars() {
			for(vector<Car*>::iterator car = cars.begin(); car != cars.end(); ++car) {
				(*car)->start_movement(UP, .01);
			}
		}

		void tick_cars() {
			for(vector<Car*>::iterator car = cars.begin(); car != cars.end(); ++car) {
				(*car)->tick();
			}
		}

		void draw_cars() {
			for(vector<Car*>::iterator car = cars.begin(); car != cars.end(); ++car) {
				(*car)->drawYoSelf();
			}
		}
};

BlockIterator *blocks = new BlockIterator(300, 300, block_size);
RandomIterator *heights = new RandomIterator(100, 5);
TrafficConductor *car_controller = new TrafficConductor(40);

int main(int argc, char **argv) {
	// init glut and let it eat the args it wants to
    //time_t timer;
	//srand(time(&timer));
	glutInit(&argc, argv);

	if(setup_graphics() != true) {
		cout << "Exiting with errors." << endl;
		return 1;
	}

	car_controller->start_cars();

	// good to go! enter main loop
	glutMainLoop();
}


////////////////////////////////////////////// some OGL utility functions
string get_contents(const char* filename) {
	// stream to string conversion nabbed from stack overflow
	ifstream fh(filename);
	string str((istreambuf_iterator<char>(fh)), istreambuf_iterator<char>());
	return str;
}

GLint setup_shader(const char* filename, GLenum kind) {
	GLint shader = glCreateShader(kind);
	string source = get_contents(filename);

	// ugly, have to figure out the proper cast
	const char *wrap[1];
	wrap[0] = source.c_str();

	// try to compile shader
	glShaderSource(shader, 1, wrap, NULL); // 1, NULL means take a 1-length array of cstrings
	glCompileShader(shader);

	// check if it worked
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

	if(status != GL_TRUE) {
		// shader error. print it and return with failure.
		GLchar error_log[1024]; // hopefully long enough
		GLsizei length; // of the copied message

		glGetShaderInfoLog(shader, 1024, &length, error_log); // copy error to error_log
		cout << "Shader did not compile correctly." << endl;
		cout << error_log << endl;

		return -1;
	}

	// success!
	return shader;
}

GLuint load_texture(const char* filename) {
	CBitmap image(filename);
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.GetWidth(), image.GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, image.GetBits());

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


	return tex;
}

bool setup_graphics() {
	// start by setting up glut
	glutInitWindowSize(viewport_width, viewport_height);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH); // forgetting GLUT_DEPTH earned me a segfault!

   	 // make our window
  	glutCreateWindow("Tiny Town");

    // bind basic handlers
    glutDisplayFunc(display_handler);
	glutReshapeFunc(reshape_handler);
	glutIdleFunc(idle_handler);

	// bind input handlers
	glutMouseFunc(mouse_handler);
	glutKeyboardFunc(keyboard_handler);
	glutSpecialFunc(keyboard_special_handler);

	#ifndef __APPLE__
	glewInit();
	#endif
	
	// now we can set up our shaders
	shader_program = glCreateProgram(); // holds collection of shaders
	GLint arrow_frag_shader = setup_shader("frag.glsl", GL_FRAGMENT_SHADER);
	GLint arrow_vert_shader = setup_shader("vert.glsl", GL_VERTEX_SHADER);

	// check shader compiled okay
	if(arrow_frag_shader == -1 || arrow_vert_shader == -1) {
		cout << "Aborting due to shader compilation error." << endl;
		return false; // exit with error
	}

	// attach the shader to the program, then link and activate it.
	glAttachShader(shader_program, arrow_frag_shader);
	glAttachShader(shader_program, arrow_vert_shader);

	glLinkProgram(shader_program);

	// make sure it linked
	GLint status;
	glGetProgramiv(shader_program, GL_LINK_STATUS, &status);

	if(status == GL_FALSE) {
		// link error. print and die.
		GLchar error_log[1024];
		GLsizei length;
		glGetProgramInfoLog(shader_program, 1024, &length, error_log);
		cout << "Link error." << endl;
		cout << error_log << endl;
		cout << "Aborting due to link error." << endl;
		return false;
	}

	// Load textures
	setup_textures();
	// Initialize Camera
	setup_camera();
	sun_unif = glGetUniformLocation(shader_program, "sun_pos"); // send the resolution to the shader


	
	// PHEW! All ready to go! Push the button.
	glUseProgram(shader_program);
	
	setup_viewport();

	return true;
}

void setup_camera() {
	eyeX = 50.0; eyeY = 150.00; eyeZ = 100.0;
    upX  = 0.0; upY  = 1.0;  upZ  = 0.0;
    viewAngX = 20.0; viewAngY = -35.0;
	updateView();
}

void setup_textures() {
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
	textures[0] = load_texture("tex0.bmp");
	glBindSampler(GL_TEXTURE0, textures[0]);

	glActiveTexture(GL_TEXTURE1);
	textures[1] = load_texture("tex1.bmp");
	glBindSampler(GL_TEXTURE1, textures[1]);

	glActiveTexture(GL_TEXTURE2);
	textures[2] = load_texture("tex2.bmp");
	glBindSampler(GL_TEXTURE2, textures[2]);

	glActiveTexture(GL_TEXTURE3);
	textures[3] = load_texture("tex3.bmp");
	glBindSampler(GL_TEXTURE3, textures[3]);

	glActiveTexture(GL_TEXTURE4);
	textures[4] = load_texture("tex4.bmp");
	glBindSampler(GL_TEXTURE4, textures[4]);

	glActiveTexture(GL_TEXTURE5);
	textures[5] = load_texture("tex5.bmp");
	glBindSampler(GL_TEXTURE5, textures[5]);

	GLint tex_unif = glGetUniformLocation(shader_program, "textures"); // send the resolution to the shader
	glUniform1iv(tex_unif, 6, (const GLint*) textures);
}

void setup_viewport() {
	// (and also the shader a little bit)
	// a bunch of this we have to call at least once right off the bat, so it's in here.

	// set some default colors - we should never see them, but if we do we know something's wrong
	//glClearColor(1, 1, 1, 1.0);
	glClearColor(0.52, 0.8, 0.92, 1.0);
	glColor3d(0, 0, 0);

	// feed it a projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(75, viewport_width/viewport_height, .1, 1000);
	//gluOrtho2D(0, viewport_width, 0, viewport_height); // this is fine because I deal with the projection later!

	// and set it back to normal
	glMatrixMode(GL_MODELVIEW);

	// not managing the depth buffer has led to lots of segfaults.
	// at least I think that's why.
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);


	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// fill uniforms - size, scale, and time elapsed in millis
	GLint size_unif = glGetUniformLocation(shader_program, "size"); // send the resolution to the shader
	glUniform2f(size_unif, viewport_width, viewport_height);
}

////////////////////////////////////////////// OGL handlers
void reshape_handler(GLint new_x, GLint new_y) {
	// keep state globals in sync
	viewport_width = new_x;
	viewport_height = new_y;

	// unsure as to exactly how much I have to do here.
	glViewport(0, 0, viewport_width, viewport_height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // not managing the depth buffer has led to lots of segfaults.

	// update uniforms that changed
	GLint size_unif = glGetUniformLocation(shader_program, "size"); // send the resolution to the shader
	glUniform2f(size_unif, viewport_width, viewport_height);
}

void display_handler() {
	glUniform3f(sun_unif, 0.0, 1.0, 1.0);
	if (spins_pause) frame++;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glLoadIdentity();

	
	if (spins) gluLookAt(300*cos(frame/150.0)+150, 150, 300*sin(frame/150.0)-150, 150, 0, -150, 0, 1, 0);
	else if (follow_car) track_car();
	else gluLookAt(eyeX, eyeY, eyeZ, tarX, tarY, tarZ, upX, upY, upZ);

	glUniform3f(sun_unif, 0.0, 1.0, 1.0);
	GLint tex_flag_unif = glGetUniformLocation(shader_program, "tex_flag"); // send the resolution to the shader
	glUniform1f(tex_flag_unif, 0.0);

	glColor3d(0.2, 0.2, 0.2);
	glBegin(GL_QUADS);
		glNormal3f(0.0, 1.0, 0.0);
		glVertex3f(-5.0, 0.0, -305.0);

		glNormal3f(0.0, 1.0, 0.0);
		glVertex3f(-5.0, 0.0, 5.0);

		glNormal3f(0.0, 1.0, 0.0);
		glVertex3f(305.0, 0.0, 5.0);

		glNormal3f(0.0, 1.0, 0.0);
		glVertex3f(305.0, 0.0, -305.0);
	glEnd();

	glUniform1f(tex_flag_unif, 0.0);

	// Draw asphalt around buildings
	blocks->reset();
	while(blocks->has_next()) {
		Point2D_t p = blocks->next();
		glColor3d(0.5, 0.5, 0.5);
		glBegin(GL_TRIANGLE_STRIP);
			glVertex3f(p.x+5,  0.1, -1*p.y-25); 
			glVertex3f(p.x+5,  0.1, -1*p.y-5); 
			glVertex3f(p.x+25, 0.1, -1*p.y-25); 
			glVertex3f(p.x+25, 0.1, -1*p.y-5); 
		glEnd();
	}

	// Draw street lines
	blocks->reset();
	while(blocks->has_next()) {
		Point2D_t p = blocks->next();
		glColor3d(0.9, 0.9, 0.0);
		glBegin(GL_TRIANGLES);
			// Draw street lines along x axis
			for (int i = 2; i < block_size+2; i+=6) {
				glVertex3f(p.x+i,   0.1, -1*p.y-30);    glVertex3f(p.x+i,   0.1, -1*p.y-29.75); 
				glVertex3f(p.x+i+2, 0.1, -1*p.y-30);    glVertex3f(p.x+i+2, 0.1, -1*p.y-30); 
				glVertex3f(p.x+i,   0.1, -1*p.y-29.75); glVertex3f(p.x+i+2, 0.1, -1*p.y-29.75); 

				glVertex3f(p.x+i,   0.1, -1*p.y);      glVertex3f(p.x+i,   0.1, -1*p.y-0.25); 
				glVertex3f(p.x+i+2, 0.1, -1*p.y);      glVertex3f(p.x+i+2, 0.1, -1*p.y); 
				glVertex3f(p.x+i,   0.1, -1*p.y-0.25); glVertex3f(p.x+i+2, 0.1, -1*p.y-0.25); 

			}
			// Draw street lines along y axis
			for (int i = 2; i < block_size+2; i+=6) {
				glVertex3f(p.x,      0.1, -1*p.y-i);   glVertex3f(p.x+0.25, 0.1, -1*p.y-i); 
				glVertex3f(p.x,      0.1, -1*p.y-i-2); glVertex3f(p.x,      0.1, -1*p.y-i-2);
				glVertex3f(p.x+0.25, 0.1, -1*p.y-i);   glVertex3f(p.x+0.25, 0.1, -1*p.y-i-2);

				glVertex3f(p.x+30,    0.1, -1*p.y-i);   glVertex3f(p.x+29.75, 0.1, -1*p.y-i); 
				glVertex3f(p.x+30,    0.1, -1*p.y-i-2); glVertex3f(p.x+30,    0.1, -1*p.y-i-2);
				glVertex3f(p.x+29.75, 0.1, -1*p.y-i);   glVertex3f(p.x+29.75, 0.1, -1*p.y-i-2);
			}
		glEnd();
	}

	glUniform1f(tex_flag_unif, 0.0);
	// Draw buildings
	int tex = 0;
	heights->reset();
	blocks->reset();
	while(blocks->has_next()) {
		Point2D_t p = blocks->next();
		glPushMatrix();
			int sf = heights->next();
			if (sf * 10 > 10) {
				// block_size / 2 moves the building to the center of the block
				glTranslatef(p.x + block_size / 2, 2.0, -1 * p.y - block_size / 2);
				glScalef(5.0, 1.0, 5.0); // expand the footprint
				building(10.0 * sf, tex % 3);
				tex++;
			} else {
				// If no building draw grass
				glUniform1f(tex_flag_unif, 0.0);
				glColor3d(0.2, 0.6, 0.2);
				glBegin(GL_TRIANGLE_STRIP);
					glNormal3f(0.0, 1.0, 0.0);
					glVertex3f(p.x+7,  0.16, -1*p.y-23); 
					glNormal3f(0.0, 1.0, 0.0);
					glVertex3f(p.x+7,  0.16, -1*p.y-7); 
					glNormal3f(0.0, 1.0, 0.0);
					glVertex3f(p.x+23, 0.16, -1*p.y-23); 
					glNormal3f(0.0, 1.0, 0.0);
					glVertex3f(p.x+23, 0.16, -1*p.y-7); 
				glEnd();
				glUniform1f(tex_flag_unif, 0.0);

			//if(abs(p.x*p.x + p.x/p.y + p.y*p.y) < 70000) {
					glPushMatrix();
					glTranslatef(p.x + block_size / 2, 15, -1 * p.y - block_size / 2);
					glutSolidSphere(10, 5, 5);
					glPopMatrix();
					
					glPushMatrix();
					glColor3f(.6, .3, 0);
					glScalef(1, 15.0/2, 1);
					glTranslatef(p.x + block_size / 2, 1.01, -1 * p.y - block_size / 2);
					glRotatef(90, 1, 0, 0);
					glutSolidTorus(1, 1.2, 6, 6);
					glPopMatrix();
				//}
			}
		glPopMatrix();
	}

	glUniform1f(tex_flag_unif, 0.0);
	car_controller->draw_cars();
	car_controller->tick_cars();

	glutSwapBuffers();
}

void mouse_handler(GLint button, GLint state, GLint pos_x, GLint pos_y) {
	if (follow_car) {
		//look_x = (double) pos_x/viewport_width;
		//look_y = (double) pos_y/viewport_height;	
	}
	// to be continued...
	return;
}

void keyboard_handler(unsigned char key, GLint pos_x, GLint pos_y) {
	// read in our curves
	switch(key) {
		case 'q': exit(0); break;

        case 'w': move(4); break; // forward
        case 's': move(5); break; // back
        case 'a': move(2); break; // left
        case 'd': move(3); break; // right

        case 'r': move(0); break; // up
        case 'f': move(1); break; // down

        case 'i': look(0); break; // look up
        case 'k': look(1); break; // look down
        case 'j': look(2); break; // look left
        case 'l': look(3); break; // look right

        case '1': spins = !spins; follow_car = false; break; // look right
        case '2': spins_pause = !spins_pause; break; // look right
		case '3': follow_car = !follow_car; spins = false; break;

		default:
			break;
	}
	
	// and request an update right away so it's responsive.
	glutPostRedisplay();
}

void keyboard_special_handler(int key, GLint pos_x, GLint pos_y) {
	switch(key) {
		case GLUT_KEY_UP: look(0); break;
		case GLUT_KEY_DOWN: look(1); break;
		case GLUT_KEY_LEFT: look(2); break;
		case GLUT_KEY_RIGHT: look(3); break;
		default: break;
	}
	
	// and request an update right away so it's responsive.
	glutPostRedisplay();
}

void idle_handler() {
	// if it's just sitting around, may as well ask for another frame.
	glutPostRedisplay();
}

////////////////////////////////////////////// Some other helpers

void building(int height, int tex) {
	Building b(height, tex*2);
	b.emit();
}

void car_block(int height, float r, float g, float b) {
	Car_Block c(height, r, g, b);
	c.emit();
}

////////////////////////////////////////////// Camera control
void track_car() {
	vector<Car*>::iterator it = car_controller->cars.begin();
	double x_pos = (*it)->m_x_pos;
	double y_pos = (*it)->m_y_pos;

	if ((*it)->get_heading() == UP) {
		gluLookAt(x_pos, 10.0, -y_pos,    x_pos, 10, -y_pos-1,   0, 1, 0); 
	} else if ((*it)->get_heading() == DOWN) {
		gluLookAt(x_pos, 10.0, -y_pos,    x_pos, 10, -y_pos+1,   0, 1, 0); 
	} else if ((*it)->get_heading() == LEFT) {
		gluLookAt(x_pos, 10.0, -y_pos,    x_pos-1, 10, -y_pos,   0, 1, 0); 
	} else if ((*it)->get_heading() == RIGHT) {			
		gluLookAt(x_pos, 10.0, -y_pos,    x_pos+1, 10, -y_pos,   0, 1, 0); 
	} else {
		gluLookAt(x_pos, 10.0, -y_pos,    x_pos+1, 10, -y_pos,   0, 1, 0); 
	}
}

void move(int dir) {
    switch (dir) {
        //case 0: tarY += 4.0; eyeY += 0.4; updateView(); break; // Rise
        case 0: eyeY += 4.0; updateView(); break; // Rise

        //case 1: tarY -= 4.0; eyeY -= 0.4; updateView(); break; // Drop
        case 1: eyeY -= 4.0; updateView(); break; // Drop

        case 2: eyeX -= 2.0*cos(rads(viewAngX)); // Left
                eyeZ -= 2.0*sin(rads(viewAngX));
                updateView(); break;

        case 3: eyeX += 2.0*cos(rads(viewAngX)); // Right
                eyeZ += 2.0*sin(rads(viewAngX));
                updateView(); break;

        case 4: eyeZ -= 2.0*cos(rads(viewAngX))*cos(rads(viewAngY)); // Forward
                eyeX += 2.0*sin(rads(viewAngX))*cos(rads(viewAngY));
                eyeY += 2.0*sin(rads(viewAngY));
                updateView(); break;

        case 5: eyeZ += 2.0*cos(rads(viewAngX))*cos(rads(viewAngY)); // Backwards
                eyeX -= 2.0*sin(rads(viewAngX))*cos(rads(viewAngY));
                eyeY -= 2.0*sin(rads(viewAngY));
                updateView(); break;
    }
}

void look(int dir) {
    switch (dir) {
        case 0: viewAngY += 5.0; updateView(); break; // up
        case 1: viewAngY -= 5.0; updateView(); break; //down
        case 2: viewAngX -= 5.0; updateView(); break; //left
        case 3: viewAngX += 5.0; updateView(); break; // right
    }
}

void updateView() {
    if (viewAngY > 85.0) viewAngY = 85;
    if (viewAngY < -85.0) viewAngY = -85; 
    tarZ = eyeZ - (cos(rads(viewAngX)))*(cos(rads(viewAngY)));
    tarX = eyeX + (sin(rads(viewAngX)))*(cos(rads(viewAngY)));
    tarY = eyeY + (sin(rads(viewAngY)));
}

double rads(double degrees) {
    return degrees*(PI/180.0);
}

void passive_motion(int x, int y) {
	look_x = (double) x/viewport_width;
	look_y = (double) y/viewport_height;
}
