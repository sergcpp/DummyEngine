package com.serg.occdemo;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

import android.content.Context;
import android.content.res.AssetManager;
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
        
        AssetManager asset_manager = getAssets();
        
        view_ = new MainView(getApplication(), asset_manager);
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
    public MainView(Context context, AssetManager am) {
        super(context);
        // Pick an EGLConfig with RGB8 color, 16-bit depth, no stencil,
        // supporting OpenGL ES 2.0 or later backwards-compatible versions.
        setEGLConfigChooser(8, 8, 8, 0, 16, 0);
        setEGLContextClientVersion(3);
        setRenderer(new Renderer(am));
    }

    private static class Renderer implements GLSurfaceView.Renderer {
        private AssetManager am_;
        
        public Renderer(AssetManager am) {
            am_ = am;
        }
        
        public void onDrawFrame(GL10 gl) {
            gl.glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
            gl.glClear(GL10.GL_COLOR_BUFFER_BIT | GL10.GL_DEPTH_BUFFER_BIT);
            //GLES3JNILib.step();
        }

        public void onSurfaceChanged(GL10 gl, int width, int height) {
            //GLES3JNILib.resize(width, height);
        }

        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            LibJNI.Init(256, 256, am_);
        }
    }
}