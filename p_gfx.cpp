#include "p_gfx.h"

using namespace _3D;

HWND Create_Window(const wchar_t* title, int wd, int ht, HINSTANCE hInst, int nCmd, int* error, WNDPROC winproc)
{
	*error = 0;
	WNDCLASSEX win_class;
	ZeroMemory(&win_class, sizeof(WNDCLASSEX));
	win_class.cbSize = sizeof(WNDCLASSEX);
	win_class.hbrBackground = (HBRUSH)COLOR_WINDOW;
	win_class.hInstance = hInst;
	win_class.lpfnWndProc = winproc;
	win_class.lpszClassName = L"MainWindow";
	win_class.style = CS_HREDRAW | CS_VREDRAW;
	RegisterClassEx(&win_class);

	//LPCWSTR title = L"DEMO";
	int win_wd = (wd > 1280) ? 1280 : wd;
	int win_ht = (ht > 800) ? 800 : ht;

	RECT rect = { 0,0,win_wd,win_ht };
	AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, false, WS_EX_OVERLAPPEDWINDOW);
	HWND wHandle = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW, win_class.lpszClassName, title, WS_OVERLAPPEDWINDOW, 25, 10, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, hInst, 0);
	if (!wHandle) *error = -1;

	ShowWindow(wHandle, nCmd);
	return wHandle;
}

gfx::gfx(HWND handle, bool sync)
{
	win_handle = handle;

	RECT rect;
	GetClientRect(win_handle, &rect);
	wHeight = rect.bottom - rect.top;
	wWidth = rect.right - rect.left;

	vsync = sync;
	factory = NULL;
	render_target = NULL;
	bitmap = NULL;
	bmp_Size = { 0 };

	scr_Buff = new bgra8[wHeight * wWidth];
	memset(scr_Buff, 200, sizeof(bgra8) * wHeight * wWidth);
	zBuffer = new float[wHeight * wWidth];
	memset(zBuffer, 1.0f, sizeof(float) * wHeight * wWidth);

	projection_mat = Identity4();
	camera_mat = Identity4();
	camera_pos = { 0 };

	model_mat = Identity4();
	dtype = TEXTURED;
	obj_tex = nullptr;
	th_data[0] = { nullptr, nullptr, 0 };
	th_data[1] = { nullptr, nullptr, 0 };
	ready = false;
	kp_running = false;
	done = false;
	
	capital_alphs = nullptr;
	smaller_alphs = nullptr;
	digits = nullptr;
}

gfx::~gfx()
{
	if (factory)factory->Release();
	if (render_target)render_target->Release();
	if (bitmap)bitmap->Release();

	delete[] scr_Buff;
	delete[] zBuffer;
	delete[] capital_alphs;
	delete[] smaller_alphs;
	delete[] digits;
	gfx_terminate();
}

bool gfx::Init()
{
	
	HRESULT res = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &factory);
	if (res != S_OK)return false;

	RECT rect;
	GetClientRect(win_handle, &rect);
	D2D1_SIZE_U size = D2D1::SizeU(rect.right - rect.left, rect.bottom - rect.top);

	D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd_props = D2D1::HwndRenderTargetProperties(win_handle, size);
	if(!vsync)hwnd_props.presentOptions = D2D1_PRESENT_OPTIONS_IMMEDIATELY;

	D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
	props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
	//props.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

	res = factory->CreateHwndRenderTarget(props, hwnd_props, &render_target);
	if (res != S_OK)return false;

	res = render_target->CreateBitmap(size, D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)), &bitmap);
	bmp_Size = bitmap->GetSize();

	kp_running = true;
	draw_thd = std::thread(&gfx::pooled_draw, this, 0);

	init_font_system();

	return true;
}

void gfx::Line(const int x1, const int y1, const int x2, const int y2, const bgra8 color)
{
	int dx = (x2 - x1) > 0 ? (x2 - x1) : -(x2 - x1);
	int sx = x1 < x2 ? 1 : -1;
	int dy = (y2 - y1) > 0 ? -(y2 - y1) : (y2 - y1);
	int sy = y1 < y2 ? 1 : -1;
	int err = dx + dy;
	int e2 = 0;
	int x = x1;
	int y = y1;

	while (true) {
		if (x == x2 && y == y2) break;

		if (y >= 0 && y < wHeight && x >= 0 && x < wWidth) {
			scr_Buff[(y * wWidth) + x] = color;
		}

		e2 = 2 * err;

		if (e2 >= dy) {
			err += dy;
			x += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y += sy;
		}
	}
}

