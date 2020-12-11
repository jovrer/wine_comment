/* Support for window-specific OpenGL extensions.
 *
 * Copyright (c) 2004 Lionel Ulmer
 * Copyright (c) 2005 Alex Woods
 * Copyright (c) 2005 Raphael Junqueira
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winerror.h"

#include "gdi.h"
#include "wgl.h"
#include "wgl_ext.h"
#include "opengl_ext.h"
#include "wine/library.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(opengl);


/* x11drv GDI escapes */
#define X11DRV_ESCAPE 6789
enum x11drv_escape_codes
{
    X11DRV_GET_DISPLAY,   /* get X11 display for a DC */
    X11DRV_GET_DRAWABLE,  /* get current drawable for a DC */
    X11DRV_GET_FONT,      /* get current X font for a DC */
    X11DRV_SET_DRAWABLE,     /* set current drawable for a DC */
};
struct x11drv_escape_set_drawable
{
    enum x11drv_escape_codes code;         /* escape code (X11DRV_SET_DRAWABLE) */
    Drawable                 drawable;     /* X drawable */
    int                      mode;         /* ClipByChildren or IncludeInferiors */
    POINT                    org;          /* origin of DC relative to drawable */
    POINT                    drawable_org; /* origin of drawable relative to screen */
};

/* retrieve the X display to use on a given DC */
inline static Display *get_display( HDC hdc )
{
    Display *display;
    enum x11drv_escape_codes escape = X11DRV_GET_DISPLAY;

    if (!ExtEscape( hdc, X11DRV_ESCAPE, sizeof(escape), (LPCSTR)&escape,
                    sizeof(display), (LPSTR)&display )) display = NULL;
    return display;
}
inline static void set_drawable( HDC hdc, Drawable drawable )
{
    struct x11drv_escape_set_drawable escape;

    escape.code = X11DRV_SET_DRAWABLE;
    escape.drawable = drawable;
    escape.mode = IncludeInferiors;
    escape.org.x = escape.org.y = 0;
    escape.drawable_org.x = escape.drawable_org.y = 0;

    ExtEscape( hdc, X11DRV_ESCAPE, sizeof(escape), (LPCSTR)&escape, 0, NULL );
}

/* Some WGL extensions... */
static const char *WGL_extensions_base = "WGL_ARB_extensions_string WGL_EXT_extensions_string";
static char *WGL_extensions = NULL;

/**
 * Extensions-query functions 
 *
 * @TODO: use a struct to handle parameters 
 */
BOOL query_function_make_current_read(glXGetProcAddressARB_t proc, const char *gl_version, const char *gl_extensions, 
				      const char* glx_version, const char *glx_extensions,
				      const char *server_glx_extensions, const char *client_glx_extensions)
{
  return 0 <= strcmp("1.3", glx_version);
}

BOOL query_function_multisample(glXGetProcAddressARB_t proc, const char *gl_version, const char *gl_extensions, 
				const char* glx_version, const char *glx_extensions,
				const char *server_glx_extensions, const char *client_glx_extensions)
{
  return NULL != strstr(glx_extensions, "GLX_ARB_multisample");
}

BOOL query_function_pbuffer(glXGetProcAddressARB_t proc, const char *gl_version, const char *gl_extensions, 
			    const char* glx_version, const char *glx_extensions,
			    const char *server_glx_extensions, const char *client_glx_extensions)
{
  TRACE("gl_version is: \"%s\"\n", gl_version);
  TRACE("glx_exts is: \"%s\"\n", glx_extensions);

  return 0 <= strcmp("1.3", glx_version) || NULL != strstr(glx_extensions, "GLX_SGIX_pbuffer");
}

BOOL query_function_pixel_format(glXGetProcAddressARB_t proc, const char *gl_version, const char *gl_extensions, 
				 const char* glx_version, const char *glx_extensions,
				 const char *server_glx_extensions, const char *client_glx_extensions)
{
  return TRUE;
}

/** GLX_ARB_render_texture */
/**
 * http://oss.sgi.com/projects/ogl-sample/registry/ARB/wgl_render_texture.txt
 * ~/tmp/ogl/ogl_offscreen_rendering_3
 */
Bool (*p_glXBindTexImageARB)(Display *dpy, GLXPbuffer pbuffer, int buffer);
Bool (*p_glXReleaseTexImageARB)(Display *dpy, GLXPbuffer pbuffer, int buffer);
Bool (*p_glXDrawableAttribARB)(Display *dpy, GLXDrawable draw, const int *attribList);
int  use_render_texture_emulation = 0;
BOOL query_function_render_texture(glXGetProcAddressARB_t proc, const char *gl_version, const char *gl_extensions, 
				   const char* glx_version, const char *glx_extensions,
				   const char *server_glx_extensions, const char *client_glx_extensions)
{
  BOOL bTest = (0 <= strcmp("1.3", glx_version) || NULL != strstr(glx_extensions, "GLX_SGIX_pbuffer"));
  if (bTest) {
    if (NULL != strstr(glx_extensions, "GLX_ARB_render_texture")) {
      p_glXBindTexImageARB = proc( (const GLubyte *) "glXBindTexImageARB");
      p_glXReleaseTexImageARB = proc( (const GLubyte *) "glXReleaseTexImageARB");
      p_glXDrawableAttribARB = proc( (const GLubyte *) "glXDrawableAttribARB");
      bTest = (NULL != p_glXBindTexImageARB && NULL != p_glXReleaseTexImageARB && NULL != p_glXDrawableAttribARB);
    } else {
      use_render_texture_emulation = 0;
    }
  }
  return bTest;
}

int (*p_glXSwapIntervalSGI)(int);
BOOL query_function_swap_control(glXGetProcAddressARB_t proc, const char *gl_version, const char *gl_extensions, 
				 const char* glx_version, const char *glx_extensions,
				 const char *server_glx_extensions, const char *client_glx_extensions)
{
  BOOL bTest = (0 <= strcmp("1.3", glx_version) || NULL != strstr(glx_extensions, "GLX_SGI_swap_control"));
  if (bTest) {
    p_glXSwapIntervalSGI = proc( (const GLubyte *) "glXSwapIntervalSGI");
    bTest = (NULL != p_glXSwapIntervalSGI);
  }
  return bTest;
}

