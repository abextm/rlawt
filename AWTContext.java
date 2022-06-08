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
package net.runelite.rlawt;

import java.awt.Component;
import java.lang.annotation.Native;

public final class AWTContext
{
	@Native
	private long instance;

	@Native
	private int framebuffer;

	private static native long create0(Component component);
	public AWTContext(Component component)
	{
		this.instance = create0(component);
		if (instance == 0)
		{
			throw new NullPointerException();
		}
	}

	public native void configureInsets(int x, int y);
	public native void configurePixelFormat(int alpha, int depth, int stencil);
	public native void configureMultisamples(int samples);

	public native void destroy();

	public native void createGLContext();

	public native int setSwapInterval(int interval);

	public native void makeCurrent();
	public native void detachCurrent();

	public native void swapBuffers();

	public native long getGLContext();

	public native long getCGLShareGroup();
	public native long getGLXDisplay();
	public native long getWGLHDC();
}