void gfx::Circle(int x0, int y0, int radius, bgra8 color)
{
	if (radius <= 0 || x0 <= 0 || y0 <= 0)return;

	int f = 1 - radius;
	int ddF_x = 0;
	int ddF_y = -2 * radius;
	int x = 0;
	int y = radius;

	scr_Buff[((y0 + radius) * wWidth) + x0] = color;
	scr_Buff[((y0 - radius) * wWidth) + x0] = color;
	scr_Buff[((y0)*wWidth) + x0 + radius] = color;
	scr_Buff[((y0)*wWidth) + x0 - radius] = color;

	while (x < y) {

		if (f >= 0)
		{
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x + 1;

		scr_Buff[((y0 + y) * wWidth) + (x0 + x)] = color;
		scr_Buff[((y0 + y) * wWidth) + (x0 - x)] = color;
		scr_Buff[((y0 - y) * wWidth) + (x0 + x)] = color;
		scr_Buff[((y0 - y) * wWidth) + (x0 - x)] = color;
		scr_Buff[((y0 + x) * wWidth) + (x0 + y)] = color;
		scr_Buff[((y0 + x) * wWidth) + (x0 - y)] = color;
		scr_Buff[((y0 - x) * wWidth) + (x0 + y)] = color;
		scr_Buff[((y0 - x) * wWidth) + (x0 - y)] = color;

	}
}

void gfx::Triangle(const int& x1, const int& y1, const int& x2, const int& y2, const int& x3, const int& y3, const bgra8& color)
{
	Line(x1, y1, x2, y2, color);
	Line(x2, y2, x3, y3, color);
	Line(x3, y3, x1, y1, color);
}

void gfx::init_font_system()
{
	capital_alphs = new bool[26 * 35 * 35];
	smaller_alphs = new bool[26 * 35 * 35];
	digits = new bool[10 * 35 * 35];

	FILE* f_sheet_cap = fopen("dpnds/a-z_capital.sht", "rb");
	FILE* f_sheet_sma = fopen("dpnds/a-z_smaller.sht", "rb");
	char* temp_buff = new char[35 * 35];

	for (int k = 0; k < 26; k++) {
		fread(temp_buff, sizeof(char), 35 * 35, f_sheet_cap);
		for (int i = 0; i < 35; i++)
			for (int j = 0; j < 35; j++) {
				if (temp_buff[i * 35 + j] == 'Y')
					capital_alphs[k * 35 * 35 + i * 35 + j] = true;
				else
					capital_alphs[k * 35 * 35 + i * 35 + j] = false;
			}

		fread(temp_buff, sizeof(char), 35 * 35, f_sheet_sma);
		for (int i = 0; i < 35; i++)
			for (int j = 0; j < 35; j++) {
				if (temp_buff[i * 35 + j] == 'Y')
					smaller_alphs[k * 35 * 35 + i * 35 + j] = true;
				else
					smaller_alphs[k * 35 * 35 + i * 35 + j] = false;
			}
	}
	fclose(f_sheet_cap); fclose(f_sheet_sma);

	FILE* f_sheet_dgt = fopen("dpnds/dgt_punc.sht", "rb");
	for (int k = 0; k < 10; k++) {
		fread(temp_buff, sizeof(char), 35 * 35, f_sheet_dgt);
		for (int i = 0; i < 35; i++)
			for (int j = 0; j < 35; j++) {
				if (temp_buff[i * 35 + j] == 'Y')
					digits[k * 35 * 35 + i * 35 + j] = true;
				else
					digits[k * 35 * 35 + i * 35 + j] = false;
			}
	}
	fclose(f_sheet_dgt);
	delete[] temp_buff;
}

void gfx::Textured_Triangle(int x1, int y1, float u1, float v1, float w1,
	int x2, int y2, float u2, float v2, float w2,
	int x3, int y3, float u3, float v3, float w3,
	float _If, float _I1, float _I2, float _I3)
{

	if (y2 < y1)
	{
		std::swap(y1, y2); std::swap(x1, x2);
		std::swap(u1, u2); std::swap(v1, v2); std::swap(w1, w2);
		std::swap(_I1, _I2);
	}

	if (y3 < y1)
	{
		std::swap(y1, y3); std::swap(x1, x3);
		std::swap(u1, u3); std::swap(v1, v3); std::swap(w1, w3);
		std::swap(_I1, _I3);
	}

	if (y3 < y2)
	{
		std::swap(y2, y3); std::swap(x2, x3);
		std::swap(u2, u3); std::swap(v2, v3); std::swap(w2, w3);
		std::swap(_I2, _I3);
	}

	

	const int t_wd = obj_tex->i_width;
	const int t_ht = obj_tex->i_height;
	float dy1 = _abs_(y2 - y1);
	float dy2 = _abs_(y3 - y1);
	bgra8 light_col = light.get_Color();

	float tex_w = 0;
	float dx1_step = 0, dx2_step = 0,
		du1_step = 0, dv1_step = 0,
		du2_step = 0, dv2_step = 0,
		dw1_step = 0, dw2_step = 0;

	if ((int)dy2) {
		dx2_step = (x3 - x1) / dy2;
		du2_step = (u3 - u1) / dy2;
		dv2_step = (v3 - v1) / dy2;
		dw2_step = (w3 - w1) / dy2;
	}
	float rgb;
	float intensity = _If;
	float _Icx = 0, approx_I = 0;
	float _Ic1 = (_I1 - _I2) / (y2 - y1);
	float _Ic2 = (_I1 - _I3) / (y3 - y1);
	float ix1 = (((y1-1) - y2) * _I1 + (y1 - (y1-1)) * _I2) / (y2 - y1);
	float ix2 = (((y1-1) - y3) * _I1 + (y1 - (y1-1)) * _I3) / (y3 - y1);

	if ((int)dy1)
	{
		dx1_step = (x2 - x1) / dy1;
		du1_step = (u2 - u1) / dy1;
		dv1_step = (v2 - v1) / dy1;
		dw1_step = (w2 - w1) / dy1;
		

		for (int i = (int)y1; i <= (int)y2; i++)
		{	
			int ax = x1 + (float)(i - y1) * dx1_step;
			float tex_su = u1 + (float)(i - y1) * du1_step;
			float tex_sv = v1 + (float)(i - y1) * dv1_step;
			float tex_sw = w1 + (float)(i - y1) * dw1_step;

			int bx = x1 + (float)(i - y1) * dx2_step;
			float tex_eu = u1 + (float)(i - y1) * du2_step;
			float tex_ev = v1 + (float)(i - y1) * dv2_step;
			float tex_ew = w1 + (float)(i - y1) * dw2_step;
			
			if (ax > bx)
			{
				float tmp = ax; ax = bx; bx = tmp;
				tmp = tex_su; tex_su = tex_eu; tex_eu = tmp;
				tmp = tex_sv; tex_sv = tex_ev; tex_ev = tmp;
				tmp = tex_sw; tex_sw = tex_ew; tex_ew = tmp;
			}
			float tstep = 1.0f / (float)(bx - ax);
			float t = 0.0f;

			ix1 += _Ic1;
			ix2 += _Ic2;
			_Icx = (ix2 - ix1) / (bx - ax);
			approx_I = ((bx - (ax - 1)) * ix1 + ((ax - 1) - ax) * ix2) / (bx - ax);

			for (int j = ax; j < bx; j++)
			{
				tex_w = (1.0f - t) * tex_sw + t * tex_ew;

				int p_indx = i * wWidth + j;
				int textur_x = (float)t_wd * (((1.0f - t) * tex_su + t * tex_eu) / tex_w);
				int textur_y = (float)(t_ht - 1) * (((1.0f - t) * tex_sv + t * tex_ev) / tex_w);
				int t_indx = (textur_y)*t_wd + textur_x;
				/*approx_I += _Icx;
				intensity = approx_I;*/
				if (tex_w > zBuffer[p_indx])
				{
					//scr_Buff[p_indx] = obj_tex->data[t_indx];
					/*scr_Buff[p_indx].r = 255.0 * intensity > 255 ? 255 : 255.0 * intensity;
					scr_Buff[p_indx].g = 255.0 * intensity > 255 ? 255 : 255.0 * intensity;
					scr_Buff[p_indx].b = 255.0 * intensity > 255 ? 255 : 255.0 * intensity;*/
					rgb = (obj_tex->data[t_indx].r + light_col.r) / 2.0f * intensity;
					scr_Buff[p_indx].r = rgb > 255 ? 255 : rgb;
					rgb = (obj_tex->data[t_indx].g + light_col.g) / 2.0f * intensity;
					scr_Buff[p_indx].g = rgb > 255 ? 255 : rgb;
					rgb = (obj_tex->data[t_indx].b + light_col.b) / 2.0f * intensity;
					scr_Buff[p_indx].b = rgb > 255 ? 255 : rgb;
					zBuffer[p_indx] = tex_w;
				}
				t += tstep; 
			}

		}
	}

	dy1 = _abs_(y3 - y2);
	_Ic1 = (_I2 - _I3) / (y3 - y2);
	ix1 = (((y2 - 1) - y3) * _I2 + (y2 - (y2 - 1)) * _I3) / (y3 - y2);
	ix2 = (((y2 - 1) - y3) * _I1 + (y1 - (y2 - 1)) * _I3) / (y3 - y1);

	if ((int)dy1)
	{
		if ((int)dy2) dx2_step = (x3 - x1) / dy2;
		dx1_step = (x3 - x2) / dy1;
		du1_step = (u3 - u2) / dy1;
		dv1_step = (v3 - v2) / dy1;
		dw1_step = (w3 - w2) / dy1;

		for (int i = (int)y2; i <= (int)y3; i++)
		{
			int ax = x2 + (float)(i - y2) * dx1_step;
			int bx = x1 + (float)(i - y1) * dx2_step;

			float tex_su = u2 + (float)(i - y2) * du1_step;
			float tex_sv = v2 + (float)(i - y2) * dv1_step;
			float tex_sw = w2 + (float)(i - y2) * dw1_step;

			float tex_eu = u1 + (float)(i - y1) * du2_step;
			float tex_ev = v1 + (float)(i - y1) * dv2_step;
			float tex_ew = w1 + (float)(i - y1) * dw2_step;

			if (ax > bx)
			{
				float tmp = ax; ax = bx; bx = tmp;
				tmp = tex_su; tex_su = tex_eu; tex_eu = tmp;
				tmp = tex_sv; tex_sv = tex_ev; tex_ev = tmp;
				tmp = tex_sw; tex_sw = tex_ew; tex_ew = tmp;
			}

			float tstep = 1.0f / ((float)(bx - ax));
			float t = 0.0f;

			ix1 += _Ic1;
			ix2 += _Ic2;
			_Icx = (ix2 - ix1) / (bx - ax);
			approx_I = ((bx - (ax - 1)) * ix1 + ((ax - 1) - ax) * ix2) / (bx - ax);

			for (int j = ax; j < bx; j++)
			{
				tex_w = (1.0f - t) * tex_sw + t * tex_ew;

				int p_indx = i * wWidth + j;
				int textur_x = (float)t_wd * (((1.0f - t) * tex_su + t * tex_eu) / tex_w);
				int textur_y = (float)(t_ht - 1) * (((1.0f - t) * tex_sv + t * tex_ev) / tex_w);
				int t_indx = (textur_y)*t_wd + textur_x;
				/*approx_I += _Icx;
				intensity = approx_I;*/
				if (tex_w > zBuffer[p_indx])
				{
					//scr_Buff[p_indx] = obj_tex->data[t_indx];
					/*scr_Buff[p_indx].r = 255.0 * intensity > 255 ? 255 : 255.0 * intensity;
					scr_Buff[p_indx].g = 255.0 * intensity > 255 ? 255 : 255.0 * intensity;
					scr_Buff[p_indx].b = 255.0 * intensity > 255 ? 255 : 255.0 * intensity;*/
					rgb = (obj_tex->data[t_indx].r + light_col.r) / 2.0f * intensity;
					scr_Buff[p_indx].r = rgb > 255 ? 255 : rgb;
					rgb = (obj_tex->data[t_indx].g + light_col.g) / 2.0f * intensity;
					scr_Buff[p_indx].g = rgb > 255 ? 255 : rgb;
					rgb = (obj_tex->data[t_indx].b + light_col.b) / 2.0f * intensity;
					scr_Buff[p_indx].b = rgb > 255 ? 255 : rgb;
					zBuffer[p_indx] = tex_w;
				}
				t += tstep;
			}
		}
	}
}

void gfx::Solid_Triangle(int x1, int y1, float w1, int x2, int y2, float w2, int x3, int y3, float w3, float intensity, bgra8 color)
{
	if (y2 < y1)
	{
		std::swap(y1, y2); std::swap(x1, x2);
		std::swap(w1, w2);// std::swap(_I1, _I2);
	}

	if (y3 < y1)
	{
		std::swap(y1, y3); std::swap(x1, x3);
		std::swap(w1, w3);// std::swap(_I1, _I3);
	}

	if (y3 < y2)
	{
		std::swap(y2, y3); std::swap(x2, x3);
		std::swap(w2, w3);// std::swap(_I2, _I3);
	}
	
	float dy1 = _abs_(y2 - y1);
	float dy2 = _abs_(y3 - y1);
	bgra8 light_col = light.get_Color();
	unsigned char rgb = 0;

	float tex_w = 0;
	float dx1_step = 0, dx2_step = 0,
		dw1_step = 0, dw2_step = 0;

	if ((int)dy2) {
		dx2_step = (x3 - x1) / dy2;
		dw2_step = (w3 - w1) / dy2;
	}

	/*float _Icx = 0, intensity = 0;
	float _Ic1 = (_I1 - _I2) / (y2 - y1);
	float _Ic2 = (_I1 - _I3) / (y3 - y1);
	float ix1 = (((y1-1) - y2) * _I1 + (y1 - (y1-1)) * _I2) / (y2 - y1);
	float ix2 = (((y1-1) - y3) * _I1 + (y1 - (y1-1)) * _I3) / (y3 - y1);*/

	if ((int)dy1)
	{
		dx1_step = (x2 - x1) / dy1;
		dw1_step = (w2 - w1) / dy1;


		for (int i = (int)y1; i <= (int)y2; i++)
		{
			int ax = x1 + (float)(i - y1) * dx1_step;
			float tex_sw = w1 + (float)(i - y1) * dw1_step;

			int bx = x1 + (float)(i - y1) * dx2_step;
			float tex_ew = w1 + (float)(i - y1) * dw2_step;

			if (ax > bx)
			{
				float tmp = ax; ax = bx; bx = tmp;
				tmp = tex_sw; tex_sw = tex_ew; tex_ew = tmp;
			}
			float tstep = 1.0f / (float)(bx - ax);
			float t = 0.0f;

			/*ix1 += _Ic1;
			ix2 += _Ic2;
			_Icx = (ix2 - ix1) / (bx - ax);
			intensity = ((bx - (ax - 1)) * ix1 + ((ax - 1) - ax) * ix2) / (bx - ax);*/

			for (int j = ax; j < bx; j++)
			{
				int p_indx = i * wWidth + j;
				tex_w = (1.0f - t) * tex_sw + t * tex_ew;
				//intensity += _Icx;
				if (tex_w > zBuffer[p_indx])
				{
					scr_Buff[p_indx].r = color.r * intensity > 255 ? 255 : color.r * intensity;
					scr_Buff[p_indx].g = color.g * intensity > 255 ? 255 : color.g * intensity;
					scr_Buff[p_indx].b = color.b * intensity > 255 ? 255 : color.b * intensity;
					zBuffer[p_indx] = tex_w;
				}
				t += tstep;
			}

		}
	}

	dy1 = _abs_(y3 - y2);
	/*_Ic1 = (_I2 - _I3) / (y3 - y2);
	ix1 = (((y2 - 1) - y3) * _I2 + (y2 - (y2 - 1)) * _I3) / (y3 - y2);
	ix2 = (((y2 - 1) - y3) * _I1 + (y1 - (y2 - 1)) * _I3) / (y3 - y1);*/

	if ((int)dy1)
	{
		if ((int)dy2) dx2_step = (x3 - x1) / dy2;
		dx1_step = (x3 - x2) / dy1;
		dw1_step = (w3 - w2) / dy1;

		for (int i = (int)y2; i <= (int)y3; i++)
		{
			int ax = x2 + (float)(i - y2) * dx1_step;
			int bx = x1 + (float)(i - y1) * dx2_step;

			float tex_sw = w2 + (float)(i - y2) * dw1_step;

			float tex_ew = w1 + (float)(i - y1) * dw2_step;

			if (ax > bx)
			{
				float tmp = ax; ax = bx; bx = tmp;
				tmp = tex_sw; tex_sw = tex_ew; tex_ew = tmp;
			}

			float tstep = 1.0f / ((float)(bx - ax));
			float t = 0.0f;

			/*ix1 += _Ic1;
			ix2 += _Ic2;
			_Icx = (ix2 - ix1) / (bx - ax);
			intensity = ((bx - (ax - 1)) * ix1 + ((ax - 1) - ax) * ix2) / (bx - ax);*/

			for (int j = ax; j < bx; j++)
			{
				tex_w = (1.0f - t) * tex_sw + t * tex_ew;

				int p_indx = i * wWidth + j;
				//intensity += _Icx;
				if (tex_w > zBuffer[p_indx])
				{
					scr_Buff[p_indx].r = color.r * intensity > 255 ? 255 : color.r * intensity;
					scr_Buff[p_indx].g = color.g * intensity > 255 ? 255 : color.g * intensity;
					scr_Buff[p_indx].b = color.b * intensity > 255 ? 255 : color.b * intensity;
					zBuffer[p_indx] = tex_w;
				}
				t += tstep;
			}
		}
	}
}

bool gfx::Draw_obj(mesh3d* mesh, const mat4x4& mdl_mat, Draw_Type type)
{
	model_mat = mdl_mat;
	dtype = type;
	obj_tex = mesh->mtexture;
	int n_tris = mesh->get_num_Triangles();
	{
		std::lock_guard<std::mutex> lk1(draw_lock);
		th_data[0].tris_list = mesh->triangles_list;
		th_data[0].f_normals = mesh->face_normals;
		th_data[0].v_normals = mesh->vertex_normals;
		th_data[0].n_triangles = n_tris / 2;
		done = false;
		ready = true;
	}

	th_data[1].tris_list = &mesh->triangles_list[n_tris / 2];
	th_data[1].f_normals = &mesh->face_normals[n_tris / 2];
	th_data[1].v_normals = &mesh->vertex_normals[n_tris / 2];
	th_data[1].n_triangles = n_tris / 2 + n_tris % 2;

	draw_cv.notify_one();

	main_Rasterizer(1);

	while (!done) {
		//std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	ready = false;

	return true;
}

void gfx::Draw_String(const char* str, int x, int y, bgra8 color)
{
	if (str == nullptr)return;
	int max_chars = (wWidth - 35) / 28;

	for (int n = 0, nc = 0; str[nc] != '\0'; n++, nc++) {
		/*if (nc == (max_chars - 1)) {
			y += 35;
			n = 0;
		}*/
		if ((x + n * 28) >= (wWidth - 35)) {
			y += 35; n = 0; x = 10;
		}
		if (str[nc] >= 97 && str[nc] <= 122) {
			int k = (int)str[nc] - 97;
			for (int i = 0; i < 35; i++)
				for (int j = 0; j < 35; j++)
					if (smaller_alphs[k * 35 * 35 + i * 35 + j])
						scr_Buff[(i + y) * wWidth + j + n * 25 + x] = color;
		}
		else if (str[nc] >= 65 && str[nc] <= 90) {
			int k = (int)str[nc] - 65;
			for (int i = 0; i < 35; i++)
				for (int j = 0; j < 35; j++)
					if (capital_alphs[k * 35 * 35 + i * 35 + j])
						scr_Buff[(i + y) * wWidth + j + n * 25 + x] = color;
		}
		else if (str[nc] >= 48 && str[nc] <= 57) {
			int k = (int)str[nc] - 48;
			for (int i = 0; i < 35; i++)
				for (int j = 0; j < 35; j++)
					if (digits[k * 35 * 35 + i * 35 + j])
						scr_Buff[(i + y) * wWidth + j + n * 20 + x] = color;
		}
		else {

		}
	}
}

void gfx::Draw_Image(const Texture* img, int x, int y)
{
	if (img == nullptr)return;
	if ((img->i_width + x) >= wWidth || (img->i_height + y) >= wHeight)return ;

	int wd = img->i_width; int ht = img->i_height;
	for (int i = 0; i < ht; i++)
		for (int j = 0; j < wd; j++)
			scr_Buff[(i+y) * wWidth + j + x] = img->data[i * wd + j];
}

void gfx::pooled_draw(const int id)
{
	while (kp_running) {
		std::unique_lock<std::mutex> unq_lock(draw_lock);
		draw_cv.wait(unq_lock, [=] {return (ready == true || kp_running == false); });
		unq_lock.unlock();

		if (!kp_running) {
			done = true;
			return;
		}

		main_Rasterizer(id);

		done = true;
		ready = false;
	}
}

void gfx::main_Rasterizer(const int id)
{
	mat4x4 mdl_mat = model_mat;
	Transpose_mat4(mdl_mat);
	mat_tri t_projected, t_transformed, t_viewed;
	vec3d cam_ray, f_normal;
	vec3d vn[3];
	vec3d light_ray = light.get_Normal(), light_pos = light.get_Position(); float light_pow = light.get_Power();
	normalise_vec3(light_ray);
	mat_tri clipped[2];
	std::deque<mat_tri> clip_t;
	__m128 _ones = _mm_set1_ps(1.0);
	__m128 _scl = _mm_set_ps(1.0, 1.0, 0.5 * wHeight, 0.5 * wWidth);
	int n_triangles = th_data[id].n_triangles;

	for (int i = 0; i < n_triangles; i++) {
		tri_mat4_mult(th_data[id].tris_list[i], model_mat, t_transformed);
		vec4_mat4_mult(th_data[id].f_normals[i], mdl_mat, f_normal);

		cam_ray.x = t_transformed.mat[0][X] - camera_pos.x;
		cam_ray.y = t_transformed.mat[0][Y] - camera_pos.y;
		cam_ray.z = t_transformed.mat[0][Z] - camera_pos.z;

		if (dot_vec3(f_normal, cam_ray) < 0.0f) {
			
			vec4_mat4_mult(th_data[id].v_normals[i].v1, mdl_mat, vn[0]);
			vec4_mat4_mult(th_data[id].v_normals[i].v2, mdl_mat, vn[1]);
			vec4_mat4_mult(th_data[id].v_normals[i].v3, mdl_mat, vn[2]);
			normalise_vec3(vn[0]); normalise_vec3(vn[1]); normalise_vec3(vn[2]);
			vec3d centriod;
			centriod.x = (t_transformed.mat[0][0] + t_transformed.mat[1][0] + t_transformed.mat[2][0]) / 3.0f;
			centriod.y = (t_transformed.mat[0][1] + t_transformed.mat[1][1] + t_transformed.mat[2][1]) / 3.0f;
			centriod.z = (t_transformed.mat[0][2] + t_transformed.mat[1][2] + t_transformed.mat[2][2]) / 3.0f;
			
			float vi1 = (dot_vec3(vn[0], light_ray) * light_pow) / (12.5663 * sqrd_distance({ t_transformed.mat[0][0], t_transformed.mat[0][1] ,t_transformed.mat[0][2] ,0 }, light_pos));
			float vi2 = (dot_vec3(vn[1], light_ray) * light_pow) / (12.5663 * sqrd_distance({ t_transformed.mat[1][0], t_transformed.mat[1][1] ,t_transformed.mat[1][2] ,0 }, light_pos));
			float vi3 = (dot_vec3(vn[2], light_ray) * light_pow) / (12.5663 * sqrd_distance({ t_transformed.mat[2][0], t_transformed.mat[2][1] ,t_transformed.mat[2][2] ,0 }, light_pos));
			
			float brightness = (dot_vec3(f_normal, light_ray) * light_pow) / (12.5663 * sqrd_distance(centriod, light_pos));
			brightness = max(brightness, 0);
			

			tri_mat4_mult(t_transformed, camera_mat, t_viewed);
			t_viewed.tex_mat[0] = th_data[id].tris_list[i].tex_mat[0];
			t_viewed.tex_mat[1] = th_data[id].tris_list[i].tex_mat[1];
			t_viewed.tex_mat[2] = th_data[id].tris_list[i].tex_mat[2];

			int ntri_clipped = 0;
			ntri_clipped = fnear_Clipping(1.0f, t_viewed, clipped[0], clipped[1]);

			for (int n = 0; n < ntri_clipped; n++) {

				tri_mat4_mult(clipped[n], projection_mat, t_projected);
				t_projected.tex_mat[0] = clipped[n].tex_mat[0];
				t_projected.tex_mat[1] = clipped[n].tex_mat[1];
				t_projected.tex_mat[2] = clipped[n].tex_mat[2];

				_mm_storeu_ps(&t_projected.tex_mat[0].u, _mm_div_ps(_mm_loadu_ps(&t_projected.tex_mat[0].u), _mm_set1_ps(t_projected.mat[0][3])));
				_mm_storeu_ps(&t_projected.tex_mat[1].u, _mm_div_ps(_mm_loadu_ps(&t_projected.tex_mat[1].u), _mm_set1_ps(t_projected.mat[1][3])));
				_mm_storeu_ps(&t_projected.tex_mat[2].u, _mm_div_ps(_mm_loadu_ps(&t_projected.tex_mat[2].u), _mm_set1_ps(t_projected.mat[2][3])));

				_mm_storeu_ps(&t_projected.mat[0][0], _mm_mul_ps(_mm_add_ps(_mm_div_ps(_mm_loadu_ps(&t_projected.mat[0][0]), _mm_set1_ps(t_projected.mat[0][3])), _ones), _scl));
				_mm_storeu_ps(&t_projected.mat[1][0], _mm_mul_ps(_mm_add_ps(_mm_div_ps(_mm_loadu_ps(&t_projected.mat[1][0]), _mm_set1_ps(t_projected.mat[1][3])), _ones), _scl));
				_mm_storeu_ps(&t_projected.mat[2][0], _mm_mul_ps(_mm_add_ps(_mm_div_ps(_mm_loadu_ps(&t_projected.mat[2][0]), _mm_set1_ps(t_projected.mat[2][3])), _ones), _scl));


				clip_t.push_back(t_projected);
				int new_tris = 1;
				for (int p = 0; p < 4; p++) {
					int n_tris = 0;

					while (new_tris > 0) {
						mat_tri test = clip_t.front();
						clip_t.pop_front();
						new_tris--;

						switch (p) {
						case 0: { n_tris = top_Clipping(test, clipped[0], clipped[1]); break; }
						case 1: { n_tris = bottom_Clipping(wHeight - 1.0f, test, clipped[0], clipped[1]); break; }
						case 2: { n_tris = left_Clipping(test, clipped[0], clipped[1]); break; }
						case 3: { n_tris = right_Clipping(wWidth - 1.0f, test, clipped[0], clipped[1]); break; }
						}

						for (int ww = 0; ww < n_tris; ww++)
							clip_t.push_back(clipped[ww]);
					}
					new_tris = clip_t.size();
				}
				int clip_t_size = clip_t.size();
				for (int it = 0; it < clip_t_size; it++) {

					switch (dtype) {
					case WIRE_FRAME: {
						Triangle(
							(int)clip_t[it].mat[0][X], (int)clip_t[it].mat[0][Y],
							(int)clip_t[it].mat[1][X], (int)clip_t[it].mat[1][Y],
							(int)clip_t[it].mat[2][X], (int)clip_t[it].mat[2][Y], { 250,250,250,0 });
						break; }

					case SOLID: {
						Solid_Triangle(
							clip_t[it].mat[0][X], clip_t[it].mat[0][Y], clip_t[it].tex_mat[0].w,
							clip_t[it].mat[1][X], clip_t[it].mat[1][Y], clip_t[it].tex_mat[1].w,
							clip_t[it].mat[2][X], clip_t[it].mat[2][Y], clip_t[it].tex_mat[2].w,
							brightness, { 250,250,250,0 });
						break; }

					case TEXTURED: {
						Textured_Triangle(
							(int)clip_t[it].mat[0][X], clip_t[it].mat[0][Y], clip_t[it].tex_mat[0].u, clip_t[it].tex_mat[0].v, clip_t[it].tex_mat[0].w,
							clip_t[it].mat[1][X], clip_t[it].mat[1][Y], clip_t[it].tex_mat[1].u, clip_t[it].tex_mat[1].v, clip_t[it].tex_mat[1].w,
							clip_t[it].mat[2][X], clip_t[it].mat[2][Y], clip_t[it].tex_mat[2].u, clip_t[it].tex_mat[2].v, clip_t[it].tex_mat[2].w,
							brightness, vi1, vi2, vi3);
						break; }
					}

				}
				clip_t.clear();
			}


		}
	}
}

_3D::mat4x4 _3D::Identity4()
{
	mat4x4 matrix;
	matrix.mat[0][0] = 1.0f;
	matrix.mat[1][1] = 1.0f;
	matrix.mat[2][2] = 1.0f;
	matrix.mat[3][3] = 1.0f;
	return matrix;
}

_3D::mat4x4 _3D::XRotation_mat4(float angle)
{
	mat4x4 matrix;
	matrix.mat[0][0] = 1.0f;
	matrix.mat[1][1] = cosf(angle);
	matrix.mat[1][2] = sinf(angle);
	matrix.mat[2][1] = -sinf(angle);
	matrix.mat[2][2] = cosf(angle);
	matrix.mat[3][3] = 1.0f;
	return matrix;
}

_3D::mat4x4 _3D::YRotation_mat4(float angle)
{
	mat4x4 matrix;
	matrix.mat[0][0] = cosf(angle);
	matrix.mat[0][2] = sinf(angle);
	matrix.mat[2][0] = -sinf(angle);
	matrix.mat[1][1] = 1.0f;
	matrix.mat[2][2] = cosf(angle);
	matrix.mat[3][3] = 1.0f;
	return matrix;
}

_3D::mat4x4 _3D::ZRotation_mat4(float angle)
{
	mat4x4 matrix;
	matrix.mat[0][0] = cosf(angle);
	matrix.mat[0][1] = sinf(angle);
	matrix.mat[1][0] = -sinf(angle);
	matrix.mat[1][1] = cosf(angle);
	matrix.mat[2][2] = 1.0f;
	matrix.mat[3][3] = 1.0f;
	return matrix;
}

_3D::mat4x4 _3D::Translation_mat4(float x, float y, float z)
{
	mat4x4 matrix;
	matrix.mat[0][0] = 1.0f;
	matrix.mat[1][1] = 1.0f;
	matrix.mat[2][2] = 1.0f;
	matrix.mat[3][3] = 1.0f;
	matrix.mat[3][0] = x;
	matrix.mat[3][1] = y;
	matrix.mat[3][2] = z;
	return matrix;
}

_3D::mat4x4 _3D::Projection_mat4(float fov_degrees, float asp_ratio, float fnear, float ffar)
{
	float fov_rad = 1.0f / tanf(fov_degrees * 0.5f / 180.0f * 3.14159f);
	mat4x4 matrix;
	matrix.mat[0][0] = asp_ratio * fov_rad;
	matrix.mat[1][1] = fov_rad;
	matrix.mat[2][2] = ffar / (ffar - fnear);
	matrix.mat[3][2] = (-ffar * fnear) / (ffar - fnear);
	matrix.mat[2][3] = 1.0f;
	matrix.mat[3][3] = 0.0f;
	return matrix;
}

_3D::mat4x4 _3D::operator*(const mat4x4& m1, const mat4x4& m2)
{
	mat4x4 res;
	const __m128 row1 = _mm_load_ps(&m2.mat[0][0]);
	const __m128 row2 = _mm_load_ps(&m2.mat[1][0]);
	const __m128 row3 = _mm_load_ps(&m2.mat[2][0]);
	const __m128 row4 = _mm_load_ps(&m2.mat[3][0]);

	for (int i = 0; i < 4; i++) {
		_mm_store_ps(&res.mat[i][0],
			_mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(m1.mat[i][0]), row1),
				_mm_mul_ps(_mm_set1_ps(m1.mat[i][1]), row2)),
				_mm_add_ps(_mm_mul_ps(_mm_set1_ps(m1.mat[i][2]), row3),
					_mm_mul_ps(_mm_set1_ps(m1.mat[i][3]), row4))));
	}

	return res;
}

