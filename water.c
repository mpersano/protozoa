/*
 * credit where due: parts of this are based on the water explanation
 * by Laurent Lardinois
 * original article: http://www.codeproject.com/KB/openGL/dsaqua.aspx
 *
 * the effect is quite old, though
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <GL/glew.h>
#ifdef WIN32
#include <GL/wglew.h>
#endif

#include "in_game.h"
#include "settings.h"
#include "vector.h"
#include "common.h"
#include "image.h"
#include "gl_util.h"
#include "arena.h"
#include "background.h"
#include "water.h"

#define RENDER_TO_TEXTURE

enum {
	WATER_GRID_SIZE = 64,
};

#define WATER_QUAD_RADIUS 7.2f /* world coordinate (half) size of water grid */

static GLuint backg_texture_id;
static int env_texture_id;
static GLuint fbo_id;
static GLuint depth_rb_id;

#ifdef WIN32
static HPBUFFERARB pbuffer;
static HDC pbuffer_hdc, prev_hdc;
static HGLRC pbuffer_wgl_context, prev_wgl_context;
#endif

static float water_grid[2][WATER_GRID_SIZE][WATER_GRID_SIZE];
static float smoothed_water[WATER_GRID_SIZE][WATER_GRID_SIZE];
static float height_map[2*WATER_GRID_SIZE][2*WATER_GRID_SIZE];

static vector3 normal_map[2*WATER_GRID_SIZE][2*WATER_GRID_SIZE];
static vector2 backg_uv_map[2*WATER_GRID_SIZE][2*WATER_GRID_SIZE];
static vector2 env_uv_map[2*WATER_GRID_SIZE][2*WATER_GRID_SIZE];

static int cur_grid;

static void
update_water_grid(void)
{
	float u, v, du, dv;
	int i, j, k;
	int cur, prev;

	cur = cur_grid;
	prev = cur ^ 1;

	cur_grid ^= 1;

	/* discretized differential equation */

	for (i = 1; i < WATER_GRID_SIZE - 1; i++) {
		for (j = 1; j < WATER_GRID_SIZE - 1; j++) {
			float v = 
			  ((water_grid[prev][i - 1][j] +
			    water_grid[prev][i + 1][j] +
			    water_grid[prev][i][j - 1] +
			    water_grid[prev][i][j + 1]) / 2) -
			  water_grid[cur][i][j];
			v *= .85;
#define EPSILON 1e-2
			if (v > -EPSILON && v < EPSILON)
				v = 0.f;
			water_grid[cur][i][j] = v;
		}
	}

	/* lowpass */

	for (i = 0; i < WATER_GRID_SIZE - 1; i++) {
		for (j = 0; j < WATER_GRID_SIZE - 1; j++) {
			smoothed_water[i][j] =
			  (3*water_grid[cur][i][j] +
			   2*water_grid[cur][i + 1][j] +
			   2*water_grid[cur][i][j + 1] +
			   water_grid[cur][i + 1][j + 1]) / 8;
		}
	}

	for (k = 0; k < 3; k++) {
		for (i = 0; i < WATER_GRID_SIZE - 1; i++) {
			for (j = 0; j < WATER_GRID_SIZE - 1; j++) {
				smoothed_water[i][j] =
					(3*smoothed_water[i][j] +
					 2*smoothed_water[i + 1][j] +
					 2*smoothed_water[i][j + 1] +
					 smoothed_water[i + 1][j + 1]) / 8;
			}
		}
	}

	/* height map */

	for (i = 1; i < WATER_GRID_SIZE; i++) {
		for (j = 1; j < WATER_GRID_SIZE; j++) {
			float h = .01f*smoothed_water[i][j];
			if (h < 0.f)
				h = 0.f;
			height_map[2*(i - 1)][2*(j - 1)] = h;
		}
	}

	/* expand, filtering further */

	for (i = 0; i < WATER_GRID_SIZE; i++) {
		for (j = 0; j < WATER_GRID_SIZE; j++) {
			height_map[i*2][j*2 + 1] =
				(height_map[i*2][j*2] +
				 height_map[i*2][j*2 + 2])/2;
		}
	}

	for (i = 0; i < WATER_GRID_SIZE; i++) {
		for (j = 0; j < WATER_GRID_SIZE*2; j++) {
			height_map[i*2 + 1][j] =
			  (height_map[i*2][j] + height_map[i*2 + 2][j])/2;
		}
	}

	/* initialize normal map */

	for (i = 0; i < 2*WATER_GRID_SIZE; i++) {
		for (j = 0; j < 2*WATER_GRID_SIZE; j++) {
			normal_map[i][j].x = 
			  height_map[i][j] - height_map[i + 1][j];
			normal_map[i][j].y = 
			  height_map[i][j] - height_map[i][j + 1];
		}
	}

	/* uv map */

	du = 1.f/(2*WATER_GRID_SIZE);
	dv = 1.f/(2*WATER_GRID_SIZE);
	
	u = 0.f;

	for (i = 1; i < 2*WATER_GRID_SIZE - 1; i++) {
		v = 0.f;

		for (j = 1; j < 2*WATER_GRID_SIZE - 1; j++) {
			vector3 sn; /* vertex normal */
			float l;

			sn.x = normal_map[i - 1][j].x + normal_map[i + 1][j].x +
			  normal_map[i][j - 1].x + normal_map[i][j + 1].x;
			sn.y = normal_map[i - 1][j].y + normal_map[i + 1][j].y +
			  normal_map[i][j - 1].y + normal_map[i][j + 1].y;

			l = sqrtf(sn.x*sn.x + sn.y*sn.y + .16f /* .0016f */);

			sn.x /= l;
			sn.y /= l;
			/* sn.z = .04f/l; */

			v += dv;

			/* fake refraction */
			backg_uv_map[i][j].x = u + .3f*sn.x; /* ??? was .05f */
			backg_uv_map[i][j].y = v + .3f*sn.y;

			/* reflection */
			env_uv_map[i][j].x = .5f + .45f*sn.x;
			env_uv_map[i][j].y = .5f + .45f*sn.y;
		}

		u += du;
	}
}

