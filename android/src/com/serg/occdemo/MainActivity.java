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
import android.view.View;

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
        
        /*requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, 
                             WindowManager.LayoutParams.FLAG_FULLSCREEN);*/
        //getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
        
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
    
    @Override
    public void onWindowFocusChanged(boolean has_focus) {
        super.onWindowFocusChanged(has_focus);
        if (has_focus) {
            //getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
            HideSystemUI();
        }
    }
        
    private void HideSystemUI() {
    // Enables regular immersive mode.
    // For "lean back" mode, remove SYSTEM_UI_FLAG_IMMERSIVE.
    // Or for "sticky immersive," replace it with SYSTEM_UI_FLAG_IMMERSIVE_STICKY
    View decorView = getWindow().getDecorView();
    decorView.setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE
            // Set the content to appear under the system bars so that the
            // content doesn't resize when the system bars hide and show.
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            // Hide the nav bar and status bar
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_FULLSCREEN);
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
            LibJNI.Frame();
        }

        public void onSurfaceChanged(GL10 gl, int width, int height) {
            LibJNI.Resize(width, height);
        }

        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            LibJNI.Init(720, 405, am_);
        }
    }
}