inline void _3D::tri_mat4_mult(const mat_tri& A, const mat4x4& B, mat_tri& out)
{
	/*for (int i = 0; i < 3; i++) 
		for(int j = 0; j<4; j++)
			out.mat[i][j] = A.mat[i][0] * B.mat[0][j] + A.mat[i][1] * B.mat[1][j] + A.mat[i][2] * B.mat[2][j] + A.mat[i][3] * B.mat[3][j];*/

	const __m128 row1 = _mm_load_ps(&B.mat[0][0]);
	const __m128 row2 = _mm_load_ps(&B.mat[1][0]);
	const __m128 row3 = _mm_load_ps(&B.mat[2][0]);
	const __m128 row4 = _mm_load_ps(&B.mat[3][0]);

	for (int i = 0; i < 3; i++) {
		_mm_store_ps(&out.mat[i][0],
			_mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(A.mat[i][0]), row1),
			_mm_mul_ps(_mm_set1_ps(A.mat[i][1]), row2)),
			_mm_add_ps(_mm_mul_ps(_mm_set1_ps(A.mat[i][2]), row3),
			_mm_mul_ps(_mm_set1_ps(A.mat[i][3]), row4))));
	}
}

inline void _3D::vec4_mat4_mult(const vec3d& V, const mat4x4& M, vec3d& out)
{
	out.x = V.x * M.mat[0][0] + V.y * M.mat[0][1] + V.z * M.mat[0][2] + V.w * M.mat[0][3];
	out.y = V.x * M.mat[1][0] + V.y * M.mat[1][1] + V.z * M.mat[1][2] + V.w * M.mat[1][3];
	out.z = V.x * M.mat[2][0] + V.y * M.mat[2][1] + V.z * M.mat[2][2] + V.w * M.mat[2][3];
	out.w = V.x * M.mat[3][0] + V.y * M.mat[3][1] + V.z * M.mat[3][2] + V.w * M.mat[3][3];

	/*out.x = V.x * M.mat[0][0] + V.y * M.mat[1][0] + V.z * M.mat[2][0] + V.w * M.mat[3][0];
	out.y = V.x * M.mat[0][1] + V.y * M.mat[1][1] + V.z * M.mat[2][1] + V.w * M.mat[3][1];
	out.z = V.x * M.mat[0][2] + V.y * M.mat[1][2] + V.z * M.mat[2][2] + V.w * M.mat[3][2];
	out.w = V.x * M.mat[0][3] + V.y * M.mat[1][3] + V.z * M.mat[2][3] + V.w * M.mat[3][3];*/
}