void
reset_water(void)
{
	memset(water_grid, 0, sizeof water_grid);
	cur_grid = 0;
}

static void
draw_water_grid(float texture_max_u, float texture_max_v)
{
	int i, j;
	float x, y, dx, dy;
	float du, dv;
	const struct rgba *color = settings.static_settings->water_color;
	const int use_pbuffer = settings.static_settings->use_pbuffer;
	const int use_pbuffer_render_texture = settings.static_settings->use_pbuffer_render_texture;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glEnable(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, backg_texture_id);
	/* glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); */
	if (use_pbuffer && use_pbuffer_render_texture) {
#ifdef WIN32
		wglBindTexImageARB(pbuffer, WGL_FRONT_LEFT_ARB);
#else
		assert(0);
#endif
	}

	glActiveTextureARB(GL_TEXTURE1_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gl_texture_id(env_texture_id));
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

	glDisable(GL_BLEND);
	glDisable(GL_LIGHTING);
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);

	glColor4f(color->r, color->g, color->b, color->a);

	dx = 2.f*WATER_QUAD_RADIUS/(2*WATER_GRID_SIZE);
	dy = 2.f*WATER_QUAD_RADIUS/(2*WATER_GRID_SIZE);

	du = texture_max_u;
	dv = texture_max_v;

	x = -WATER_QUAD_RADIUS;

	for (i = 0; i < 2*WATER_GRID_SIZE - 1; i++) {
		y = -WATER_QUAD_RADIUS;

		glBegin(GL_TRIANGLE_STRIP);

		for (j = 0; j < 2*WATER_GRID_SIZE - 1; j++) {
			glMultiTexCoord2fARB(GL_TEXTURE0_ARB,
			  du*backg_uv_map[i][j].x, dv*backg_uv_map[i][j].y);
			glMultiTexCoord2fARB(GL_TEXTURE1_ARB,
			  env_uv_map[i][j].x, env_uv_map[i][j].y);
			glVertex2f(x, y);

			glMultiTexCoord2fARB(GL_TEXTURE0_ARB,
			  du*backg_uv_map[i + 1][j].x,
			  dv*backg_uv_map[i + 1][j].y);
			glMultiTexCoord2fARB(GL_TEXTURE1_ARB,
			  env_uv_map[i + 1][j].x,
			  env_uv_map[i + 1][j].y);
			glVertex2f(x + dx, y);

			y += dy;
		}

		glEnd();

		x += dx;
	}

	if (use_pbuffer && use_pbuffer_render_texture) {
#ifdef WIN32
		wglReleaseTexImageARB(pbuffer, WGL_FRONT_LEFT_ARB);
#else
		assert(0);
#endif
	}

	glPopAttrib();
}

