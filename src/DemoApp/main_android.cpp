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
    
}

extern "C" JNIEXPORT void JNICALL Java_com_serg_occdemo_LibJNI_AddEvent(JNIEnv *, jclass, jint, jint, jfloat, jfloat) {
    
}


#if 0

/*
#include "GameApp.h"

extern "C" {
    JNIEXPORT void JNICALL Java_com_yablokov_serg_sw_1demo_DemoLib_Init(JNIEnv *, jclass, jint, jint, jstring);
    JNIEXPORT void JNICALL Java_com_yablokov_serg_sw_1demo_DemoLib_Deinit(JNIEnv *, jclass);
    JNIEXPORT void JNICALL Java_com_yablokov_serg_sw_1demo_DemoLib_InitAssetManager(JNIEnv *, jclass, jobject);
    JNIEXPORT void JNICALL Java_com_yablokov_serg_sw_1demo_DemoLib_Step(JNIEnv *, jclass);
    JNIEXPORT void JNICALL Java_com_yablokov_serg_sw_1demo_DemoLib_AddEvent(JNIEnv *, jclass, jint, jint, jfloat, jfloat);
    JNIEXPORT jboolean JNICALL Java_com_yablokov_serg_sw_1demo_DemoLib_IsTerminated(JNIEnv *, jclass);
};

namespace {
    TestApp app;

    int w, h;
    const int RES = 256;

    const char constant_vs[] =  "attribute vec3 aVertexPosition;\n"
                                "attribute vec2 aVertexUVs;\n"
                                "varying vec2 aVertexUVs_;\n"
                                "void main(void)\n"
                                "{\n"
                                "    gl_Position = vec4(aVertexPosition, 1.0);\n"
                                "    aVertexUVs_ = aVertexUVs;\n"
                                "}";

    const char constant_fs[] =  "precision mediump float;\n"
                                "uniform sampler2D s_texture;\n"
                                "varying vec2 aVertexUVs_;\n"
                                "void main()\n"
                                "{\n"
                                //"    gl_FragColor = texture2D(s_texture, aVertexUVs_);\n"
                                "    vec4 col = texture2D(s_texture, aVertexUVs_);\n"
                                "    gl_FragColor = vec4(col.b, col.g, col.r, col.a);\n"
                                //"    gl_FragColor = vec4(1, aVertexUVs_[0], 0, 1);\n"
                                "}";

    GLuint fb_program, fb_tex;
    GLint a_pos_loc, a_uv_loc, u_tex_loc;

    int LoadShader(GLenum shader_type, const char *p_source) {
        GLuint shader = glCreateShader(shader_type);
        if (shader) {
            glShaderSource(shader, 1, &p_source, NULL);
            glCompileShader(shader);
            GLint compiled = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (!compiled) {
                GLint infoLen = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
                if (infoLen) {
                    char *buf = (char *) malloc(infoLen);
                    if (buf) {
                        glGetShaderInfoLog(shader, infoLen, NULL, buf);
                        LOGE("Could not compile shader %d: %s", shader_type, buf);
                        free(buf);
                    }
                    glDeleteShader(shader);
                    shader = 0;
                }
            }
        } else {
            LOGE("error");
        }
        return shader;
    }
}

JNIEXPORT void JNICALL Java_com_yablokov_serg_sw_1demo_DemoLib_Init(JNIEnv * env, jclass obj, jint width, jint height, jstring path) {*/
/*try {
    std::string dir;
    {
        const char *native_str = env->GetStringUTFChars(path, 0);
        dir = native_str;
        env->ReleaseStringUTFChars(path, native_str);
    }

    demo = new Demo(width, height);
    demo->Start();
} catch (std::exception &e) {
    LOGI("ERROR: %s", e.what());
}*/
/*
    w = RES * float(width) / height;//RES * roundf(float(width) / height);
    w = 2 * (w/2);
    h = RES;

    app.Init(w, h);

    GLint v_shader = LoadShader(GL_VERTEX_SHADER, constant_vs);
    GLint f_shader = LoadShader(GL_FRAGMENT_SHADER, constant_fs);

    fb_program = glCreateProgram();
    if (fb_program) {
        glAttachShader(fb_program, v_shader);
        glAttachShader(fb_program, f_shader);
        glLinkProgram(fb_program);
        GLint link_status = GL_FALSE;
        glGetProgramiv(fb_program, GL_LINK_STATUS, &link_status);
        if (link_status != GL_TRUE) {
            GLint buf_length = 0;
            glGetProgramiv(fb_program, GL_INFO_LOG_LENGTH, &buf_length);
            if (buf_length) {
                char* buf = (char*) malloc(buf_length);
                if (buf) {
                    glGetProgramInfoLog(fb_program, buf_length, NULL, buf);
                    LOGE("Could not link program: %s", buf);
                    free(buf);
                }
            }
            glDeleteProgram(fb_program);
            fb_program = 0;
        }
    } else {
        LOGE("error");
    }

    a_pos_loc = glGetAttribLocation(fb_program, "aVertexPosition");
    a_uv_loc= glGetAttribLocation(fb_program, "aVertexUVs");
    u_tex_loc = glGetUniformLocation(fb_program, "s_texture");

    glGenTextures(1, &fb_tex);
    glBindTexture(GL_TEXTURE_2D, fb_tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

JNIEXPORT void JNICALL Java_com_yablokov_serg_sw_1demo_DemoLib_Deinit(JNIEnv * env, jclass obj) {
    //delete demo; demo = nullptr;
    app.Destroy();
}

JNIEXPORT void JNICALL Java_com_yablokov_serg_sw_1demo_DemoLib_InitAssetManager(JNIEnv * env, jclass obj, jobject am) {
    AAssetManager *asset_mgr = AAssetManager_fromJava(env, am);
    AssetFile::InitAssetManager(asset_mgr);
}

//#include <cstdlib>

JNIEXPORT void JNICALL Java_com_yablokov_serg_sw_1demo_DemoLib_Step(JNIEnv * env, jclass obj){
    app.Frame();

    const void *pixels = swGetPixelDataRef(swGetCurFramebuffer());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    //delete[] img;

    //glClearColor(0, 0.2f, 0.2f, 1);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(fb_program);

    GLfloat tri[] = {-1, -1, 0,      0, 1,
                     1, 1, 0,        1, 0,
                     -1, 1, 0,       0, 0,

                     -1, -1, 0,      0, 1,
                     1, -1, 0,       1, 1,
                     1, 1, 0,        1, 0};


    glUniform1i(u_tex_loc, 0);

    glEnableVertexAttribArray(a_pos_loc);
    glVertexAttribPointer(a_pos_loc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), tri);
    glEnableVertexAttribArray(a_uv_loc);
    glVertexAttribPointer(a_uv_loc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), tri + 3);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}
*/
JNIEXPORT void JNICALL Java_com_yablokov_serg_sw_1demo_DemoLib_AddEvent(JNIEnv * env, jclass obj, jint type, jint key, jfloat x, jfloat y) {
    /*Input::Event evt;
    evt.type = (Input::RawInputEvent) type;
    evt.key = (Input::RawInputButton) key;
    evt.point.x = (float) x;
    evt.point.y = (float) y;
    evt.time_stamp = Time::GetTicks();
    Input::AddRawInputEvent(evt);*/
}
/*
JNIEXPORT jboolean JNICALL Java_com_yablokov_serg_sw_1demo_DemoLib_IsTerminated(JNIEnv *env, jclass obj) {
    //bool b = demo->terminated;
    //return (jboolean)b;
    return (jboolean)0;
}*/

#endif
