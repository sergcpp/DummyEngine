package com.serg.occdemo;

import android.content.res.AssetManager;

public class LibJNI {
    static {
        System.loadLibrary("DemoApp"); 
    }
   
    public static native void Init(int w, int h, AssetManager asset_manager);
    public static native void Destroy();
    public static native void Frame();
    public static native void AddEvent(int type, int key, float x, float y);
}
