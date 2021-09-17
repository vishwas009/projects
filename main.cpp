#include <Windows.h>
#include <chrono>
#include <string>
#include "p_gfx.h"

//extern "C" {
//	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
//}

using namespace _3D;


LRESULT CALLBACK WindowProc(HWND   hwnd, UINT   uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == WM_DESTROY) { PostQuitMessage(0); return 0; }
	if (uMsg == WM_PAINT) {}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


int WINAPI wWinMain(HINSTANCE hInstance , HINSTANCE  hPrevInstance, LPWSTR  lpCmdLine , int  nCmdShow ) {
	
	int err = 0;
	HWND winHandle = Create_Window(L"_DEMO_", 1280, 768, hInstance, nCmdShow, &err, WindowProc);
	if (err != 0)return -1;

	gfx d2d_demo(winHandle, true);
	if (!d2d_demo.Init()) {
		d2d_demo.~gfx();
		return -1;
	}
	d2d_demo.ClearScreen({ 200,200,200,200 });
	MSG msg = { 0 };
	
	const float _width = (float)d2d_demo.get_Width();
	const float _height = (float)d2d_demo.get_Height();

	auto start = std::chrono::steady_clock::now();
	auto stop = std::chrono::steady_clock::now();
	auto durt = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
	std::string win_title = "_3D_  ";
	
	Texture sptl;
	sptl.load_image_data("sptl_inv.jpg");
	mesh3d crwn;
	crwn.load_obj("fancy.obj",true);
	crwn.bind_Texture(&sptl);

	/*Texture face;
	if (!face.load_image_data("face_inv.jpg"))return -1;
	mesh3d head;
	head.load_obj("tex_head+.obj", true);
	head.bind_Texture(&face);*/

	mesh3d car;
	car.load_obj("car.obj", false);

	mesh3d tree;
	tree.load_obj("tree.obj", true);

	mesh3d plane;
	plane.load_obj("plane.obj", true);
	Texture concrete;
	concrete.load_image_data("concrete.jpg");
	plane.bind_Texture(&concrete);

	mesh3d man;
	man.load_obj("man.obj", true);
	Texture cloth;
	cloth.load_image_data("cloth.jpg");
	man.bind_Texture(&cloth);

	Texture giraffe;
	giraffe.load_image_data("grf.jpg");

	/*Texture body;
	body.load_image_data("body2.jpg");
	mesh3d torso;
	torso.load_obj("body.obj", true);
	torso.bind_Texture(&body);*/

	//mat4x4 proj_mat = Projection_mat4(90.0f,_width/_height, 0.5f, 100.0f);
	mat4x4 proj_mat = Projection_mat4(70.0f, 1.0, 0.5f, 100.0f);
	d2d_demo.set_Projection_Matrices(&proj_mat);

	mat4x4 matRotZ, matRotX, matRotY;
	float fTheta = 0, xTheta = 0.0, yTheta = 0, zTheta = 3.14f;
	vec3d cam_pos = { 0.0,0.0,0.0 };
	plane_Light light;
	light.set_Color(0, 0, 0);
	light.set_Position(0, 2.0f, 0);
	light.set_Normal(0, -0.65f, -1.0);
	light.set_Power(300);
	float vt = 0;
	float xo = 0;

	bool run = true;
	
	while (run) {
		//start = std::chrono::steady_clock::now();

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)run = false;
		}

		d2d_demo.Begin_draw();
		d2d_demo.ClearScreen({ 50,50,50,200 });

		if (GetAsyncKeyState(0x58)) run = false;
		if (GetAsyncKeyState(0x57)) cam_pos.z += 1.0f * 0.05f;
		if (GetAsyncKeyState(0x53)) cam_pos.z -= 1.0f * 0.05f;
		if (GetAsyncKeyState(0x41)) cam_pos.x += 1.0f * 0.05f;
		if (GetAsyncKeyState(0x44)) cam_pos.x -= 1.0f * 0.05f;
		if (GetAsyncKeyState(0x26)) cam_pos.y += 1.0f * 0.05f;
		if (GetAsyncKeyState(0x28)) cam_pos.y -= 1.0f * 0.05f;
		if (GetAsyncKeyState(0x54)) xTheta += 1.0f * 0.05f;
		if (GetAsyncKeyState(0x55)) zTheta += 1.0f * 0.05f;
		if (GetAsyncKeyState(0x59)) yTheta += 1.0f * 0.05f;
		if (GetAsyncKeyState(0x46)) xo += 0.01f * 0.5f;

		//fTheta += 1.0f * 0.05f;
		// Rotation Z
		matRotZ = ZRotation_mat4(zTheta);

		// Rotation X
		matRotX = XRotation_mat4(xTheta);
		matRotY = YRotation_mat4(yTheta);

		mat4x4 trans_mat = Translation_mat4(0.0, 0.2f, 3.5f);
		mat4x4 world_mat = Identity4();
		world_mat = (matRotZ * matRotY) * matRotX;
		world_mat = world_mat * trans_mat;

		/*vec3d look_dir = { 0,0,1 };
		vec3d up = { 0,1,0 };
		vec3d target = cam_pos + look_dir;
		mat4x4 mat_camera = pointAt_mat(cam_pos, target, up);
		mat4x4 mat_view = rt_mat_inverse(mat_camera);*/
		mat4x4 mat_view = Camera_mat4(cam_pos);

		d2d_demo.set_Frame_Variables(&mat_view, &cam_pos, &light);

		//if (!d2d_demo.Draw_obj(&head, world_mat, TEXTURED))return -1;
		if (!d2d_demo.Draw_obj(&man, world_mat, TEXTURED))return -1;
		

		trans_mat = Translation_mat4(0.0, 0.25f, 5.0f);
		world_mat = Identity4();
		world_mat = (matRotZ * matRotY) * matRotX;
		world_mat = world_mat * trans_mat;
		if (!d2d_demo.Draw_obj(&plane, world_mat, TEXTURED))return -1;

		d2d_demo.Draw_String("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz", 800, 10, { 10,10,200,0 });
		/*d2d_demo.Draw_String("abcdefghijklmnopqrstuvwxyz", 10, 10, { 10,10,200,0 });
		d2d_demo.Draw_String("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 10, 50, { 200,10,10,0 });
		d2d_demo.Draw_String("0123456789", 10, 100, { 10,200,10,0 });
		d2d_demo.Draw_Image(&giraffe, 400, 100);*/


		trans_mat = Translation_mat4(-2.0f, 0.25f, 5.0f);
		world_mat = Identity4();
		matRotY = YRotation_mat4(0);
		world_mat = (matRotZ * matRotY) * matRotX;
		world_mat = world_mat * trans_mat;
		if (!d2d_demo.Draw_obj(&tree, world_mat, SOLID))return -1;

		trans_mat = Translation_mat4(2.0f, 0.2f, 6.0f);
		world_mat = Identity4();
		matRotY = YRotation_mat4(0);
		world_mat = (matRotZ * matRotY) * matRotX;
		world_mat = world_mat * trans_mat;
		if (!d2d_demo.Draw_obj(&car, world_mat, SOLID))return -1;

		d2d_demo.UpdateScreen();
		d2d_demo.End_draw();

		//Sleep(100);
		/*stop = std::chrono::steady_clock::now();
		durt = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
		
		win_title = "_DEMO_ FPS :  ";
		vt = 0.95 * vt + 0.05 * (1000000 / durt.count());
		win_title = win_title + std::to_string((int)vt);
		d2d_demo.set_Title(win_title.c_str());*/

	}
	//d2d_demo.gfx_terminate();

	
	return 0;
}