/***********************************************************************
 *              wglGetExtensionsStringEXT(OPENGL32.@)
 */
const char * WINAPI wglGetExtensionsStringEXT(void) {
    TRACE("() returning \"%s\"\n", WGL_extensions);

    return WGL_extensions;
}

/***********************************************************************
 *              wglGetExtensionsStringARB(OPENGL32.@)
 */
const char * WINAPI wglGetExtensionsStringARB(HDC hdc) {
    TRACE("() returning \"%s\"\n", WGL_extensions);

    return WGL_extensions;    
}

static int swap_interval = 1;

/***********************************************************************
 *              wglSwapIntervalEXT(OPENGL32.@)
 */
BOOL WINAPI wglSwapIntervalEXT(int interval) {
  TRACE("(%d)\n", interval);
  swap_interval = interval;
  if (NULL != p_glXSwapIntervalSGI) {
    return 0 == p_glXSwapIntervalSGI(interval);
  }
  WARN("(): GLX_SGI_swap_control extension seems not supported \n");
  return TRUE;
}

/***********************************************************************
 *              wglGetSwapIntervalEXT(OPENGL32.@)
 */
int WINAPI wglGetSwapIntervalEXT(VOID) {
    FIXME("(),stub!\n");
    return swap_interval;
}

typedef struct wine_glpbuffer {
  Drawable   drawable;
  Display*   display;
  int        pixelFormat;
  int        width;
  int        height;
  int*       attribList;
  HDC        hdc;

  int        use_render_texture;
  GLuint     texture_target;
  GLuint     texture_bind_target;
  GLuint     texture;
  int        texture_level;
} Wine_GLPBuffer;

#define PUSH1(attribs,att)        attribs[nAttribs++] = (att); 
#define PUSH2(attribs,att,value)  attribs[nAttribs++] = (att); attribs[nAttribs++] = (value);

#define WGL_NUMBER_PIXEL_FORMATS_ARB		0x2000
#define WGL_DRAW_TO_WINDOW_ARB			0x2001
#define WGL_DRAW_TO_BITMAP_ARB			0x2002
#define WGL_ACCELERATION_ARB			0x2003
#define WGL_NEED_PALETTE_ARB			0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB		0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB		0x2006
#define WGL_SWAP_METHOD_ARB			0x2007
#define WGL_NUMBER_OVERLAYS_ARB			0x2008
#define WGL_NUMBER_UNDERLAYS_ARB		0x2009
#define WGL_TRANSPARENT_ARB			0x200A
#define WGL_TRANSPARENT_RED_VALUE_ARB		0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB		0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB		0x2039
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB		0x203A
#define WGL_TRANSPARENT_INDEX_VALUE_ARB		0x203B
#define WGL_SHARE_DEPTH_ARB			0x200C
#define WGL_SHARE_STENCIL_ARB			0x200D
#define WGL_SHARE_ACCUM_ARB			0x200E
#define WGL_SUPPORT_GDI_ARB			0x200F
#define WGL_SUPPORT_OPENGL_ARB			0x2010
#define WGL_DOUBLE_BUFFER_ARB			0x2011
#define WGL_STEREO_ARB				0x2012
#define WGL_PIXEL_TYPE_ARB			0x2013
#define WGL_COLOR_BITS_ARB			0x2014
#define WGL_RED_BITS_ARB			0x2015
#define WGL_RED_SHIFT_ARB			0x2016
#define WGL_GREEN_BITS_ARB			0x2017
#define WGL_GREEN_SHIFT_ARB			0x2018
#define WGL_BLUE_BITS_ARB			0x2019
#define WGL_BLUE_SHIFT_ARB			0x201A
#define WGL_ALPHA_BITS_ARB			0x201B
#define WGL_ALPHA_SHIFT_ARB			0x201C
#define WGL_ACCUM_BITS_ARB			0x201D
#define WGL_ACCUM_RED_BITS_ARB			0x201E
#define WGL_ACCUM_GREEN_BITS_ARB		0x201F
#define WGL_ACCUM_BLUE_BITS_ARB			0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB		0x2021
#define WGL_DEPTH_BITS_ARB			0x2022
#define WGL_STENCIL_BITS_ARB			0x2023
#define WGL_AUX_BUFFERS_ARB			0x2024

#define WGL_NO_ACCELERATION_ARB			0x2025
#define WGL_GENERIC_ACCELERATION_ARB		0x2026
#define WGL_FULL_ACCELERATION_ARB		0x2027

#define WGL_PBUFFER_WIDTH_ARB                   0x2034
#define WGL_PBUFFER_HEIGHT_ARB                  0x2035
#define WGL_PBUFFER_LOST_ARB                    0x2036
#define WGL_DRAW_TO_PBUFFER_ARB		        0x202D
#define WGL_MAX_PBUFFER_PIXELS_ARB	        0x202E
#define WGL_MAX_PBUFFER_WIDTH_ARB		0x202F
#define WGL_MAX_PBUFFER_HEIGHT_ARB	        0x2030
#define WGL_PBUFFER_LARGEST_ARB                 0x2033

#define WGL_TYPE_RGBA_ARB			0x202B
#define WGL_TYPE_COLORINDEX_ARB			0x202C

#define WGL_SAMPLE_BUFFERS_ARB		        0x2041
#define WGL_SAMPLES_ARB	                        0x2042

/**
 * WGL_render_texture 
 */
