package com.serg.dummyapp;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
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
    private MainView view_;

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
    private static final int INVALID_POINTER_ID = -1;
    private int primary_id_ = INVALID_POINTER_ID,
                secondary_id_ = INVALID_POINTER_ID;
    private float prev_prim_x_ = 0.0f;
    private float prev_prim_y_ = 0.0f;
    private float prev_sec_x_ = 0.0f;
    private float prev_sec_y_ = 0.0f;
    
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
    
    @Override
    public boolean onTouchEvent(MotionEvent e) {
        final int RAW_INPUT_P1_DOWN     = 1;
        final int RAW_INPUT_P1_UP       = 2;
        final int RAW_INPUT_P1_MOVE     = 3;
        final int RAW_INPUT_P2_DOWN     = 4;
        final int RAW_INPUT_P2_UP       = 5;
        final int RAW_INPUT_P2_MOVE     = 6;
        final int RAW_INPUT_KEY_DOWN    = 7;
        final int RAW_INPUT_KEY_UP      = 8;
        final int RAW_INPUT_RESIZE      = 9;
        final int RAW_INPUT_MOUSE_WHEEL = 10;

        final int action = e.getAction();
        switch (action & MotionEvent.ACTION_MASK) {
            case MotionEvent.ACTION_DOWN: {
                float x = e.getX(0);
                float y = e.getY(0);
                LibJNI.AddEvent(RAW_INPUT_P1_DOWN, 0, x, y, 0.0f, 0.0f);
                prev_prim_x_ = x;
                prev_prim_y_ = y;
                primary_id_ = e.getPointerId(0);
            } break;
            case MotionEvent.ACTION_POINTER_DOWN: {
                if (secondary_id_ == INVALID_POINTER_ID && e.getActionIndex() == 1) {
                    float x = e.getX(1);
                    float y = e.getY(1);
                    LibJNI.AddEvent(RAW_INPUT_P2_DOWN, 0, x, y, 0.0f, 0.0f);
                    prev_sec_x_ = x;
                    prev_sec_y_ = y;
                    secondary_id_ = e.getPointerId(1);
                }
            } break;
            case MotionEvent.ACTION_UP: {
                final int pointer_index = e.getActionIndex();
                final float x = e.getX(pointer_index);
                final float y = e.getY(pointer_index);
                if (primary_id_ == e.getPointerId(pointer_index)) {
                    LibJNI.AddEvent(RAW_INPUT_P1_UP, 0, x, y, 0.0f, 0.0f);
                    primary_id_ = INVALID_POINTER_ID;
                } else if (secondary_id_ == e.getPointerId(pointer_index)) {
                    LibJNI.AddEvent(RAW_INPUT_P2_UP, 0, x, y, 0.0f, 0.0f);
                    secondary_id_ = INVALID_POINTER_ID;
                }
            } break;
            case MotionEvent.ACTION_POINTER_UP: {
                final int pointer_index = e.getActionIndex();
                final float x = e.getX(pointer_index);
                final float y = e.getY(pointer_index);
                if (primary_id_ == e.getPointerId(pointer_index)) {
                    LibJNI.AddEvent(RAW_INPUT_P1_UP, 0, x, y, 0.0f, 0.0f);
                    primary_id_ = INVALID_POINTER_ID;
                } else if (secondary_id_ == e.getPointerId(pointer_index)) {
                    LibJNI.AddEvent(RAW_INPUT_P2_UP, 0, x, y, 0.0f, 0.0f);
                    secondary_id_ = INVALID_POINTER_ID;
                }
            } break;
            case MotionEvent.ACTION_MOVE:
                if (primary_id_ != INVALID_POINTER_ID) {
                    final int pointer_index = e.findPointerIndex(primary_id_);
                    if (pointer_index != INVALID_POINTER_ID) {
                        float x = e.getX(pointer_index);
                        float y = e.getY(pointer_index);
                        
                        LibJNI.AddEvent(RAW_INPUT_P1_MOVE, 0, x, y, x - prev_prim_x_, y - prev_prim_y_);
                        prev_prim_x_ = x;
                        prev_prim_y_ = y;
                    }
                }
                
                if (secondary_id_ != INVALID_POINTER_ID) {
                    final int pointer_index = e.findPointerIndex(secondary_id_);
                    if (pointer_index != INVALID_POINTER_ID) {
                        float x = e.getX(pointer_index);
                        float y = e.getY(pointer_index);
                        
                        LibJNI.AddEvent(RAW_INPUT_P2_MOVE, 0, x, y, x - prev_sec_x_, y - prev_sec_y_);
                        prev_sec_x_ = x;
                        prev_sec_y_ = y;
                    }
                }
            break;
            default:
            break;
        }
        
        return true;
    }
}