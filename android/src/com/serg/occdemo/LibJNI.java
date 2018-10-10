package com.serg.occdemo;

import android.content.res.AssetManager;

public class LibJNI {
    static {
        System.loadLibrary("raymark_lib"); 
    }
    
    public static int Error = -1;
    public static int Loading_Started = 0;
    public static int Loading_Finished = 1;
    public static int Warmup_Started = 2;
    public static int Warmup_Finished = 3;
    public static int Test_Started = 4;
    public static int Test_Finished = 5;
   
    public static native void Init(int w, int h, AssetManager asset_manager);
    public static native void Destroy();
    
    public static native int GetState();
    
    public static native void AsyncLoadScene(String name);
    public static native void AsyncWarmup(int time_limit_s);
    public static native void AsyncStartTest();
    public static native boolean QueryTestStatus(int[] updated_regions);
    public static native void FillPixelsRect(int[] pixels, int x, int y, int w, int h);
    public static native int GetTestResult();
}