/** GetPixelFormat/ChoosePixelFormat */
#define WGL_BIND_TO_TEXTURE_RGB_ARB             0x2070
#define WGL_BIND_TO_TEXTURE_RGBA_ARB            0x2071
/** CreatePbuffer / QueryPbuffer */
#define WGL_TEXTURE_FORMAT_ARB                  0x2072
#define WGL_TEXTURE_TARGET_ARB                  0x2073
#define WGL_MIPMAP_TEXTURE_ARB                  0x2074
/** CreatePbuffer / QueryPbuffer */
#define WGL_TEXTURE_RGB_ARB                     0x2075
#define WGL_TEXTURE_RGBA_ARB                    0x2076
#define WGL_NO_TEXTURE_ARB                      0x2077
/** CreatePbuffer / QueryPbuffer */
#define WGL_TEXTURE_CUBE_MAP_ARB                0x2078
#define WGL_TEXTURE_1D_ARB                      0x2079
#define WGL_TEXTURE_2D_ARB                      0x207A
#define WGL_NO_TEXTURE_ARB                      0x2077
/** SetPbufferAttribARB/QueryPbufferARB parameters */
#define WGL_MIPMAP_LEVEL_ARB                    0x207B
#define WGL_CUBE_MAP_FACE_ARB                   0x207C
/** SetPbufferAttribARB/QueryPbufferARB attribs */
#define WGL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB     0x207D
#define WGL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB     0x207E
#define WGL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB     0x207F
#define WGL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB     0x2080
#define WGL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB     0x2081 
#define WGL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB     0x2082
/** BindTexImageARB/ReleaseTexImageARB */
#define WGL_FRONT_LEFT_ARB                  0x2083
#define WGL_FRONT_RIGHT_ARB                 0x2084
#define WGL_BACK_LEFT_ARB                   0x2085
#define WGL_BACK_RIGHT_ARB                  0x2086
#define WGL_AUX0_ARB                        0x2087 
#define WGL_AUX1_ARB                        0x2088 
#define WGL_AUX2_ARB                        0x2089 
#define WGL_AUX3_ARB                        0x208A 
#define WGL_AUX4_ARB                        0x208B 
#define WGL_AUX5_ARB                        0x208C 
#define WGL_AUX6_ARB                        0x208D
#define WGL_AUX7_ARB                        0x208E 
#define WGL_AUX8_ARB                        0x208F 
#define WGL_AUX9_ARB                        0x2090

#if 0 /* not used yet */
static unsigned ConvertAttribGLXtoWGL(const int* iWGLAttr, int* oGLXAttr) {
  unsigned nAttribs = 0;
  FIXME("not yet implemented!\n");
  return nAttribs;
}
#endif

static unsigned ConvertAttribWGLtoGLX(const int* iWGLAttr, int* oGLXAttr, Wine_GLPBuffer* pbuf) {
  unsigned nAttribs = 0;
  unsigned cur = 0; 
  int pop;

  while (0 != iWGLAttr[cur]) {
    TRACE("pAttr[%d] = %x\n", cur, iWGLAttr[cur]);

    switch (iWGLAttr[cur]) {
    case WGL_COLOR_BITS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_BUFFER_SIZE, pop);
      TRACE("pAttr[%d] = WGL_COLOR_BITS_ARB: %d\n", cur, pop);
      break;
    case WGL_BLUE_BITS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_BLUE_SIZE, pop);
      TRACE("pAttr[%d] = GLX_BLUE_SIZE: %d\n", cur, pop);
      break;
    case WGL_RED_BITS_ARB:
      pop = iWGLAttr[++cur];
      TRACE("pAttr[%d] = GLX_RED_SIZE: %d\n", cur, pop);
      PUSH2(oGLXAttr, GLX_RED_SIZE, pop);
      break;
    case WGL_GREEN_BITS_ARB:
      pop = iWGLAttr[++cur];
      TRACE("pAttr[%d] = GLX_GREEN_SIZE: %d\n", cur, pop);
      PUSH2(oGLXAttr, GLX_GREEN_SIZE, pop);
      break;
    case WGL_ALPHA_BITS_ARB:
      pop = iWGLAttr[++cur];
      TRACE("pAttr[%d] = GLX_ALPHA_SIZE: %d\n", cur, pop);
      PUSH2(oGLXAttr, GLX_ALPHA_SIZE, pop);
      break;
    case WGL_DEPTH_BITS_ARB:
      pop = iWGLAttr[++cur];
      TRACE("pAttr[%d] = GLX_DEPTH_SIZE: %d\n", cur, pop);
      PUSH2(oGLXAttr, GLX_DEPTH_SIZE, pop);
      break;
    case WGL_STENCIL_BITS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_STENCIL_SIZE, pop);
      TRACE("pAttr[%d] = GLX_STENCIL_SIZE: %d\n", cur, pop);
      break;
    case WGL_DOUBLE_BUFFER_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_DOUBLEBUFFER, pop);
      TRACE("pAttr[%d] = GLX_DOUBLEBUFFER: %d\n", cur, pop);
      break;

    case WGL_PIXEL_TYPE_ARB:
      pop = iWGLAttr[++cur];
      switch (pop) {
      case WGL_TYPE_RGBA_ARB:  pop = GLX_RGBA_BIT; break ; 
      case WGL_TYPE_COLORINDEX_ARB: pop = GLX_COLOR_INDEX_BIT; break ;
      default:
	ERR("unexpected PixelType(%x)\n", pop);	
	pop = 0;
      }
      PUSH2(oGLXAttr, GLX_RENDER_TYPE, pop);
      TRACE("pAttr[%d] = GLX_RENDER_TYPE: %d\n", cur, pop);
      break;

    case WGL_SUPPORT_GDI_ARB:
    case WGL_DRAW_TO_WINDOW_ARB:
    case WGL_DRAW_TO_BITMAP_ARB:
    case WGL_DRAW_TO_PBUFFER_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_X_RENDERABLE, pop);
      TRACE("pAttr[%d] = GLX_RENDERABLE: %d\n", cur, pop);
      break;

    case WGL_ACCELERATION_ARB:
    case WGL_SUPPORT_OPENGL_ARB:
      pop = iWGLAttr[++cur];
      /** nothing to do, if we are here, supposing support Accelerated OpenGL */
      TRACE("pAttr[%d] = WGL_SUPPORT_OPENGL_ARB: %d\n", cur, pop);
      break;

    case WGL_PBUFFER_LARGEST_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_LARGEST_PBUFFER, pop);
      TRACE("pAttr[%d] = GLX_LARGEST_PBUFFER: %x\n", cur, pop);
      break;

    case WGL_SAMPLE_BUFFERS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_SAMPLE_BUFFERS_ARB, pop);
      TRACE("pAttr[%d] = GLX_SAMPLE_BUFFERS_ARB: %x\n", cur, pop);
      break;
      
    case WGL_SAMPLES_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_SAMPLES_ARB, pop);
      TRACE("pAttr[%d] = GLX_SAMPLES_ARB: %x\n", cur, pop);
      break;

    case WGL_TEXTURE_FORMAT_ARB:
    case WGL_TEXTURE_TARGET_ARB:
    case WGL_MIPMAP_TEXTURE_ARB:
      TRACE("WGL_render_texture Attributes: %x as %x\n", iWGLAttr[cur], iWGLAttr[cur + 1]);
      if (NULL == pbuf) {
	ERR("trying to use GLX_Pbuffer Attributes without Pbuffer (was %x)\n", iWGLAttr[cur]);
      }
      if (!use_render_texture_emulation) {
	ERR("trying to use WGL_render_texture Attributes without support (was %x)\n", iWGLAttr[cur]);
      }
      pop = iWGLAttr[++cur];
      break ;

    default:
      FIXME("unsupported %x WGL Attribute\n", iWGLAttr[cur]);
      break;
    }
    ++cur;
  }
  return nAttribs;
}