extern void set_modelview_rotation(void);

static inline int
backg_texture_size(void)
{
	static const int detail_to_size[] = { 128, 256, 512 };
	return detail_to_size[settings.water_detail];
}

static void
render_background_to_texture(void)
{
	GLfloat proj[16];
	const float z_near = .1f;
	const float z_far = 5000.f;
	float f, k, w, fx, fy;
	const int use_fbo = settings.static_settings->use_fbo;
	const int use_pbuffer = settings.static_settings->use_pbuffer;
	const int use_pbuffer_render_texture = settings.static_settings->use_pbuffer_render_texture;
	extern void set_light_pos(void);

	if (use_fbo) {
		/* set framebuffer as render target */
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id);
	} else if (use_pbuffer) {
#ifdef WIN32
		if (!wglMakeCurrent(pbuffer_hdc, pbuffer_wgl_context))
			panic("wglMakeCurrent failed");
#else
		assert(0);
#endif
	}

	glViewport(0, 0, backg_texture_size(), backg_texture_size());

	/* set up a perspective matrix so that the projected water quad
	 * fills the whole viewport. fixes field of view to that of
	 * game camera position. */
	/* this is a HACK, but seems to work pretty well! */

	k = (z_far + z_near)/(z_near - z_far);
	w = (2*z_far*z_near)/(z_near - z_far);

	f = -gc.eye.z/WATER_QUAD_RADIUS;
	fx = gc.eye.x/WATER_QUAD_RADIUS;
	fy = gc.eye.y/WATER_QUAD_RADIUS;

	proj[ 0] =  f; proj[ 1] =  0; proj[ 2] =  0; proj[ 3] =  0;
	proj[ 4] =  0; proj[ 5] =  f; proj[ 6] =  0; proj[ 7] =  0;
	proj[ 8] = fx; proj[ 9] = fy; proj[10] =  k; proj[11] = -1;
	proj[12] =  0; proj[13] =  0; proj[14] =  w; proj[15] =  0;

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
        glMultMatrixf(proj);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(gc.eye.x, gc.eye.y, gc.eye.z);
	set_light_pos();
	set_modelview_rotation();

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	draw_background(gc.cur_level, 0, 1);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	if (use_fbo) {
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	} else if (!use_pbuffer_render_texture) {
		/* copy to texture */
		glBindTexture(GL_TEXTURE_2D, backg_texture_id);

		glCopyTexSubImage2D(GL_TEXTURE_2D,
		  0, 
		  0, 0,
		  0, 0,
		  backg_texture_size(), backg_texture_size());
	}

	if (use_pbuffer) {
#ifdef WIN32
		if (!wglMakeCurrent(prev_hdc, prev_wgl_context))
			panic("wglMakeCurrent failed");
#else
		assert(0);
#endif
	}

	glViewport(0, 0, window_width, window_height);
}

