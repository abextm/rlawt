/*
 * Copyright (c) 2022 Abex
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "net_runelite_rlawt_AWTContext.h"
#include <jawt.h>
#include <jawt_md.h>
#include <stdbool.h>
#define EGL_EGL_PROTOTYPES 0
#include <EGL/egl.h>

#ifdef __APPLE__
# define GL_SILENCE_DEPRECATION
#	include <IOSurface/IOSurface.h>
# include <QuartzCore/CALayer.h>
# include <OpenGL/OpenGL.h>
#endif

#ifdef __unix__
#	include <X11/Xlib.h>
#	include <GL/glx.h>
#endif

#ifdef _WIN32
#	include <windows.h>
#	include <GL/gl.h>
#	include <wglext.h>
#endif

#define RLAWT_EGL_METHODS \
	METHOD(PFNEGLGETDISPLAYPROC, eglGetDisplay) \
	METHOD(PFNEGLINITIALIZEPROC, eglInitialize) \
	METHOD(PFNEGLTERMINATEPROC, eglTerminate) \
	METHOD(PFNEGLGETPROCADDRESSPROC, eglGetProcAddress) \
	METHOD(PFNEGLCHOOSECONFIGPROC, eglChooseConfig) \
	METHOD(PFNEGLCREATECONTEXTPROC, eglCreateContext) \
	METHOD(PFNEGLDESTROYCONTEXTPROC, eglDestroyContext) \
	METHOD(PFNEGLCREATEPBUFFERSURFACEPROC, eglCreatePbufferSurface) \
	METHOD(PFNEGLCREATEWINDOWSURFACEPROC, eglCreateWindowSurface) \
	METHOD(PFNEGLDESTROYSURFACEPROC, eglDestroySurface) \
	METHOD(PFNEGLMAKECURRENTPROC, eglMakeCurrent) \
	METHOD(PFNEGLSWAPBUFFERSPROC, eglSwapBuffers) \
	METHOD(PFNEGLSWAPINTERVALPROC, eglSwapInterval) \
	METHOD(PFNEGLQUERYSTRINGPROC, eglQueryString) \
	METHOD(PFNEGLGETCONFIGATTRIBPROC, eglGetConfigAttrib) \
	METHOD(PFNEGLWAITNATIVEPROC, eglWaitNative) \
	METHOD(PFNEGLWAITGLPROC, eglWaitGL) \
	METHOD(PFNEGLBINDAPIPROC, eglBindAPI) \
	METHOD(PFNEGLGETERRORPROC, eglGetError) \
	METHOD(PFNEGLGETPLATFORMDISPLAYPROC, eglGetPlatformDisplay) \
	METHOD(PFNEGLCREATEPLATFORMWINDOWSURFACEPROC, eglCreatePlatformWindowSurface)
typedef struct {
#	define METHOD(TYPE, NAME) TYPE NAME;
	RLAWT_EGL_METHODS
# undef METHOD
} RLAWTEGLMethods;

typedef struct AWTContext {
	JAWT awt;
	JAWT_DrawingSurface *ds;
	bool contextCreated;

	RLAWTEGLMethods egl;

	EGLDisplay eglDisplay;
	EGLContext eglContext;
	EGLSurface eglSurface;

#ifdef __APPLE__
#ifdef __OBJC__
	CALayer *layer;
#else
	void *layer;
#endif
	IOSurfaceRef buffer[2];
	CGFloat bufferScale[2];
	CGLContextObj context;

	GLuint tex[2];
	GLuint fbo[2];
	int back;

	int offsetX;
	int offsetY;
#endif

#ifdef __unix__
	Display *dpy;
	Drawable drawable;
	GLXContext context;
	PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT;
	bool glxSwapControlTear;
	PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI;
	bool doubleBuffered;
#endif

#ifdef _WIN32
	JAWT_DrawingSurfaceInfo *dsi;
	JAWT_Win32DrawingSurfaceInfo *dspi;
	HGLRC context;
	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
	bool wglSwapControlTear;
#endif

	int alphaDepth;
	int depthDepth;
	int stencilDepth;

	int multisamples;

	bool useEGL;
	bool useGLES;

	bool (*makeCurrent)(JNIEnv*, struct AWTContext*, bool attach);
	int (*setSwapInterval)(JNIEnv *env, struct AWTContext *ctx, int interval);
	void (*swapBuffers)(JNIEnv*, struct AWTContext*);
} AWTContext;

void rlawtThrow(JNIEnv *env, const char *msg);
void rlawtUnlockAWT(JNIEnv *env, AWTContext *ctx);
AWTContext *rlawtGetContext(JNIEnv *env, jobject self);
bool rlawtContextState(JNIEnv *env, AWTContext *context, bool created);


void rlawtContextFreePlatform(JNIEnv *env, AWTContext *ctx);

bool rlawtEGLInit(JNIEnv *env, AWTContext *ctx, EGLNativeWindowType nativeWindow);
void rlawtEGLDestroy(JNIEnv *env, AWTContext *ctx);