GLboolean WINAPI wglGetPixelFormatAttribivARB(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues)
{
  Display* display = get_display( hdc );
  UINT i;
  GLXFBConfig* cfgs = NULL;
  GLXFBConfig  curCfg = NULL;
  int nCfgs = 0;
  int hTest;
  int tmp;
  int curGLXAttr = 0;

  TRACE("(%p, %d, %d, %d, %p, %p)\n", hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, piValues);
  
  if (0 < iLayerPlane) {
    FIXME("unsupported iLayerPlane(%d) > 0, returns FALSE\n", iLayerPlane);
    return GL_FALSE;
  }

  cfgs = glXGetFBConfigs(display, DefaultScreen(display), &nCfgs);
  if (NULL == cfgs) {
    ERR("no FB Configs found for display(%p)\n", display);
    return GL_FALSE;
  }

  for (i = 0; i < nAttributes; ++i) {
    const int curWGLAttr = piAttributes[i];
    TRACE("pAttr[%d] = %x\n", i, curWGLAttr);
    
    switch (curWGLAttr) {
    case WGL_NUMBER_PIXEL_FORMATS_ARB:
      piValues[i] = nCfgs; 
      continue ;

    case WGL_SUPPORT_OPENGL_ARB:
      piValues[i] = GL_TRUE; 
      continue ;

    case WGL_ACCELERATION_ARB:
      curGLXAttr = GLX_CONFIG_CAVEAT;
      if (nCfgs < iPixelFormat || 0 >= iPixelFormat) goto pix_error;
      curCfg = cfgs[iPixelFormat - 1];
      hTest = glXGetFBConfigAttrib(display, curCfg, curGLXAttr, &tmp);
      if (hTest) goto get_error;
      switch (tmp) {
      case GLX_NONE: piValues[i] = WGL_FULL_ACCELERATION_ARB; break;
      case GLX_SLOW_CONFIG: piValues[i] = WGL_NO_ACCELERATION_ARB; break;
      case GLX_NON_CONFORMANT_CONFIG: piValues[i] = WGL_FULL_ACCELERATION_ARB; break;
      default:
	ERR("unexpected Config Caveat(%x)\n", tmp);
	piValues[i] = WGL_NO_ACCELERATION_ARB;
      }
      continue ;

    case WGL_TRANSPARENT_ARB:
      curGLXAttr = GLX_TRANSPARENT_TYPE;
      if (nCfgs < iPixelFormat || 0 >= iPixelFormat) goto pix_error;
      curCfg = cfgs[iPixelFormat - 1];
      hTest = glXGetFBConfigAttrib(display, curCfg, curGLXAttr, &tmp);
      if (hTest) goto get_error;
      piValues[i] = GL_FALSE;
      if (GLX_NONE != tmp) piValues[i] = GL_TRUE;
      continue ;

    case WGL_PIXEL_TYPE_ARB:
      curGLXAttr = GLX_RENDER_TYPE;
      if (nCfgs < iPixelFormat || 0 >= iPixelFormat) goto pix_error;
      curCfg = cfgs[iPixelFormat - 1];
      hTest = glXGetFBConfigAttrib(display, curCfg, curGLXAttr, &tmp);
      if (hTest) goto get_error;
      switch (tmp) {
      case GLX_RGBA_BIT: piValues[i] = WGL_TYPE_RGBA_ARB; break;
      case GLX_COLOR_INDEX_BIT: piValues[i] = WGL_TYPE_COLORINDEX_ARB; break ;
      default:
	ERR("unexpected RenderType(%x)\n", tmp);
	piValues[i] = WGL_TYPE_RGBA_ARB;
      }
      continue ;
      
    case WGL_COLOR_BITS_ARB:
      curGLXAttr = GLX_BUFFER_SIZE;
      break;
    case WGL_BLUE_BITS_ARB:
      curGLXAttr = GLX_BLUE_SIZE;
      break;
    case WGL_RED_BITS_ARB:
      curGLXAttr = GLX_RED_SIZE;
      break;
    case WGL_GREEN_BITS_ARB:
      curGLXAttr = GLX_GREEN_SIZE;
      break;
    case WGL_ALPHA_BITS_ARB:
      curGLXAttr = GLX_ALPHA_SIZE;
      break;
    case WGL_DEPTH_BITS_ARB:
      curGLXAttr = GLX_DEPTH_SIZE;
      break;
    case WGL_STENCIL_BITS_ARB:
      curGLXAttr = GLX_STENCIL_SIZE;
      break;
    case WGL_DOUBLE_BUFFER_ARB:
      curGLXAttr = GLX_DOUBLEBUFFER;
      break;
    case WGL_STEREO_ARB:
      curGLXAttr = GLX_STEREO;
      break;
    case WGL_AUX_BUFFERS_ARB:
      curGLXAttr = GLX_AUX_BUFFERS;
      break;

    case WGL_SUPPORT_GDI_ARB:
    case WGL_DRAW_TO_WINDOW_ARB:
    case WGL_DRAW_TO_BITMAP_ARB:
    case WGL_DRAW_TO_PBUFFER_ARB:
      curGLXAttr = GLX_X_RENDERABLE;
      break;

    case WGL_PBUFFER_LARGEST_ARB:
      curGLXAttr = GLX_LARGEST_PBUFFER;
      break;

    case WGL_SAMPLE_BUFFERS_ARB:
      curGLXAttr = GLX_SAMPLE_BUFFERS_ARB;
      break;
      
    case WGL_SAMPLES_ARB:
      curGLXAttr = GLX_SAMPLES_ARB;
      break;

    case WGL_BIND_TO_TEXTURE_RGB_ARB: 
    case WGL_BIND_TO_TEXTURE_RGBA_ARB:
      if (!use_render_texture_emulation) {
	piValues[i] = GL_FALSE;
	continue ;	
      }
      curGLXAttr = GLX_RENDER_TYPE;
      if (nCfgs < iPixelFormat || 0 >= iPixelFormat) goto pix_error;
      curCfg = cfgs[iPixelFormat - 1];
      hTest = glXGetFBConfigAttrib(display, curCfg, curGLXAttr, &tmp);
      if (hTest) goto get_error;
      if (GLX_COLOR_INDEX_BIT == tmp) {
	piValues[i] = GL_FALSE;  
	continue ;
      }
      curGLXAttr = GLX_X_RENDERABLE;
      break;

    default:
      FIXME("unsupported %x WGL Attribute\n", curWGLAttr);
    }
    
    if (0 != curGLXAttr) {
      if (nCfgs < iPixelFormat || 0 >= iPixelFormat) goto pix_error;
      curCfg = cfgs[iPixelFormat - 1];
      hTest = glXGetFBConfigAttrib(display, curCfg, curGLXAttr, piValues + i);
      if (hTest) goto get_error;
    } else { 
      piValues[i] = GL_FALSE; 
    }
  }
  
  return GL_TRUE;

get_error:
  ERR("(%p): unexpected failure on GetFBConfigAttrib(%x) returns FALSE\n", hdc, curGLXAttr);
  XFree(cfgs);
  return GL_FALSE;

pix_error:
  ERR("(%p): unexpected iPixelFormat(%d) vs nFormats(%d), returns FALSE\n", hdc, iPixelFormat, nCfgs);
  XFree(cfgs);
  return GL_FALSE;
}