void _3D::Transpose_mat4(mat4x4& matrix)
{
	__m128 row1 = _mm_load_ps(&matrix.mat[0][0]);
	__m128 row2 = _mm_load_ps(&matrix.mat[1][0]);
	__m128 row3 = _mm_load_ps(&matrix.mat[2][0]);
	__m128 row4 = _mm_load_ps(&matrix.mat[3][0]);
	_MM_TRANSPOSE4_PS(row1, row2, row3, row4);
	_mm_store_ps(&matrix.mat[0][0], row1);
	_mm_store_ps(&matrix.mat[1][0], row2);
	_mm_store_ps(&matrix.mat[2][0], row3);
	_mm_store_ps(&matrix.mat[3][0], row4);
}

_3D::vec3d _3D::cross_vec3(vec3d& v1, vec3d& v2)
{
	vec3d v;
	v.x = v1.y * v2.z - v1.z * v2.y;
	v.y = v1.z * v2.x - v1.x * v2.z;
	v.z = v1.x * v2.y - v1.y * v2.x;
	return v;
}

_3D::mat4x4 _3D::pointAt_mat(vec3d& pos, vec3d& target, vec3d& up)
{
	vec3d forward = target - pos;
	normalise_vec3(forward);

	vec3d a = forward * (dot_vec3(forward, up));
	vec3d newUp = up - a;
	normalise_vec3(newUp);

	vec3d right = cross_vec3(newUp, forward);

	mat4x4 matrix;
	matrix.mat[0][0] = right.x;	  matrix.mat[0][1] = right.y;	matrix.mat[0][2] = right.z;	  matrix.mat[0][3] = 0.0f;
	matrix.mat[1][0] = newUp.x;	  matrix.mat[1][1] = newUp.y;	matrix.mat[1][2] = newUp.z;	  matrix.mat[1][3] = 0.0f;
	matrix.mat[2][0] = forward.x; matrix.mat[2][1] = forward.y; matrix.mat[2][2] = forward.z; matrix.mat[2][3] = 0.0f;
	matrix.mat[3][0] = pos.x;	  matrix.mat[3][1] = pos.y;     matrix.mat[3][2] = pos.z;	  matrix.mat[3][3] = 1.0f;
	return matrix;
}

