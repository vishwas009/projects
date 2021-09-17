#pragma once

#include <d2d1_1.h>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>
#include <strstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <immintrin.h>
#include <jpeglib.h>

#define NUM_THREADS 2
#define _abs_(x)  (((x)<0)?-(x):(x))
#define _swap_(x,y) { x = x + y; y = x - y; x = x - y; }

enum xyzw {
	X = 0, Y, Z, W
};

enum Draw_Type {
	WIRE_FRAME = 0, SOLID, TEXTURED
};

HWND Create_Window(const wchar_t* title, int wd, int ht, HINSTANCE hInst, int  nCmd, int* error, WNDPROC winproc);

struct bgra8 {
	unsigned char b = 0;
	unsigned char g = 0;
	unsigned char r = 0;
	unsigned char a = 1;
};

struct frgb {
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float a = 0.0f;
};

namespace _3D {

	struct vec2d {
		float u = 0, v = 0, w = 1, pad = 0;
	};

	struct vec3d {
		float x = 0, y = 0, z = 0, w = 1;
	};

	struct vec_tri {
		vec3d vertx[3];
		vec2d tex_vertx[3];
	};

	struct vec3dx3 {
		vec3d v1;
		vec3d v2;
		vec3d v3;
	};

	struct mat4x4 {
		float mat[4][4] = { 0 };
	};

	struct mat_tri {
		float mat[3][4] = { 0 };
		vec2d tex_mat[3] = { 0 };
		bgra8 color;
	};

	mat4x4 Identity4();
	mat4x4 XRotation_mat4(float angle);
	mat4x4 YRotation_mat4(float angle);
	mat4x4 ZRotation_mat4(float angle);
	mat4x4 Translation_mat4(float x, float y, float z);
	mat4x4 Projection_mat4(float fov_degrees, float asp_ratio, float fnear, float ffar);
	mat4x4 operator*(const mat4x4& m1, const mat4x4& m2);
	inline void tri_mat4_mult(const mat_tri& tri, const mat4x4& mat, mat_tri& out);
	inline void vec4_mat4_mult(const vec3d& V, const mat4x4& M, vec3d& out);
	void Transpose_mat4(mat4x4& matrix);

	inline vec3d operator+(vec3d& v1, vec3d& v2) {
		return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
	}

	inline vec3d operator-(vec3d& v1, vec3d& v2) {
		return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
	}

	inline vec3d operator*(vec3d& v1, float k) {
		return { v1.x * k, v1.y * k, v1.z * k };
	}

	inline vec3d operator/(vec3d& v1, float k) {
		return { v1.x / k, v1.y / k, v1.z / k };
	}

