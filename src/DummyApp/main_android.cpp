#include <jni.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <Sys/AssetFile.h>
#include "DummyApp.h"

#include "com_serg_dummyapp_LibJNI.h"

// "C:\Program Files\Java\jdk1.8.0_171\bin\javah.exe" -jni -classpath C:/Users/MA/AppData/Local/Android/Sdk/platforms/android-21/android.jar;./bin com.serg.dummyapp.LibJNI

namespace {
    DummyApp *g_app = nullptr;
}

// dummy value for now
unsigned int g_android_local_ip_address = 0;

extern "C" JNIEXPORT void JNICALL Java_com_serg_dummyapp_LibJNI_Init(JNIEnv *env, jclass, jint w, jint h, jobject am) {
    AAssetManager *asset_mgr = AAssetManager_fromJava(env, am);
    Sys::AssetFile::InitAssetManager(asset_mgr);
    
    g_app = new DummyApp();
    g_app->Init((int)w, (int)h);
}

extern "C" JNIEXPORT void JNICALL Java_com_serg_dummyapp_LibJNI_Destroy(JNIEnv *, jclass) {
    
}

extern "C" JNIEXPORT void JNICALL Java_com_serg_dummyapp_LibJNI_Frame(JNIEnv *, jclass) {
    g_app->Frame();
}

extern "C" JNIEXPORT void JNICALL Java_com_serg_dummyapp_LibJNI_Resize(JNIEnv *, jclass, jint w, jint h) {
    g_app->Resize(int(w), int(h));
}

extern "C" JNIEXPORT void JNICALL Java_com_serg_dummyapp_LibJNI_AddEvent(JNIEnv *, jclass, jint type, jint key, jfloat x, jfloat y, jfloat dx, jfloat dy) {
    g_app->AddEvent((int)type, (uint32_t)key, (float)x, (float)y, (float)dx, (float)dy);
}