void
draw_water()
{
	float get_arena_alpha(void);
	float texture_max_u, texture_max_v;

	render_background_to_texture();

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glPushMatrix();
	set_modelview_rotation();
	draw_background(gc.cur_level, get_arena_alpha(), 1);
	glPopMatrix();

#if 1
	texture_max_u = texture_max_v = 1.f;

	/* arena stencil */
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glEnable(GL_STENCIL_TEST);
	glClear(GL_STENCIL_BUFFER_BIT);

	glStencilFunc(GL_ALWAYS, 1, 1);
	glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDisable(GL_CULL_FACE);

	draw_filled_arena(cur_arena);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_EQUAL, 1, 1);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	/* finally, the water */
	draw_water_grid(texture_max_u, texture_max_v);

	glPopAttrib();
#else
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, backg_texture_id);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(-WATER_QUAD_RADIUS, -WATER_QUAD_RADIUS);
	glTexCoord2f(1, 0);
	glVertex2f(WATER_QUAD_RADIUS, -WATER_QUAD_RADIUS);
	glTexCoord2f(1, 1);
	glVertex2f(WATER_QUAD_RADIUS, WATER_QUAD_RADIUS);
	glTexCoord2f(0, 1);
	glVertex2f(-WATER_QUAD_RADIUS, WATER_QUAD_RADIUS);
	glEnd();
#endif
}

void
update_water(void)
{
	update_water_grid();
}

static void
initialize_backg_texture(void)
{
	if (settings.static_settings->use_fbo) {
		glGenFramebuffersEXT(1, &fbo_id);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id);

		glGenRenderbuffersEXT(1, &depth_rb_id);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth_rb_id);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
		  GL_DEPTH_COMPONENT24,
		  backg_texture_size(), backg_texture_size());
	}

	glGenTextures(1, &backg_texture_id);
	glBindTexture(GL_TEXTURE_2D, backg_texture_id);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
	  backg_texture_size(), backg_texture_size(),
	  0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	if (settings.static_settings->use_fbo) {
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
		  GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D,
		  backg_texture_id, 0);

		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
		  GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT,
		  depth_rb_id);

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	}
}

static void
initialize_env_texture(void)
{
	if (!env_texture_id)
		env_texture_id = png_to_texture("envmap.png");
}

static void
tear_down_fbo(void)
{
	glDeleteFramebuffersEXT(1, &fbo_id);
	glDeleteRenderbuffersEXT(1, &depth_rb_id);
}

static void
tear_down_pbuffer(void)
{
#ifdef WIN32
	wglDeleteContext(pbuffer_wgl_context);

	wglReleasePbufferDCARB(pbuffer, pbuffer_hdc);

	wglDestroyPbufferARB(pbuffer);
#else
	assert(0);
#endif
}

void
tear_down_water(void)
{
	if (backg_texture_id)
		glDeleteTextures(1, &backg_texture_id);

	if (settings.static_settings->use_fbo)
		tear_down_fbo();

	if (settings.static_settings->use_pbuffer)
		tear_down_pbuffer();
}

#ifdef WIN32
#include <windows.h>
#endif

