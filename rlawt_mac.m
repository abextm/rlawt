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

#ifdef __APPLE__

#include "rlawt.h"
#include <jawt_md.h>
#include <OpenGL/gl3.h>

#include <AppKit/NSColor.h>

@protocol CanSetContentsChanged
-(void)setContentsChanged;
@end

@interface RLLayer : CALayer
@end

@implementation RLLayer
@end


void rlawtThrow(JNIEnv *env, const char *msg) {
	if ((*env)->ExceptionCheck(env)) {
		return;
	}
	jclass clazz = (*env)->FindClass(env, "java/lang/RuntimeException");
	(*env)->ThrowNew(env, clazz, msg);
}

static void rlawtThrowCGLError(JNIEnv *env, const char *msg, CGLError err) {
	char buf[256] = {0};
	snprintf(buf, sizeof(buf), "%s (cgl: %s)", msg, CGLErrorString(err));
	rlawtThrow(env, buf);
}

static bool makeCurrent(JNIEnv *env, CGLContextObj ctx) {
	CGLError err = CGLSetCurrentContext(ctx);
	if (err != kCGLNoError) {
		rlawtThrowCGLError(env, "unable to make current", err);
		return false;
	}

	return true;
}

static void propsPutInt(CFMutableDictionaryRef props, const CFStringRef key, int value) {
	CFNumberRef boxedValue = CFNumberCreate(NULL, kCFNumberIntType, &value);
	CFDictionaryAddValue(props, key, boxedValue);
	CFRelease(boxedValue);
}

JNIEXPORT void JNICALL Java_net_runelite_rlawt_AWTContext_createGLContext(JNIEnv *env, jobject self) {
	AWTContext *ctx = rlawtGetContext(env, self);
	if (!ctx || !rlawtContextState(env, ctx, false)) {
		return;
	}

	JAWT_DrawingSurfaceInfo *dsi = ctx->ds->GetDrawingSurfaceInfo(ctx->ds);
	if (!dsi) {
		rlawtThrow(env, "unable to get dsi");
		return;
	}

	id<JAWT_SurfaceLayers> dspi = (id<JAWT_SurfaceLayers>) dsi->platformInfo;
	if (!dspi) {
		rlawtThrow(env, "unable to get platform dsi");
		goto freeDSI;
	}

	CGLPixelFormatAttribute attribs[] = {
		kCGLPFAColorSize, 24,
		kCGLPFAAlphaSize, ctx->alphaDepth,
		kCGLPFADepthSize, ctx->depthDepth,
		kCGLPFAStencilSize, ctx->stencilDepth,
		kCGLPFADoubleBuffer, true,
		kCGLPFASampleBuffers, ctx->multisamples > 0,
		kCGLPFASamples, ctx->multisamples,
		kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute) kCGLOGLPVersion_GL4_Core,
		0
	};

	CGLPixelFormatObj pxFmt = NULL;
	int numPxFmt = 1;
	CGLError err = CGLChoosePixelFormat(attribs, &pxFmt, &numPxFmt);
	if (!pxFmt || err != kCGLNoError) {
		rlawtThrowCGLError(env, "unable to choose format", err);
		goto freeDSI;
	}

	err = CGLCreateContext(pxFmt, NULL, &ctx->context);
	CGLReleasePixelFormat(pxFmt);
	if (!ctx->context || err != kCGLNoError) {
		rlawtThrowCGLError(env, "unable to create context", err);
		goto freeDSI;
	}

	if (!makeCurrent(env, ctx->context)) {
		goto freeContext;
	}

	{
		CFMutableDictionaryRef props = CFDictionaryCreateMutable(NULL, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		propsPutInt(props, kIOSurfaceHeight, dsi->bounds.height);
		propsPutInt(props, kIOSurfaceWidth, dsi->bounds.width);
		propsPutInt(props, kIOSurfaceBytesPerElement, 4);
		propsPutInt(props, kIOSurfacePixelFormat, (int)'BGRA');

		ctx->back = IOSurfaceCreate(props);
		CFRelease(props);
		if (!ctx->back) {
			rlawtThrow(env, "unable to create io surface");
			goto freeContext;
		}
	}

	glGenTextures(1, &ctx->tex);
	glGenFramebuffers(1, &ctx->fbo);
	{
		const GLuint target = GL_TEXTURE_RECTANGLE;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(target, ctx->tex);
		err = CGLTexImageIOSurface2D(
			ctx->context,
			target, GL_RGBA,
			dsi->bounds.width, dsi->bounds.height,
			GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
			ctx->back, 
			0);
		glBindTexture(target, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, ctx->tex, 0);

		if (err != kCGLNoError) {
			rlawtThrowCGLError(env, "unable to bind io surface to texture", err);
			goto freeDSI;
		}

		int fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
			char buf[256] = {0};
			snprintf(buf, sizeof(buf), "unable to create fb (%d)", fbStatus);
			rlawtThrow(env, buf);
			goto freeDSI;
		}
	}

	{
		jclass clazz = (*env)->GetObjectClass(env, self);
		jfieldID fboID = (*env)->GetFieldID(env, clazz, "framebuffer", "I");
		if (!fboID) {
			goto freeDSI;
		}
		(*env)->SetIntField(env, self, fboID, ctx->fbo);
	}
dispatch_sync(dispatch_get_main_queue(), ^{
	ctx->layer = [[RLLayer alloc] init];
	dspi.layer = ctx->layer;
	ctx->layer.opaque = true;
	ctx->layer.affineTransform = CGAffineTransformMakeScale(1, -1);
	ctx->layer.frame = CGRectMake(
		dsi->bounds.x - ctx->offsetX,
		dspi.windowLayer.bounds.size.height - (dsi->bounds.y - ctx->offsetY) - dsi->bounds.height,
		dsi->bounds.width,
		dsi->bounds.height);

	[ctx->layer setContents: (id) ctx->back];
	[ctx->layer setContentsScale: 1.0];
});

	ctx->ds->FreeDrawingSurfaceInfo(dsi);

	ctx->contextCreated = true;
	return;

freeContext:
	CGLDestroyContext(ctx->context);
freeDSI:
	ctx->ds->FreeDrawingSurfaceInfo(dsi);
}

void rlawtContextFreePlatform(JNIEnv *env, AWTContext *ctx) {
}

JNIEXPORT int JNICALL Java_net_runelite_rlawt_AWTContext_setSwapInterval(JNIEnv *env, jobject self, jint interval) {
	return 0;
}

JNIEXPORT void JNICALL Java_net_runelite_rlawt_AWTContext_makeCurrent(JNIEnv *env, jobject self) {
	AWTContext *ctx = rlawtGetContext(env, self);
	if (!ctx || !rlawtContextState(env, ctx, true)) {
		return;
	}

	makeCurrent(env, ctx->context);
}

JNIEXPORT void JNICALL Java_net_runelite_rlawt_AWTContext_detachCurrent(JNIEnv *env, jobject self) {
	AWTContext *ctx = rlawtGetContext(env, self);
	if (!ctx || !rlawtContextState(env, ctx, true)) {
		return;
	}

	makeCurrent(env, NULL);
}

JNIEXPORT void JNICALL Java_net_runelite_rlawt_AWTContext_swapBuffers(JNIEnv *env, jobject self) {
	AWTContext *ctx = rlawtGetContext(env, self);
	if (!ctx || !rlawtContextState(env, ctx, true)) {
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbo);
	glFlush();
	dispatch_sync(dispatch_get_main_queue(), ^{
		[(id<CanSetContentsChanged>)ctx->layer setContentsChanged];
	});
}

#endif