_3D::mat4x4 _3D::Camera_mat4(vec3d& camPostion)
{
	vec3d look_dir = { 0,0,1 };
	vec3d up = { 0,1,0 };
	vec3d target = camPostion + look_dir;
	vec3d forward = target - camPostion;
	normalise_vec3(forward);

	vec3d a = forward * (dot_vec3(forward, up));
	vec3d newUp = up - a;
	normalise_vec3(newUp);

	vec3d right = cross_vec3(newUp, forward);

	mat4x4 matrix;
	matrix.mat[0][0] = right.x;	  matrix.mat[0][1] = right.y;	matrix.mat[0][2] = right.z;	  matrix.mat[0][3] = 0.0f;
	matrix.mat[1][0] = newUp.x;	  matrix.mat[1][1] = newUp.y;	matrix.mat[1][2] = newUp.z;	  matrix.mat[1][3] = 0.0f;
	matrix.mat[2][0] = forward.x; matrix.mat[2][1] = forward.y; matrix.mat[2][2] = forward.z; matrix.mat[2][3] = 0.0f;
	matrix.mat[3][0] = camPostion.x;	  matrix.mat[3][1] = camPostion.y;     matrix.mat[3][2] = camPostion.z;	  matrix.mat[3][3] = 1.0f;

	mat4x4 view_mat = rt_mat_inverse(matrix);
	
	return view_mat;
}

_3D::mat4x4 _3D::rt_mat_inverse(mat4x4& m)
{
	mat4x4 matrix;
	matrix.mat[0][0] =   m.mat[0][0];  matrix.mat[0][1] = m.mat[1][0];  matrix.mat[0][2] = m.mat[2][0];  matrix.mat[0][3] = 0.0f;
	matrix.mat[1][0] =   m.mat[0][1];  matrix.mat[1][1] = m.mat[1][1];  matrix.mat[1][2] = m.mat[2][1];  matrix.mat[1][3] = 0.0f;
	matrix.mat[2][0] =   m.mat[0][2];  matrix.mat[2][1] = m.mat[1][2];  matrix.mat[2][2] = m.mat[2][2];  matrix.mat[2][3] = 0.0f;
	matrix.mat[3][0] = -(m.mat[3][0] * matrix.mat[0][0] + m.mat[3][1] * matrix.mat[1][0] + m.mat[3][2] * matrix.mat[2][0]);
	matrix.mat[3][1] = -(m.mat[3][0] * matrix.mat[0][1] + m.mat[3][1] * matrix.mat[1][1] + m.mat[3][2] * matrix.mat[2][1]);
	matrix.mat[3][2] = -(m.mat[3][0] * matrix.mat[0][2] + m.mat[3][1] * matrix.mat[1][2] + m.mat[3][2] * matrix.mat[2][2]);
	matrix.mat[3][3] = 1.0f;
	return matrix;
}

