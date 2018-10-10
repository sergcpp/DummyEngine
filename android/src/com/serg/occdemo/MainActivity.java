package com.serg.occdemo;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

import android.content.Context;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;

import java.io.File;

public class MainActivity extends Activity {
    MainView view_;

    @Override protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        view_ = new MainView(getApplication());
        setContentView(view_);
    }

    @Override protected void onPause() {
        super.onPause();
        view_.onPause();
    }

    @Override protected void onResume() {
        super.onResume();
        view_.onResume();
    }
}

class MainView extends GLSurfaceView {
    public MainView(Context context) {
        super(context);
        // Pick an EGLConfig with RGB8 color, 16-bit depth, no stencil,
        // supporting OpenGL ES 2.0 or later backwards-compatible versions.
        setEGLConfigChooser(8, 8, 8, 0, 16, 0);
        setEGLContextClientVersion(3);
        setRenderer(new Renderer());
    }

    private static class Renderer implements GLSurfaceView.Renderer {
        public void onDrawFrame(GL10 gl) {
            //GLES3JNILib.step();
        }

        public void onSurfaceChanged(GL10 gl, int width, int height) {
            //GLES3JNILib.resize(width, height);
        }

        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            //GLES3JNILib.init();
        }
    }
}