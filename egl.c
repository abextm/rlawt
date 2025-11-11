#include "rlawt.h"
#include <EGL/eglext.h>

static bool rlawtEGLMakeCurrent(JNIEnv *env, AWTContext *ctx, bool attach) {
	if (!ctx->egl.eglMakeCurrent(
		ctx->eglDisplay,
		attach ? ctx->eglSurface : EGL_NO_SURFACE,
		attach ? ctx->eglSurface : EGL_NO_SURFACE,
		attach ? ctx->eglContext : EGL_NO_CONTEXT
	)) {
		rlawtThrow(env, "unable to make current");
		return false;
	}
	return true;
}

static int rlawtEGLSetSwapInterval(JNIEnv *env, AWTContext *ctx, int interval) {
	if (interval < 0) {
		interval = -interval;
	}

	ctx->egl.eglSwapInterval(ctx->eglDisplay, interval);

	return interval;
}

static void rlawtEGLSwapBuffers(JNIEnv *env, AWTContext *ctx) {
	ctx->egl.eglSwapBuffers(ctx->eglDisplay, ctx->eglSurface);
}

bool rlawtEGLInit(JNIEnv *env, AWTContext *ctx, EGLNativeWindowType nativeWindow) {
	if (!ctx->egl.eglBindAPI(ctx->useGLES ? EGL_OPENGL_ES_API : EGL_OPENGL_API)) {
		rlawtThrow(env, "eglBindAPI failed");
		goto freeDisplay;
	}

	if (!ctx->egl.eglInitialize(ctx->eglDisplay, NULL, NULL)) {
		rlawtThrow(env, "eglInitialize failed");
		goto freeDisplay;
	}

	EGLint configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, ctx->alphaDepth,
		EGL_DEPTH_SIZE, ctx->depthDepth,
		EGL_STENCIL_SIZE, ctx->stencilDepth,
		EGL_NONE
	};

	EGLConfig config = NULL;
	EGLint numConfigs = 0;
	if (!ctx->egl.eglChooseConfig(ctx->eglDisplay, configAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
		rlawtThrow(env, "eglChooseConfig failed");
		goto freeDisplay;
	}

	EGLint ctxAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};
	ctx->eglContext = ctx->egl.eglCreateContext(ctx->eglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
	if (ctx->eglContext == EGL_NO_CONTEXT) {
		rlawtThrow(env, "eglCreateContext failed");
		goto freeDisplay;
	}

	ctx->eglSurface = ctx->egl.eglCreateWindowSurface(ctx->eglDisplay, config, nativeWindow, NULL);
	if (ctx->eglSurface == EGL_NO_SURFACE) {
		rlawtThrow(env, "eglCreateWindowSurface failed");
		goto freeContext;
	}

	if (!rlawtEGLMakeCurrent(env, ctx, true)) {
		rlawtThrow(env, "eglMakeCurrent failed");
		goto freeSurface;
	}

	ctx->makeCurrent = rlawtEGLMakeCurrent;
	ctx->setSwapInterval = rlawtEGLSetSwapInterval;
	ctx->swapBuffers = rlawtEGLSwapBuffers;

	return true;

freeSurface:
	ctx->egl.eglDestroySurface(ctx->eglDisplay, ctx->eglSurface);
freeContext:
	ctx->egl.eglDestroyContext(ctx->eglDisplay, ctx->eglContext);
freeDisplay:
	ctx->egl.eglTerminate(ctx->eglDisplay);
	return false;
}

void rlawtEGLDestroy(JNIEnv *env, AWTContext *ctx) {
	if (!ctx->contextCreated) {
		return;
	}

	if (!ctx->egl.eglMakeCurrent(ctx->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
		rlawtThrow(env, "eglMakeCurrent failed");
	}

	if (!ctx->egl.eglDestroyContext(ctx->eglDisplay, ctx->eglContext)) {
		rlawtThrow(env, "eglDestroyContext failed");
	}

	if (!ctx->egl.eglDestroySurface(ctx->eglDisplay, ctx->eglSurface)) {
		rlawtThrow(env, "eglDestroySurface failed");
	}

	if (!ctx->egl.eglTerminate(ctx->eglDisplay)) {
		rlawtThrow(env, "eglTerminate failed");
	}
}