GLboolean WINAPI wglGetPixelFormatAttribfvARB(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues)
{
    FIXME("(%p, %d, %d, %d, %p, %p): stub\n", hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, pfValues);
    return GL_FALSE;
}
/**
 * http://publib.boulder.ibm.com/infocenter/pseries/index.jsp?topic=/com.ibm.aix.doc/libs/openglrf/glXChooseFBConfig.htm
 */
GLboolean WINAPI wglChoosePixelFormatARB(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats)
{
  Display* display = get_display( hdc );
  int gl_test = 0;
  int attribs[256];
  unsigned nAttribs = 0;

  GLXFBConfig* cfgs = NULL;
  int nCfgs = 0;
  UINT it;
  int fmt_id;

  GLXFBConfig* cfgs_fmt = NULL;
  int nCfgs_fmt = 0;
  UINT it_fmt;
  int tmp_fmt_id;

  int pfmt_it = 0;

  TRACE("(%p, %p, %p, %d, %p, %p): hackish\n", hdc, piAttribIList, pfAttribFList, nMaxFormats, piFormats, nNumFormats);
  if (NULL != pfAttribFList) {
    FIXME("unused pfAttribFList\n");
  }

  nAttribs = ConvertAttribWGLtoGLX(piAttribIList, attribs, NULL);
  PUSH1(attribs, None);

  cfgs = glXChooseFBConfig(display, DefaultScreen(display), attribs, &nCfgs);
  if (NULL == cfgs) {
    WARN("Compatible Pixel Format not found\n");
    return GL_FALSE;
  }

  cfgs_fmt = glXGetFBConfigs(display, DefaultScreen(display), &nCfgs_fmt);
  if (NULL == cfgs_fmt) {
    ERR("Failed to get All FB Configs\n");
    XFree(cfgs);
    return GL_FALSE;
  }

  for (it = 0; it < nMaxFormats && it < nCfgs; ++it) {
    gl_test = glXGetFBConfigAttrib(display, cfgs[it], GLX_FBCONFIG_ID, &fmt_id);
    if (gl_test) {
      ERR("Failed to retrieve FBCONFIG_ID from GLXFBConfig, expect problems.\n");
      continue ;
    }
    for (it_fmt = 0; it_fmt < nCfgs_fmt; ++it_fmt) {
      gl_test = glXGetFBConfigAttrib(display, cfgs_fmt[it_fmt], GLX_FBCONFIG_ID, &tmp_fmt_id);
      if (gl_test) {
	ERR("Failed to retrieve FBCONFIG_ID from GLXFBConfig, expect problems.\n");
	continue ;
      }
      if (fmt_id == tmp_fmt_id) {
	piFormats[pfmt_it] = it_fmt + 1;
	++pfmt_it;
	break ;
      }
    }
    if (it_fmt == nCfgs_fmt) {
      ERR("Failed to get valid fmt for %d. Try next.\n", it);
      continue ;
    }
    TRACE("at %d/%d found FBCONFIG_ID(%d/%d)\n", it + 1, nCfgs, piFormats[it], nCfgs_fmt);
  }
  
  *nNumFormats = pfmt_it;
  /** free list */
  XFree(cfgs);
  XFree(cfgs_fmt);
  return GL_TRUE;
}

