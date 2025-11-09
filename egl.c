#include "rlawt.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

void rlawtEglInit(JNIEnv *env, AWTContext *ctx, EGLNativeWindowType nativeWindow) {
    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE) {
        rlawtThrow(env, "eglBindAPI failed");
        return;
    }

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        rlawtThrow(env, "eglGetDisplay failed");
        return;
    }

    if (!eglInitialize(display, NULL, NULL)) {
        rlawtThrow(env, "eglInitialize failed");
        goto freedisplay;
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
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
        rlawtThrow(env, "eglChooseConfig failed");
        goto freedisplay;
    }

    EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext eglCtx = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttribs);
    if (eglCtx == EGL_NO_CONTEXT) {
        rlawtThrow(env, "eglCreateContext failed");
        goto freedisplay;
    }

    EGLSurface surf = eglCreateWindowSurface(display, config, nativeWindow, NULL);
    if (surf == EGL_NO_SURFACE) {
        rlawtThrow(env, "eglCreateWindowSurface failed");
        goto freecontext;
    }

    if (!eglMakeCurrent(display, surf, surf, eglCtx)) {
        rlawtThrow(env, "eglMakeCurrent failed");
        goto freesurface;
    }

    ctx->eglDisplay = display;
    ctx->eglContext = eglCtx;
    ctx->eglSurface = surf;
    ctx->eglConfig = config;
    return;

freesurface:
    eglDestroySurface(display, surf);
freecontext:
    eglDestroyContext(display, eglCtx);
freedisplay:
    eglTerminate(display);
}

void rlawtEglDestroy(JNIEnv *env, AWTContext *ctx) {
    if (!eglMakeCurrent(ctx->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
        rlawtThrow(env, "eglMakeCurrent failed");
    }

    if (!eglDestroyContext(ctx->eglDisplay, ctx->eglContext)) {
        rlawtThrow(env, "eglDestroyContext failed");
    }

    if (!eglDestroySurface(ctx->eglDisplay, ctx->eglSurface)) {
        rlawtThrow(env, "eglDestroySurface failed");
    }

    if (!eglTerminate(ctx->eglDisplay)) {
        rlawtThrow(env, "eglTerminate failed");
    }
}