static void
initialize_pbuffer(void)
{
#ifdef WIN32
	static int attribs_render_texture[] = {
		WGL_DRAW_TO_PBUFFER_ARB, 1,
		WGL_SUPPORT_OPENGL_ARB, 1,
		WGL_RED_BITS_ARB, 8,
		WGL_GREEN_BITS_ARB, 8,
		WGL_BLUE_BITS_ARB, 8,
		WGL_ALPHA_BITS_ARB, 8,
		WGL_DEPTH_BITS_ARB, 24,
		WGL_BIND_TO_TEXTURE_RGBA_ARB, 1,
		0
	};
	static int attribs_no_render_texture[] = {
		WGL_DRAW_TO_PBUFFER_ARB, 1,
		WGL_SUPPORT_OPENGL_ARB, 1,
		WGL_RED_BITS_ARB, 8,
		WGL_GREEN_BITS_ARB, 8,
		WGL_BLUE_BITS_ARB, 8,
		WGL_ALPHA_BITS_ARB, 8,
		WGL_DEPTH_BITS_ARB, 24,
		0
	};


	static int pbuffer_attribs_render_texture[] = {
		WGL_TEXTURE_FORMAT_ARB, WGL_TEXTURE_RGBA_ARB,
		WGL_TEXTURE_TARGET_ARB, WGL_TEXTURE_2D_ARB,
		0
	};
	static int pbuffer_attribs_no_render_texture[] = {
		0
	};

	int count, pixel_format;
	int width, height;
	const int rt = settings.static_settings->use_pbuffer_render_texture;

	prev_hdc = wglGetCurrentDC();
	prev_wgl_context = wglGetCurrentContext();

	wglChoosePixelFormatARB(prev_hdc, rt ? attribs_render_texture : attribs_no_render_texture, NULL, 1,
	  &pixel_format, &count);

	pbuffer = wglCreatePbufferARB(prev_hdc, pixel_format,
	  backg_texture_size(), backg_texture_size(),
	  rt ? pbuffer_attribs_render_texture : pbuffer_attribs_no_render_texture);
	if (!pbuffer)
		panic("wglCreatePbufferARB failed");

	if (!(pbuffer_hdc = wglGetPbufferDCARB(pbuffer)))
		panic("wglGetPbufferDCARB failed");

	if (!(pbuffer_wgl_context = wglCreateContext(pbuffer_hdc)))
		panic("wglCreateContext failed");

	fprintf(stderr, "pbuffer wgl context: %p\n", pbuffer_hdc);

	if (!wglShareLists(prev_wgl_context, pbuffer_wgl_context))
		panic("wglShareLists failed");

	wglQueryPbufferARB(pbuffer, WGL_PBUFFER_WIDTH_ARB, &width);
	wglQueryPbufferARB(pbuffer, WGL_PBUFFER_HEIGHT_ARB, &height);

	fprintf(stderr, "pbuffer created: %dx%d\n", width, height);
#else
	assert(0);
#endif
}

void
initialize_water(void)
{
	initialize_env_texture();
	initialize_backg_texture();
	if (settings.static_settings->use_pbuffer)
		initialize_pbuffer();
}

void
perturb_water(const vector2 *pos, float amount)
{
	float s = (pos->x - (-WATER_QUAD_RADIUS))/(2.f*WATER_QUAD_RADIUS);
	float t = (pos->y - (-WATER_QUAD_RADIUS))/(2.f*WATER_QUAD_RADIUS);

	if (s >= 0.f && s < 1.f && t >= 0.f && t < 1.f) {
		water_grid[cur_grid][(int)(s*WATER_GRID_SIZE) + 4]
		  [(int)(t*WATER_GRID_SIZE) + 4] -= amount;
	}
}

void
serialize_water(FILE *out)
{
	float min, max, r;
	int i, j;

	min = max = water_grid[cur_grid][0][0];

	for (i = 0; i < WATER_GRID_SIZE; i++) {
		for (j = 0; j < WATER_GRID_SIZE; j++) {
			if (water_grid[cur_grid][i][j] < min)
				min = water_grid[cur_grid][i][j];
			if (water_grid[cur_grid][i][j] > max)
				max = water_grid[cur_grid][i][j];
		}
	}

	fwrite(&min, sizeof min, 1, out);
	fwrite(&max, sizeof max, 1, out);

	r = max - min;

	for (i = 0; i < WATER_GRID_SIZE; i++) {
		for (j = 0; j < WATER_GRID_SIZE; j++) {
			unsigned b = 255*(water_grid[cur_grid][i][j] - min)/r;
			fputc(b, out);
		}
	}
}

int
unserialize_water(const char *data)
{
	float min, max, r;
	int i, j;
	const unsigned char *q = (unsigned char *)data;

	memcpy(&min, q, sizeof min);
	q += sizeof min;

	memcpy(&max, q, sizeof max);
	q += sizeof max;

	r = max - min;

	for (i = 0; i < WATER_GRID_SIZE; i++) {
		for (j = 0; j < WATER_GRID_SIZE; j++) {
			unsigned b = *q++;
			water_grid[0][i][j] = min + (r*b)/255;
		}
	}

	cur_grid = 0;

	update_water_grid();

	return q - (unsigned char *)data;
}