#define HPBUFFERARB void *
HPBUFFERARB WINAPI wglCreatePbufferARB(HDC hdc, int iPixelFormat, int iWidth, int iHeight, const int *piAttribList)
{
  Wine_GLPBuffer* object = NULL;
  Display* display = get_display( hdc );
  GLXFBConfig* cfgs = NULL;
  int nCfgs = 0;
  int attribs[256];
  unsigned nAttribs = 0;

  TRACE("(%p, %d, %d, %d, %p)\n", hdc, iPixelFormat, iWidth, iHeight, piAttribList);

  if (0 >= iPixelFormat) {
    ERR("(%p): unexpected iPixelFormat(%d) <= 0, returns NULL\n", hdc, iPixelFormat);
    SetLastError(ERROR_INVALID_PIXEL_FORMAT);
    return NULL; /* unespected error */
  }

  cfgs = glXGetFBConfigs(display, DefaultScreen(display), &nCfgs);

  if (NULL == cfgs || 0 == nCfgs) {
    ERR("(%p): Cannot get FB Configs for iPixelFormat(%d), returns NULL\n", hdc, iPixelFormat);
    SetLastError(ERROR_INVALID_PIXEL_FORMAT);
    return NULL; /* unespected error */
  }
  if (nCfgs < iPixelFormat) {
    ERR("(%p): unexpected iPixelFormat(%d) > nFormats(%d), returns NULL\n", hdc, iPixelFormat, nCfgs);
    SetLastError(ERROR_INVALID_PIXEL_FORMAT);
    goto create_failed; /* unespected error */
  }

  object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Wine_GLPBuffer));
  if (NULL == object) {
    SetLastError(ERROR_NO_SYSTEM_RESOURCES);
    goto create_failed; /* unespected error */
  }
  object->hdc = hdc;
  object->display = display;
  object->width = iWidth;
  object->height = iHeight;
  object->pixelFormat = iPixelFormat;

  nAttribs = ConvertAttribWGLtoGLX(piAttribList, attribs, object);
  PUSH2(attribs, GLX_PBUFFER_WIDTH,  iWidth);
  PUSH2(attribs, GLX_PBUFFER_HEIGHT, iHeight); 
  PUSH1(attribs, None);

  while (0 != *piAttribList) {
    int attr_v;
    switch (*piAttribList) {
    case WGL_TEXTURE_FORMAT_ARB: {
      if (!use_render_texture_emulation) {
	SetLastError(ERROR_INVALID_DATA);	  
	goto create_failed;
      }
      ++piAttribList;
      attr_v = *piAttribList;
      TRACE("WGL_render_texture Attribute: WGL_TEXTURE_FORMAT_ARB as %x\n", attr_v);
      switch (attr_v) {
      case WGL_TEXTURE_RGB_ARB:
      case WGL_TEXTURE_RGBA_ARB:
      case WGL_NO_TEXTURE_ARB:
	break;
      default:
	SetLastError(ERROR_INVALID_DATA);	  
	goto create_failed;
      }
      break;
    }
    
    case WGL_TEXTURE_TARGET_ARB: {
      if (!use_render_texture_emulation) {
	SetLastError(ERROR_INVALID_DATA);	  
	goto create_failed;
      }
      ++piAttribList;
      attr_v = *piAttribList;
      TRACE("WGL_render_texture Attribute: WGL_TEXTURE_TARGET_ARB as %x\n", attr_v);
      switch (attr_v) {
      case WGL_TEXTURE_CUBE_MAP_ARB: {
	if (iWidth != iHeight) {
	  SetLastError(ERROR_INVALID_DATA);	  
	  goto create_failed;
	}
	object->texture_target = GL_TEXTURE_CUBE_MAP;
	object->texture_bind_target = GL_TEXTURE_CUBE_MAP;
	break;
      }
      case WGL_TEXTURE_1D_ARB: {
	if (1 != iHeight) {
	  SetLastError(ERROR_INVALID_DATA);	  
	  goto create_failed;
	}
	object->texture_target = GL_TEXTURE_1D;
	object->texture_bind_target = GL_TEXTURE_1D;
	break;
      }
      case WGL_TEXTURE_2D_ARB: {
	object->texture_target = GL_TEXTURE_2D;
	object->texture_bind_target = GL_TEXTURE_2D;
	break;
      }
      case WGL_NO_TEXTURE_ARB:
	break;
      default:
	SetLastError(ERROR_INVALID_DATA);	  
	goto create_failed;
      }
      break;
    }

    case WGL_MIPMAP_TEXTURE_ARB: {
      if (!use_render_texture_emulation) {
	SetLastError(ERROR_INVALID_DATA);	  
	goto create_failed;
      }
      ++piAttribList;
      attr_v = *piAttribList;
      TRACE("WGL_render_texture Attribute: WGL_MIPMAP_TEXTURE_ARB as %x\n", attr_v);
      if (0 != attr_v) {
	SetLastError(ERROR_INVALID_DATA);	  
	goto create_failed;	
      }
      break ;
    }

    }
    ++piAttribList;
  }

  object->drawable = glXCreatePbuffer(display, cfgs[iPixelFormat - 1], attribs);
  TRACE("new Pbuffer drawable as %p\n", (void*) object->drawable);
  if (!object->drawable) {
    SetLastError(ERROR_NO_SYSTEM_RESOURCES);
    goto create_failed; /* unespected error */
  }
  TRACE("->(%p)\n", object);

  /** free list */
  XFree(cfgs);
  return (HPBUFFERARB) object;