int _3D::left_Clipping(mat_tri& tri_in, mat_tri& tri_out1, mat_tri& tri_out2)
{
	vec_tri in; vec3d tmp_vert;
	memcpy(&in, &tri_in, sizeof(vec_tri));

	vec3d* p_in[3]; int in_count = 0;
	vec3d* p_out[3]; int out_count = 0;
	vec2d* t_in[3]; int in_texCount = 0;
	vec2d* t_out[3]; int out_texCount = 0;

	if (in.vertx[0].x >= 0) { p_in[in_count++] = &in.vertx[0]; t_in[in_texCount++] = &in.tex_vertx[0]; }
	else { p_out[out_count++] = &in.vertx[0]; t_out[out_texCount++] = &in.tex_vertx[0]; }
	if (in.vertx[1].x >= 0) { p_in[in_count++] = &in.vertx[1]; t_in[in_texCount++] = &in.tex_vertx[1]; }
	else { p_out[out_count++] = &in.vertx[1];  t_out[out_texCount++] = &in.tex_vertx[1]; }
	if (in.vertx[2].x >= 0) { p_in[in_count++] = &in.vertx[2]; t_in[in_texCount++] = &in.tex_vertx[2]; }
	else { p_out[out_count++] = &in.vertx[2];  t_out[out_texCount++] = &in.tex_vertx[2]; }

	if (in_count == 0) return 0;
	if (in_count == 3) { memcpy(&tri_out1,&tri_in,sizeof(mat_tri)); return 1; }

	if (in_count == 1 && out_count == 2) {
		tri_out1.color = tri_in.color;
		//tri_out1.color.r = 250; tri_out1.color.g = 0; tri_out1.color.b = 0; tri_out1.color.a = 250;

		__m128 pin = _mm_loadu_ps(&p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[0][X], pin);
		tri_out1.tex_mat[0] = *t_in[0];

		float t = (-p_in[0]->x) / (p_out[0]->x - p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[1][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin), _mm_set1_ps(t), pin));
		__m128 tin = _mm_loadu_ps(&t_in[0]->u);
		_mm_storeu_ps(&tri_out1.tex_mat[1].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));

		t = (-p_in[0]->x) / (p_out[1]->x - p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[1]->x), pin), _mm_set1_ps(t), pin));
		_mm_storeu_ps(&tri_out1.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[1]->u), tin), tin));

		return 1;
	}
	if (in_count == 2 && out_count == 1) {
		tri_out1.color = tri_in.color;
		//tri_out1.color.r = 0; tri_out1.color.g = 250; tri_out1.color.b = 0; tri_out1.color.a = 250;
		tri_out2.color = tri_in.color;
		//tri_out2.color.r = 0; tri_out2.color.g = 0; tri_out2.color.b = 250; tri_out2.color.a = 250;

		__m128 pin = _mm_loadu_ps(&p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[0][X], pin);
		__m128 pin_1 = _mm_loadu_ps(&p_in[1]->x);
		_mm_storeu_ps(&tri_out1.mat[1][X], pin_1);
		tri_out1.tex_mat[0] = *t_in[0];
		tri_out1.tex_mat[1] = *t_in[1];
		
		float t = (-p_in[0]->x) / (p_out[0]->x - p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin), _mm_set1_ps(t), pin));
		__m128 tin = _mm_loadu_ps(&t_in[0]->u);
		_mm_storeu_ps(&tri_out1.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));

		_mm_storeu_ps(&tri_out2.mat[0][X], pin_1);
		tri_out2.tex_mat[0] = *t_in[1];
		_mm_storeu_ps(&tri_out2.mat[1][X], _mm_loadu_ps(&tri_out1.mat[2][X]));
		tri_out2.tex_mat[1] = tri_out1.tex_mat[2];

		t = (-p_in[1]->x) / (p_out[0]->x - p_in[1]->x);
		_mm_storeu_ps(&tri_out2.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin_1), _mm_set1_ps(t), pin_1));
		tin = _mm_loadu_ps(&t_in[1]->u);
		_mm_storeu_ps(&tri_out2.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));
		
		return 2;
	}
}

int _3D::top_Clipping(mat_tri& tri_in, mat_tri& tri_out1, mat_tri& tri_out2)
{
	vec_tri in; vec3d tmp_vert;
	memcpy(&in, &tri_in, sizeof(vec_tri));
	
	vec3d* p_in[3]; int in_count = 0;
	vec3d* p_out[3]; int out_count = 0;
	vec2d* t_in[3]; int in_texCount = 0;
	vec2d* t_out[3]; int out_texCount = 0;

	if (in.vertx[0].y >= 0) { p_in[in_count++] = &in.vertx[0]; t_in[in_texCount++] = &in.tex_vertx[0]; }
	else { p_out[out_count++] = &in.vertx[0]; t_out[out_texCount++] = &in.tex_vertx[0]; }
	if (in.vertx[1].y >= 0) { p_in[in_count++] = &in.vertx[1]; t_in[in_texCount++] = &in.tex_vertx[1]; }
	else { p_out[out_count++] = &in.vertx[1];  t_out[out_texCount++] = &in.tex_vertx[1]; }
	if (in.vertx[2].y >= 0) { p_in[in_count++] = &in.vertx[2]; t_in[in_texCount++] = &in.tex_vertx[2]; }
	else { p_out[out_count++] = &in.vertx[2];  t_out[out_texCount++] = &in.tex_vertx[2]; }

	if (in_count == 0) return 0;
	if (in_count == 3) { memcpy(&tri_out1, &tri_in, sizeof(mat_tri)); return 1; }

	if (in_count == 1 && out_count == 2) {
		tri_out1.color = tri_in.color;
		//tri_out1.color.r = 250; tri_out1.color.g = 0; tri_out1.color.b = 0; tri_out1.color.a = 250;

		__m128 pin = _mm_loadu_ps(&p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[0][X], pin);
		tri_out1.tex_mat[0] = *t_in[0];

		float t = (-p_in[0]->y) / (p_out[0]->y - p_in[0]->y);
		_mm_storeu_ps(&tri_out1.mat[1][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin), _mm_set1_ps(t), pin));
		__m128 tin = _mm_loadu_ps(&t_in[0]->u);
		_mm_storeu_ps(&tri_out1.tex_mat[1].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));

		t = (-p_in[0]->y) / (p_out[1]->y - p_in[0]->y);
		_mm_storeu_ps(&tri_out1.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[1]->x), pin), _mm_set1_ps(t), pin));
		_mm_storeu_ps(&tri_out1.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[1]->u), tin), tin));

		return 1;
	}
	if (in_count == 2 && out_count == 1) {
		tri_out1.color = tri_in.color;
		//tri_out1.color.r = 0; tri_out1.color.g = 250; tri_out1.color.b = 0; tri_out1.color.a = 250;
		tri_out2.color = tri_in.color;
		//tri_out2.color.r = 0; tri_out2.color.g = 0; tri_out2.color.b = 250; tri_out2.color.a = 250;

		__m128 pin = _mm_loadu_ps(&p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[0][X], pin);
		__m128 pin_1 = _mm_loadu_ps(&p_in[1]->x);
		_mm_storeu_ps(&tri_out1.mat[1][X], pin_1);
		tri_out1.tex_mat[0] = *t_in[0];
		tri_out1.tex_mat[1] = *t_in[1];

		float t = (-p_in[0]->y) / (p_out[0]->y - p_in[0]->y);
		_mm_storeu_ps(&tri_out1.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin), _mm_set1_ps(t), pin));
		__m128 tin = _mm_loadu_ps(&t_in[0]->u);
		_mm_storeu_ps(&tri_out1.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));

		_mm_storeu_ps(&tri_out2.mat[0][X], pin_1);
		tri_out2.tex_mat[0] = *t_in[1];
		_mm_storeu_ps(&tri_out2.mat[1][X], _mm_loadu_ps(&tri_out1.mat[2][X]));
		tri_out2.tex_mat[1] = tri_out1.tex_mat[2];

		t = (-p_in[1]->y) / (p_out[0]->y - p_in[1]->y);
		_mm_storeu_ps(&tri_out2.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin_1), _mm_set1_ps(t), pin_1));
		tin = _mm_loadu_ps(&t_in[1]->u);
		_mm_storeu_ps(&tri_out2.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));
		return 2;
	}
}

