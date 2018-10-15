#include <jni.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <Sys/AssetFile.h>
#include "DemoApp.h"

#include "com_serg_occdemo_LibJNI.h"

// "C:\Program Files\Java\jdk1.8.0_171\bin\javah.exe" -jni -classpath C:/Users/MA/AppData/Local/Android/Sdk/platforms/android-21/android.jar;./bin com.serg.occdemo.LibJNI

namespace {
    DemoApp *g_app = nullptr;
}

extern "C" JNIEXPORT void JNICALL Java_com_serg_occdemo_LibJNI_Init(JNIEnv *env, jclass, jint w, jint h, jobject am) {
    AAssetManager *asset_mgr = AAssetManager_fromJava(env, am);
    Sys::AssetFile::InitAssetManager(asset_mgr);
    
    g_app = new DemoApp();
    g_app->Init((int)w, (int)h);
}

extern "C" JNIEXPORT void JNICALL Java_com_serg_occdemo_LibJNI_Destroy(JNIEnv *, jclass) {
    
}

extern "C" JNIEXPORT void JNICALL Java_com_serg_occdemo_LibJNI_Frame(JNIEnv *, jclass) {
    g_app->Frame();
}

extern "C" JNIEXPORT void JNICALL Java_com_serg_occdemo_LibJNI_Resize(JNIEnv *, jclass, jint w, jint h) {
    g_app->Resize(int(w), int(h));
}

extern "C" JNIEXPORT void JNICALL Java_com_serg_occdemo_LibJNI_AddEvent(JNIEnv *, jclass, jint type, jint key, jfloat x, jfloat y, jfloat dx, jfloat dy) {
    g_app->AddEvent((int)type, (int)key, (float)x, (float)y, (float)dx, (float)dy);
}