create_failed:
  if (NULL != cfgs) XFree(cfgs);
  if (NULL != object) HeapFree(GetProcessHeap(), 0, object);
  TRACE("->(FAILED)\n");
  return (HPBUFFERARB) NULL;
}

HDC WINAPI wglGetPbufferDCARB(HPBUFFERARB hPbuffer)
{
  Wine_GLPBuffer* object = (Wine_GLPBuffer*) hPbuffer;
  HDC hDC;
  if (NULL == object) {
    SetLastError(ERROR_INVALID_HANDLE);
    return NULL;
  }
  hDC = CreateCompatibleDC(object->hdc);
  set_drawable(hDC, object->drawable); /* works ?? */
  TRACE("(%p)->(%p)\n", hPbuffer, hDC);
  return hDC;
}

int WINAPI wglReleasePbufferDCARB(HPBUFFERARB hPbuffer, HDC hdc)
{
  TRACE("(%p, %p)\n", hPbuffer, hdc);
  DeleteDC(hdc);
  return 0;
}

GLboolean WINAPI wglDestroyPbufferARB(HPBUFFERARB hPbuffer)
{
  Wine_GLPBuffer* object = (Wine_GLPBuffer*) hPbuffer;
  TRACE("(%p)\n", hPbuffer);
  if (NULL == object) {
    SetLastError(ERROR_INVALID_HANDLE);
    return GL_FALSE;
  }
  glXDestroyPbuffer(object->display, object->drawable);
  HeapFree(GetProcessHeap(), 0, object);
  return GL_TRUE;
}

GLboolean WINAPI wglQueryPbufferARB(HPBUFFERARB hPbuffer, int iAttribute, int *piValue)
{
  Wine_GLPBuffer* object = (Wine_GLPBuffer*) hPbuffer;
  TRACE("(%p, 0x%x, %p)\n", hPbuffer, iAttribute, piValue);
  if (NULL == object) {
    SetLastError(ERROR_INVALID_HANDLE);
    return GL_FALSE;
  }
  switch (iAttribute) {
  case WGL_PBUFFER_WIDTH_ARB:
    glXQueryDrawable(object->display, object->drawable, GLX_WIDTH, (unsigned int*) piValue);
    break;
  case WGL_PBUFFER_HEIGHT_ARB:
    glXQueryDrawable(object->display, object->drawable, GLX_HEIGHT, (unsigned int*) piValue);
    break;

  case WGL_PBUFFER_LOST_ARB:
    FIXME("unsupported WGL_PBUFFER_LOST_ARB (need glXSelectEvent/GLX_DAMAGED work)\n");
    break;

  case WGL_TEXTURE_FORMAT_ARB:
  case WGL_TEXTURE_TARGET_ARB:
  case WGL_MIPMAP_TEXTURE_ARB:
    if (!object->use_render_texture) {
      SetLastError(ERROR_INVALID_HANDLE);
      return GL_FALSE;
    }
    if (!use_render_texture_emulation) {
      SetLastError(ERROR_INVALID_DATA);      
      return GL_FALSE; /** how to FIX ? */
    }
    FIXME("unsupported WGL_ARB_render_texture attribute query for 0x%x\n", iAttribute);
    break;

  default:
    FIXME("unexpected attribute %x\n", iAttribute);
    break;
  }

  return GL_TRUE;
}

GLboolean WINAPI wglBindTexImageARB(HPBUFFERARB hPbuffer, int iBuffer)
{
  Wine_GLPBuffer* object = (Wine_GLPBuffer*) hPbuffer;
  TRACE("(%p, %d)\n", hPbuffer, iBuffer);
  if (NULL == object) {
    SetLastError(ERROR_INVALID_HANDLE);
    return GL_FALSE;
  }
  if (!object->use_render_texture) {
    SetLastError(ERROR_INVALID_HANDLE);
    return GL_FALSE;
  }
  if (1 == use_render_texture_emulation) {
    return GL_TRUE;
  }
  if (NULL != p_glXBindTexImageARB) {
    return p_glXBindTexImageARB(object->display, object->drawable, iBuffer);
  }
  return GL_FALSE;
}

GLboolean WINAPI wglReleaseTexImageARB(HPBUFFERARB hPbuffer, int iBuffer)
{
  Wine_GLPBuffer* object = (Wine_GLPBuffer*) hPbuffer;
  TRACE("(%p, %d)\n", hPbuffer, iBuffer);
  if (NULL == object) {
    SetLastError(ERROR_INVALID_HANDLE);
    return GL_FALSE;
  }
  if (!object->use_render_texture) {
    SetLastError(ERROR_INVALID_HANDLE);
    return GL_FALSE;
  }
  if (1 == use_render_texture_emulation) {
    GLint prev_binded_tex;
    glGetIntegerv(object->texture_target, &prev_binded_tex);
    glBindTexture(object->texture_target, object->texture);
    if (GL_TEXTURE_1D == object->texture_target) {
      glCopyTexSubImage1D(object->texture_bind_target, object->texture_level, 0, 0, 0, object->width);
    } else {
      glCopyTexSubImage2D(object->texture_bind_target, object->texture_level, 0, 0, 0, 0, object->width, object->height);
    }
    glBindTexture(object->texture_target, prev_binded_tex);
    return GL_TRUE;
  }
  if (NULL != p_glXReleaseTexImageARB) {
    return p_glXReleaseTexImageARB(object->display, object->drawable, iBuffer);
  }
  return GL_FALSE;
}

GLboolean WINAPI wglSetPbufferAttribARB(HPBUFFERARB hPbuffer, const int *piAttribList)
{
  Wine_GLPBuffer* object = (Wine_GLPBuffer*) hPbuffer;
  WARN("(%p, %p): alpha-testing, report any problem\n", hPbuffer, piAttribList);
  if (NULL == object) {
    SetLastError(ERROR_INVALID_HANDLE);
    return GL_FALSE;
  }
  if (!object->use_render_texture) {
    SetLastError(ERROR_INVALID_HANDLE);
    return GL_FALSE;
  }
  if (1 == use_render_texture_emulation) {
    return GL_TRUE;
  }
  if (NULL != p_glXDrawableAttribARB) {
    return p_glXDrawableAttribARB(object->display, object->drawable, piAttribList); 
  }
  return GL_FALSE;
}