int _3D::bottom_Clipping(float wht, mat_tri& tri_in, mat_tri& tri_out1, mat_tri& tri_out2)
{
	vec_tri in; vec3d tmp_vert;
	memcpy(&in, &tri_in, sizeof(vec_tri));

	float d0 = -1.0 * in.vertx[0].y + wht;
	float d1 = -1.0 * in.vertx[1].y + wht;
	float d2 = -1.0 * in.vertx[2].y + wht;

	vec3d* p_in[3]; int in_count = 0;
	vec3d* p_out[3]; int out_count = 0;
	vec2d* t_in[3]; int in_texCount = 0;
	vec2d* t_out[3]; int out_texCount = 0;

	if (d0 >= 0) { p_in[in_count++] = &in.vertx[0]; t_in[in_texCount++] = &in.tex_vertx[0]; }
	else { p_out[out_count++] = &in.vertx[0]; t_out[out_texCount++] = &in.tex_vertx[0]; }
	if (d1 >= 0) { p_in[in_count++] = &in.vertx[1]; t_in[in_texCount++] = &in.tex_vertx[1]; }
	else { p_out[out_count++] = &in.vertx[1];  t_out[out_texCount++] = &in.tex_vertx[1]; }
	if (d2 >= 0) { p_in[in_count++] = &in.vertx[2]; t_in[in_texCount++] = &in.tex_vertx[2]; }
	else { p_out[out_count++] = &in.vertx[2];  t_out[out_texCount++] = &in.tex_vertx[2]; }

	if (in_count == 0) return 0;
	if (in_count == 3) { memcpy(&tri_out1, &tri_in, sizeof(mat_tri)); return 1; }

	if (in_count == 1 && out_count == 2) {
		tri_out1.color = tri_in.color;
		//tri_out1.color.r = 250; tri_out1.color.g = 0; tri_out1.color.b = 0; tri_out1.color.a = 250;

		__m128 pin = _mm_loadu_ps(&p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[0][X], pin);
		tri_out1.tex_mat[0] = *t_in[0];

		float t = (-(wht) - (-p_in[0]->y)) / ((-p_out[0]->y) - (-p_in[0]->y));
		_mm_storeu_ps(&tri_out1.mat[1][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin), _mm_set1_ps(t), pin));
		__m128 tin = _mm_loadu_ps(&t_in[0]->u);
		_mm_storeu_ps(&tri_out1.tex_mat[1].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));

		t = (-(wht)-(-p_in[0]->y)) / ((-p_out[1]->y) - (-p_in[0]->y));
		_mm_storeu_ps(&tri_out1.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[1]->x), pin), _mm_set1_ps(t), pin));
		_mm_storeu_ps(&tri_out1.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[1]->u), tin), tin));

		return 1;
	}
	if (in_count == 2 && out_count == 1) {
		tri_out1.color = tri_in.color;
		//tri_out1.color.r = 0; tri_out1.color.g = 250; tri_out1.color.b = 0; tri_out1.color.a = 250;
		tri_out2.color = tri_in.color;
		//tri_out2.color.r = 0; tri_out2.color.g = 0; tri_out2.color.b = 250; tri_out2.color.a = 250;

		__m128 pin = _mm_loadu_ps(&p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[0][X], pin);
		__m128 pin_1 = _mm_loadu_ps(&p_in[1]->x);
		_mm_storeu_ps(&tri_out1.mat[1][X], pin_1);
		tri_out1.tex_mat[0] = *t_in[0];
		tri_out1.tex_mat[1] = *t_in[1];

		float t = (-(wht)-(-p_in[0]->y)) / ((-p_out[0]->y) - (-p_in[0]->y));
		_mm_storeu_ps(&tri_out1.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin), _mm_set1_ps(t), pin));
		__m128 tin = _mm_loadu_ps(&t_in[0]->u);
		_mm_storeu_ps(&tri_out1.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));

		_mm_storeu_ps(&tri_out2.mat[0][X], pin_1);
		tri_out2.tex_mat[0] = *t_in[1];
		_mm_storeu_ps(&tri_out2.mat[1][X], _mm_loadu_ps(&tri_out1.mat[2][X]));
		tri_out2.tex_mat[1] = tri_out1.tex_mat[2];

		t = (-(wht)-(-p_in[1]->y)) / ((-p_out[0]->y) - (-p_in[1]->y));
		_mm_storeu_ps(&tri_out2.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin_1), _mm_set1_ps(t), pin_1));
		tin = _mm_loadu_ps(&t_in[1]->u);
		_mm_storeu_ps(&tri_out2.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));
		return 2;
	}
}

int _3D::right_Clipping(float wwd, mat_tri& tri_in, mat_tri& tri_out1, mat_tri& tri_out2)
{
	vec_tri in; vec3d tmp_vert;
	memcpy(&in, &tri_in, sizeof(vec_tri));

	vec3d* p_in[3]; int in_count = 0;
	vec3d* p_out[3]; int out_count = 0;
	vec2d* t_in[3]; int in_texCount = 0;
	vec2d* t_out[3]; int out_texCount = 0;

	float d0 = -1.0 * in.vertx[0].x + wwd;
	float d1 = -1.0 * in.vertx[1].x + wwd;
	float d2 = -1.0 * in.vertx[2].x + wwd;

	if (d0 >= 0) { p_in[in_count++] = &in.vertx[0]; t_in[in_texCount++] = &in.tex_vertx[0]; }
	else { p_out[out_count++] = &in.vertx[0]; t_out[out_texCount++] = &in.tex_vertx[0]; }
	if (d1 >= 0) { p_in[in_count++] = &in.vertx[1]; t_in[in_texCount++] = &in.tex_vertx[1]; }
	else { p_out[out_count++] = &in.vertx[1];  t_out[out_texCount++] = &in.tex_vertx[1]; }
	if (d2 >= 0) { p_in[in_count++] = &in.vertx[2]; t_in[in_texCount++] = &in.tex_vertx[2]; }
	else { p_out[out_count++] = &in.vertx[2];  t_out[out_texCount++] = &in.tex_vertx[2]; }

	if (in_count == 0) return 0;
	if (in_count == 3) { memcpy(&tri_out1, &tri_in, sizeof(mat_tri)); return 1; }

	if (in_count == 1 && out_count == 2) {
		tri_out1.color = tri_in.color;
		//tri_out1.color.r = 250; tri_out1.color.g = 0; tri_out1.color.b = 0; tri_out1.color.a = 250;

		__m128 pin = _mm_loadu_ps(&p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[0][X], pin);
		tri_out1.tex_mat[0] = *t_in[0];
		float t = (-(wwd)-(-p_in[0]->x)) / ((-p_out[0]->x) - (-p_in[0]->x));
		_mm_storeu_ps(&tri_out1.mat[1][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin), _mm_set1_ps(t), pin));
		__m128 tin = _mm_loadu_ps(&t_in[0]->u);
		_mm_storeu_ps(&tri_out1.tex_mat[1].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));

		t = (-(wwd)-(-p_in[0]->x)) / ((-p_out[1]->x) - (-p_in[0]->x));
		_mm_storeu_ps(&tri_out1.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[1]->x), pin), _mm_set1_ps(t), pin));
		_mm_storeu_ps(&tri_out1.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[1]->u), tin), tin));

		return 1;
	}
	if (in_count == 2 && out_count == 1) {
		tri_out1.color = tri_in.color;
		//tri_out1.color.r = 0; tri_out1.color.g = 250; tri_out1.color.b = 0; tri_out1.color.a = 250;
		tri_out2.color = tri_in.color;
		//tri_out2.color.r = 0; tri_out2.color.g = 0; tri_out2.color.b = 250; tri_out2.color.a = 250;

		__m128 pin = _mm_loadu_ps(&p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[0][X], pin);
		__m128 pin_1 = _mm_loadu_ps(&p_in[1]->x);
		_mm_storeu_ps(&tri_out1.mat[1][X], pin_1);
		tri_out1.tex_mat[0] = *t_in[0];
		tri_out1.tex_mat[1] = *t_in[1];

		float t = (-(wwd)-(-p_in[0]->x)) / ((-p_out[0]->x) - (-p_in[0]->x));
		_mm_storeu_ps(&tri_out1.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin), _mm_set1_ps(t), pin));
		__m128 tin = _mm_loadu_ps(&t_in[0]->u);
		_mm_storeu_ps(&tri_out1.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));

		_mm_storeu_ps(&tri_out2.mat[0][X], pin_1);
		tri_out2.tex_mat[0] = *t_in[1];
		_mm_storeu_ps(&tri_out2.mat[1][X], _mm_loadu_ps(&tri_out1.mat[2][X]));
		tri_out2.tex_mat[1] = tri_out1.tex_mat[2];

		t = (-(wwd)-(-p_in[1]->x)) / ((-p_out[0]->x) - (-p_in[1]->x));
		_mm_storeu_ps(&tri_out2.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin_1), _mm_set1_ps(t), pin_1));
		tin = _mm_loadu_ps(&t_in[1]->u);
		_mm_storeu_ps(&tri_out2.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));
		return 2;
	}
}