	inline float dot_vec3(vec3d& v1, vec3d& v2) {
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	inline float lenth_vec3(vec3d& v) {
		return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	inline void normalise_vec3(vec3d& v) {
		float l = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
		v.x /= l;  v.y /= l; v.z /= l;
	}

	inline float sqrd_distance(const vec3d& v1, const vec3d& v2) {
		return ((v1.x - v2.x) * (v1.x - v2.x) + (v1.y - v2.y) * (v1.y - v2.y) + (v1.z - v2.z) * (v1.z - v2.z));
	}

	vec3d cross_vec3(vec3d& v1, vec3d& v2);
	mat4x4 pointAt_mat(vec3d& pos, vec3d& target, vec3d& up);
	mat4x4 Camera_mat4(vec3d& camPostion);
	mat4x4 rt_mat_inverse(mat4x4& m);

	int left_Clipping(mat_tri& tri_in, mat_tri& tri_out1, mat_tri& tri_out2);
	int top_Clipping(mat_tri& tri_in, mat_tri& tri_out1, mat_tri& tri_out2);
	int bottom_Clipping(float wht, mat_tri& tri_in, mat_tri& tri_out1, mat_tri& tri_out2);
	int right_Clipping(float wd, mat_tri& tri_in, mat_tri& tri_out1, mat_tri& tri_out2);
	int fnear_Clipping(float fnear, mat_tri& tri_in, mat_tri& tri_out1, mat_tri& tri_out2);
	
}

using namespace _3D;

class Texture {
private:
	int i_width;
	int i_height;
	bgra8* data;

public:
	Texture() {
		i_width = 0;
		i_height = 0;
		data = nullptr;
	}

	~Texture() { delete[] data; }

	bool load_image_data(const char* jpeg_path);
	inline int image_Width() { return i_width; }
	inline int image_Heigt() { return i_height; }

	friend class gfx;
};

class mesh3d {
private:
	int num_triangles;
	mat_tri* triangles_list;
	vec3d* face_normals;
	vec3dx3* vertex_normals;
	Texture* mtexture;

public:
	mesh3d() {
		num_triangles = 0;
		triangles_list = nullptr;
		face_normals = nullptr;
		vertex_normals = nullptr;
		mtexture = nullptr;
	}

	~mesh3d() {
		delete[] triangles_list;
		delete[] face_normals;
		delete[] vertex_normals;
		mtexture = nullptr;
	}

	bool load_obj(const char* file, bool isTextured);
	void bind_Texture(Texture* tex) { mtexture = tex; }
	inline int get_num_Triangles() { return num_triangles; }

	friend class gfx;

};

class plane_Light {
private:
	vec3d position = { 0 };
	vec3d normal = { 0, 0, -1.0,  0 };
	bgra8 color = { 0 };
	float power = 1.0f;

public:
	void set_Position(const float x, const float y, const float z) {
		position.x = x; position.y = y; position.z = z;
	}
	void set_Normal(const float x, const float y, const float z) {
		normal.x = x; normal.y = y; normal.z = z;
	}
	void set_Color(const unsigned char r, const unsigned char g, const unsigned char b) {
		color.r = r; color.g = g; color.b = b;
	}
	void set_Power(float p) { power = max(0, p); }

	bgra8 get_Color() { return color; }
	float get_Power() { return power; }
	vec3d get_Position() { return position; }
	vec3d get_Normal() { return normal; }

};

struct thread_data {
	mat_tri* tris_list;
	vec3d* f_normals;
	vec3dx3* v_normals;
	int n_triangles;
};

class gfx {
private:

	int wHeight;
	int wWidth;
	bool vsync;
	HWND win_handle;
	ID2D1Factory* factory;
	ID2D1HwndRenderTarget* render_target;
	ID2D1Bitmap* bitmap;
	D2D1_SIZE_F bmp_Size;
	bgra8* scr_Buff;
	float* zBuffer;

	// For 3D stuff and calculations ////
	mat4x4 camera_mat;
	mat4x4 projection_mat;
	plane_Light light;
	vec3d camera_pos;
	mat4x4 model_mat;
	Draw_Type dtype;
	Texture* obj_tex;

	// For Multi-threading  //////////
	thread_data th_data[2];      
	std::thread draw_thd;
	std::mutex draw_lock;
	std::condition_variable draw_cv;
	std::atomic<bool> done;
	std::atomic<bool> ready;
	std::atomic<bool> kp_running;

	// For Drawing Strings /////
	bool* capital_alphs;
	bool* smaller_alphs;
	bool* digits;

	void init_font_system();

	void Textured_Triangle(int x1, int y1, float u1, float v1, float w1,
		int x2, int y2, float u2, float v2, float w2,
		int x3, int y3, float u3, float v3, float w3,
		float _If, float _I1, float _I2, float _I3);
	void Solid_Triangle(int x1, int y1, float w1,
		int x2, int y2, float w2,
		int x3, int y3, float w3,
		float intensity, bgra8 color);
	void main_Rasterizer(const int id);
	void pooled_draw(const int id);

public:
	
	gfx(HWND handle, bool sync);
	~gfx();

	bool Init();

	bool gfx_terminate()
	{
		kp_running = false; 
		draw_cv.notify_one();
		if (draw_thd.joinable()) {
			draw_thd.join();
			return true;
		}
		else return false;
	}

	void set_Frame_Variables(mat4x4* cam_mat, vec3d* cam_pos, plane_Light* light_p) {
		camera_mat = *cam_mat;
		light = *light_p;
		camera_pos = *cam_pos;
	}

	void set_Projection_Matrices(mat4x4* proj_mat) {
		projection_mat = *proj_mat;
	}

	inline void Begin_draw() { render_target->BeginDraw();  }
	inline void End_draw() { render_target->EndDraw(); }

	inline void ClearScreen(bgra8 color) {
		float lumen = color.r * 0.29 + color.g * 0.58 + color.b * 0.13;
		memset(scr_Buff, (unsigned char)lumen, sizeof(bgra8) * wHeight * wWidth);
		memset(zBuffer, 0, sizeof(float) * wHeight * wWidth);
	}

	inline void ClearScreen_D2D(float r, float g, float b, float a) { render_target->Clear(D2D1::ColorF(r, g, b, a)); }
	inline void set_Title(const char* title){ SetWindowTextA(win_handle, title); }

	inline void UpdateScreen() {
		bitmap->CopyFromMemory(NULL, scr_Buff, wWidth * 4);

		render_target->DrawBitmap(bitmap, D2D1::RectF(0.0f, 0.0f, bmp_Size.width, bmp_Size.height), 1.0f,
			D2D1_BITMAP_INTERPOLATION_MODE::D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
			D2D1::RectF(0.0f, 0.0f, bmp_Size.width, bmp_Size.height));
	}

	inline int get_Height() { return wHeight; }
	inline int get_Width() { return wWidth; }

	inline void set_Pixel(int x, int y, bgra8 color) {
		assert((x >= 0 && x <= wWidth) && (y >= 0 && y <= wHeight));
		scr_Buff[y * wWidth + x] = color;
	}

	void Line(int x1, int y1, int x2, int y2, bgra8 color);
	void Circle(int x0, int y0, int radius, bgra8 color);
	void Triangle(const int& x1, const int& y1, const int& x2, const int& y2, const int& x3, const int& y3, const bgra8& color);
	bool Draw_obj(mesh3d* mesh, const mat4x4& model_mat, Draw_Type type);
	void Draw_String(const char* str, int x, int y, bgra8 color);
	void Draw_Image(const Texture* img, int x, int y);

};