static const struct {
    const char *name;
    BOOL (*query_function)(glXGetProcAddressARB_t proc, const char *gl_version, const char *gl_extensions,
			   const char *glx_version, const char *glx_extensions,
			   const char *server_glx_extensions, const char *client_glx_extensions);
} extension_list[] = {
  { "WGL_ARB_make_current_read", query_function_make_current_read },
  { "WGL_ARB_multisample",       query_function_multisample },
  { "WGL_ARB_pbuffer",           query_function_pbuffer },
  { "WGL_ARB_pixel_format" ,     query_function_pixel_format },
  { "WGL_ARB_render_texture",    query_function_render_texture },
  { "WGL_EXT_swap_control",      query_function_swap_control }
};

/* Used to initialize the WGL extension string at DLL loading */
void wgl_ext_initialize_extensions(Display *display, int screen, glXGetProcAddressARB_t proc, const char* disabled_extensions)
{
    int size = strlen(WGL_extensions_base);
    const char *glx_extensions = glXQueryExtensionsString(display, screen);
    const char *server_glx_extensions = glXQueryServerString(display, screen, GLX_EXTENSIONS);
    const char *client_glx_extensions = glXGetClientString(display, GLX_EXTENSIONS);
    const char *gl_extensions = (const char *) glGetString(GL_EXTENSIONS);
    const char *gl_version = (const char *) glGetString(GL_VERSION);
    const char *glx_version = glXGetClientString(display, GLX_VERSION);
    int i;

    TRACE("GL version      : %s.\n", debugstr_a(gl_version));
    TRACE("GL exts         : %s.\n", debugstr_a(gl_extensions));
    TRACE("GLX exts        : %s.\n", debugstr_a(glx_extensions));
    TRACE("Server GLX exts : %s.\n", debugstr_a(server_glx_extensions));
    TRACE("Client GLX exts : %s.\n", debugstr_a(client_glx_extensions));

    for (i = 0; i < (sizeof(extension_list) / sizeof(extension_list[0])); i++) {
        if (strstr(disabled_extensions, extension_list[i].name)) continue ; /* disabled by config, next */
	if (extension_list[i].query_function(proc, 
					     gl_version, gl_extensions, 
					     glx_version, glx_extensions,
					     server_glx_extensions, client_glx_extensions)) {
	    size += strlen(extension_list[i].name) + 1;
	}
    }

    /* For the moment, only 'base' extensions are supported. */
    WGL_extensions = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size + 1);
    if (WGL_extensions == NULL) {
	WGL_extensions = (char *) WGL_extensions_base;
    } else {
	strcpy(WGL_extensions, WGL_extensions_base);
	for (i = 0; i < (sizeof(extension_list) / sizeof(extension_list[0])); i++) {
	    if (strstr(disabled_extensions, extension_list[i].name)) continue ; /* disabled by config, next */
	    if (extension_list[i].query_function(proc, 
						 gl_version, gl_extensions, 
						 glx_version, glx_extensions,
						 server_glx_extensions, client_glx_extensions)) {
		strcat(WGL_extensions, " ");
		strcat(WGL_extensions, extension_list[i].name);
	    }
	}
    }

    TRACE("Supporting following WGL extensions : %s.\n", debugstr_a(WGL_extensions));
}

void wgl_ext_finalize_extensions(void)
{
    if (WGL_extensions != WGL_extensions_base) {
	HeapFree(GetProcessHeap(), 0, WGL_extensions);
    }
}

/*
 * Putting this at the end to prevent having to write the prototypes :-) 
 *
 * @WARNING: this list must be ordered by name
 *
 * @TODO: real handle caps on providing some func_init functions (third param, ex: to check extensions)
 */
WGL_extension wgl_extension_registry[] = {
    { "wglBindTexImageARB", (void *) wglBindTexImageARB, NULL, NULL},
    { "wglChoosePixelFormatARB", (void *) wglChoosePixelFormatARB, NULL, NULL},
    { "wglCreatePbufferARB", (void *) wglCreatePbufferARB, NULL, NULL},
    { "wglDestroyPbufferARB", (void *) wglDestroyPbufferARB, NULL, NULL},
    { "wglGetCurrentReadDCARB", (void *) wglGetCurrentReadDCARB, NULL, NULL},
    { "wglGetExtensionsStringARB", (void *) wglGetExtensionsStringARB, NULL, NULL},
    { "wglGetExtensionsStringEXT", (void *) wglGetExtensionsStringEXT, NULL, NULL},
    { "wglGetPbufferDCARB", (void *) wglGetPbufferDCARB, NULL, NULL},
    { "wglGetPixelFormatAttribfvARB", (void *) wglGetPixelFormatAttribfvARB, NULL, NULL},
    { "wglGetPixelFormatAttribivARB", (void *) wglGetPixelFormatAttribivARB, NULL, NULL},
    { "wglGetSwapIntervalEXT", (void *) wglGetSwapIntervalEXT, NULL, NULL},
    { "wglMakeContextCurrentARB", (void *) wglMakeContextCurrentARB, NULL, NULL },
    { "wglQueryPbufferARB", (void *) wglQueryPbufferARB, NULL, NULL},
    { "wglReleasePbufferDCARB", (void *) wglReleasePbufferDCARB, NULL, NULL},
    { "wglReleaseTexImageARB", (void *) wglReleaseTexImageARB, NULL, NULL},
    { "wglSetPbufferAttribARB", (void *) wglSetPbufferAttribARB, NULL, NULL},
    { "wglSwapIntervalEXT", (void *) wglSwapIntervalEXT, NULL, NULL}
};
int wgl_extension_registry_size = sizeof(wgl_extension_registry) / sizeof(wgl_extension_registry[0]);