int _3D::fnear_Clipping(float fnear, mat_tri& tri_in, mat_tri& tri_out1, mat_tri& tri_out2)
{
	vec_tri in; vec3d tmp_vert;
	memcpy(&in, &tri_in, sizeof(vec_tri));

	vec3d* p_in[3]; int in_count = 0;
	vec3d* p_out[3]; int out_count = 0;
	vec2d* t_in[3]; int in_texCount = 0;
	vec2d* t_out[3]; int out_texCount = 0;

	if ((in.vertx[0].z - fnear) >= 0) { p_in[in_count++] = &in.vertx[0]; t_in[in_texCount++] = &in.tex_vertx[0]; }
	else { p_out[out_count++] = &in.vertx[0]; t_out[out_texCount++] = &in.tex_vertx[0]; }
	if ((in.vertx[1].z - fnear) >= 0) { p_in[in_count++] = &in.vertx[1]; t_in[in_texCount++] = &in.tex_vertx[1]; }
	else { p_out[out_count++] = &in.vertx[1];  t_out[out_texCount++] = &in.tex_vertx[1]; }
	if ((in.vertx[2].z - fnear) >= 0) { p_in[in_count++] = &in.vertx[2]; t_in[in_texCount++] = &in.tex_vertx[2]; }
	else { p_out[out_count++] = &in.vertx[2];  t_out[out_texCount++] = &in.tex_vertx[2]; }

	if (in_count == 0) return 0;
	if (in_count == 3) { memcpy(&tri_out1, &tri_in, sizeof(mat_tri)); return 1; }

	if (in_count == 1 && out_count == 2) {
		tri_out1.color = tri_in.color;
		//tri_out1.color.r = 250; tri_out1.color.g = 0; tri_out1.color.b = 0; tri_out1.color.a = 250;

		__m128 pin = _mm_loadu_ps(&p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[0][X], pin);
		tri_out1.tex_mat[0] = *t_in[0];
		
		float t = (fnear - p_in[0]->z) / (p_out[0]->z - p_in[0]->z);
		_mm_storeu_ps(&tri_out1.mat[1][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin), _mm_set1_ps(t), pin));
		__m128 tin = _mm_loadu_ps(&t_in[0]->u);
		_mm_storeu_ps(&tri_out1.tex_mat[1].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));

		t = (fnear - p_in[0]->z) / (p_out[1]->z - p_in[0]->z);
		_mm_storeu_ps(&tri_out1.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[1]->x), pin), _mm_set1_ps(t), pin));
		_mm_storeu_ps(&tri_out1.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[1]->u), tin), tin));

		return 1;
	}
	if (in_count == 2 && out_count == 1) {
		tri_out1.color = tri_in.color;
		//tri_out1.color.r = 0; tri_out1.color.g = 250; tri_out1.color.b = 0; tri_out1.color.a = 250;
		tri_out2.color = tri_in.color;
		//tri_out2.color.r = 0; tri_out2.color.g = 0; tri_out2.color.b = 250; tri_out2.color.a = 250;

		__m128 pin = _mm_loadu_ps(&p_in[0]->x);
		_mm_storeu_ps(&tri_out1.mat[0][X], pin);
		__m128 pin_1 = _mm_loadu_ps(&p_in[1]->x);
		_mm_storeu_ps(&tri_out1.mat[1][X], pin_1);
		tri_out1.tex_mat[0] = *t_in[0];
		tri_out1.tex_mat[1] = *t_in[1];

		float t = (fnear - p_in[0]->z) / (p_out[0]->z - p_in[0]->z);
		_mm_storeu_ps(&tri_out1.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin), _mm_set1_ps(t), pin));
		__m128 tin = _mm_loadu_ps(&t_in[0]->u);
		_mm_storeu_ps(&tri_out1.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));

		_mm_storeu_ps(&tri_out2.mat[0][X], pin_1);
		tri_out2.tex_mat[0] = *t_in[1];
		_mm_storeu_ps(&tri_out2.mat[1][X], _mm_loadu_ps(&tri_out1.mat[2][X]));
		tri_out2.tex_mat[1] = tri_out1.tex_mat[2];

		t = (fnear - p_in[1]->z) / (p_out[0]->z - p_in[1]->z);
		_mm_storeu_ps(&tri_out2.mat[2][X], _mm_fmadd_ps(_mm_sub_ps(_mm_loadu_ps(&p_out[0]->x), pin_1), _mm_set1_ps(t), pin_1));
		tin = _mm_loadu_ps(&t_in[1]->u);
		_mm_storeu_ps(&tri_out2.tex_mat[2].u, _mm_fmadd_ps(_mm_set1_ps(t), _mm_sub_ps(_mm_loadu_ps(&t_out[0]->u), tin), tin));
		return 2;
	}
}

bool mesh3d::load_obj(const char* file, bool isTextured)
{
	std::vector<vec_tri> tris;
	std::ifstream object(file);
	if (!object.is_open())return false;
	num_triangles = 0;

	std::vector<vec3d> verts;
	std::vector<vec2d> texs;
	std::vector<vec3d> f_indx;
	
	char line[128];
	while (!object.eof())
	{
		object.getline(line, 128);

		std::strstream s;
		s << line;

		char junk;

		if (line[0] == 'v')
		{
			if (line[1] == 't')
			{
				vec2d v;
				s >> junk >> junk >> v.u >> v.v;
				texs.push_back(v);
			}
			else
			{
				vec3d v;
				s >> junk >> v.x >> v.y >> v.z;
				verts.push_back(v);
			}
		}

		if (!isTextured)
		{
			if (line[0] == 'f')
			{
				int f[3];
				s >> junk >> f[0] >> f[1] >> f[2];
				tris.push_back({ verts[f[0] - 1], verts[f[1] - 1], verts[f[2] - 1] });
				f_indx.push_back({ (float)f[0] - 1, (float)f[1] - 1, (float)f[2] - 1, 0 });
				num_triangles++;
			}
		}
		else
		{
			if (line[0] == 'f')
			{
				s >> junk;
				std::string tokens[6];
				int nTokenCount = -1;

				while (!s.eof())
				{
					char c = s.get();
					if (c == ' ' || c == '/')
						nTokenCount++;
					else
						tokens[nTokenCount].append(1, c);
				}
				tokens[nTokenCount].pop_back();

				tris.push_back({ verts[stoi(tokens[0]) - 1], verts[stoi(tokens[2]) - 1], verts[stoi(tokens[4]) - 1],
					texs[stoi(tokens[1]) - 1], texs[stoi(tokens[3]) - 1], texs[stoi(tokens[5]) - 1] });
				f_indx.push_back({ (float)stoi(tokens[0]) - 1, (float)stoi(tokens[2]) - 1, (float)stoi(tokens[4]) - 1, 0 });
				num_triangles++;
			}

		}
	}

	vec3d* v_normals = new vec3d[verts.size()];
	memset(v_normals, 0, sizeof(vec3d) * verts.size());
	for (int i = 0; i < num_triangles; i++) {
		vec_tri tri = tris[i];
		vec3d l1, l2, normal;
		l1 = tri.vertx[1] - tri.vertx[0];
		l2 = tri.vertx[2] - tri.vertx[0];
		normal = cross_vec3(l1, l2);
		normalise_vec3(normal);
		v_normals[(int)f_indx[i].x] = v_normals[(int)f_indx[i].x] + normal;
		v_normals[(int)f_indx[i].y] = v_normals[(int)f_indx[i].y] + normal;
		v_normals[(int)f_indx[i].z] = v_normals[(int)f_indx[i].z] + normal;
		normalise_vec3(v_normals[(int)f_indx[i].x ]);
		normalise_vec3(v_normals[(int)f_indx[i].y ]);
		normalise_vec3(v_normals[(int)f_indx[i].z ]);
	}

	triangles_list = new mat_tri[num_triangles];
	face_normals = new vec3d[num_triangles];
	vertex_normals = new vec3dx3[num_triangles];

	for (int n = 0; n < num_triangles; n++) {
		vec_tri triangle = tris[n]; vec3d l1, l2, normal;
		for (int i = 0; i < 3; i++) {
			triangles_list[n].mat[i][X] = triangle.vertx[i].x;
			triangles_list[n].mat[i][Y] = triangle.vertx[i].y;
			triangles_list[n].mat[i][Z] = triangle.vertx[i].z;
			triangles_list[n].mat[i][W] = triangle.vertx[i].w;

			triangles_list[n].tex_mat[i] = triangle.tex_vertx[i];
		}

		l1 = triangle.vertx[1] - triangle.vertx[0];
		l2 = triangle.vertx[2] - triangle.vertx[0];
		normal = cross_vec3(l1, l2);
		normalise_vec3(normal);
		face_normals[n] = normal; face_normals[n].w = 0;
		vertex_normals[n] = { v_normals[(int)f_indx[n].x], v_normals[(int)f_indx[n].y] ,v_normals[(int)f_indx[n].z] };
	}

	tris.clear();
	tris.shrink_to_fit();
	object.close();
	delete[] v_normals;

	return true;
}

bool Texture::load_image_data(const char* jpeg_path)
{
	FILE* pFile = fopen(jpeg_path, "rb");
	if (!pFile)
		return false;

	jpeg_decompress_struct img_info;
	jpeg_error_mgr err;
	JSAMPARRAY buffer;

	img_info.err = jpeg_std_error(&err);

	jpeg_create_decompress(&img_info);
	jpeg_stdio_src(&img_info, pFile);
	jpeg_read_header(&img_info, TRUE);
	jpeg_start_decompress(&img_info);

	buffer = (*img_info.mem->alloc_sarray)
		((j_common_ptr)&img_info, JPOOL_IMAGE, img_info.output_width * img_info.output_components, 1);

	i_width = img_info.output_width;
	i_height = img_info.output_height;
	data = new bgra8[i_width * i_height];
	memset(data, 0, sizeof(bgra8) * i_width * i_height);

	int j = 0;
	while (img_info.output_scanline < img_info.output_height)
	{
		(void)jpeg_read_scanlines(&img_info, buffer, 1);

		// get the pointer to the row:
		unsigned char* pixel_row = (unsigned char*)(buffer[0]);
		// iterate over the pixels:
		for (int i = 0; i < img_info.output_width; i++)
		{
			int pix_id = j * i_width + i;
			data[pix_id].r = *pixel_row++;
			data[pix_id].g = *pixel_row++;
			data[pix_id].b = *pixel_row++;
			data[pix_id].a = 250;
		}
		j++;
	}

	jpeg_finish_decompress(&img_info);
	jpeg_destroy_decompress(&img_info);
	fclose(pFile);
	return true;
}
