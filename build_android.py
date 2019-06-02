
import os
import shutil

def main():
    ANDROID_HOME = os.environ['ANDROID_HOME']
    #JAVA_HOME = os.environ['JAVA_HOME']
    NDK_ROOT = os.environ['NDK_ROOT']
    
    #archs = [ "armeabi", "armeabi-v7a", "arm64-v8a", "x86", "x86_64" ]
    #archs = [ "armeabi-v7a", "arm64-v8a", "x86", "x86_64" ]
    #archs = [ "arm64-v8a", "x86" ]
    archs = [ "arm64-v8a" ]
    #archs = [ "x86_64" ]
    
    base_cmake_cmd = '"' + ANDROID_HOME + '\\cmake\\3.6.4111459\\bin\\cmake"' + ' -G"Android Gradle - Ninja" .. -DANDROID_NDK="' + NDK_ROOT + '" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="' + ANDROID_HOME + '\\cmake\\3.6.4111459\\bin\\ninja" -DCMAKE_TOOLCHAIN_FILE="' + NDK_ROOT + '\\build\\cmake\\android.toolchain.cmake" -DANDROID_NATIVE_API_LEVEL=9 -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static -DANDROID_CPP_FEATURES="rtti exceptions" -DANDROID_TOOLCHAIN=clang '
    
    base_build_cmd = '"' + ANDROID_HOME + '\\cmake\\3.6.4111459\\bin\\ninja" '
    
    # -DANDROID_ABI=armeabi-v7a
    
    # prepare assets
    os.system('DemoApp --prepare_assets android --norun')
    
    # build native part
    for arch in archs:
        if not os.path.exists("build-android-" + arch):
            os.makedirs("build-android-" + arch)
            os.chdir("build-android-" + arch)
            os.system('"' + base_cmake_cmd + '-DANDROID_ABI=' + arch + (' -DANDROID_ARM_NEON=TRUE' if arch == 'armeabi-v7a' else '') + '"')
            os.chdir('..')
        
        os.chdir("build-android-" + arch)
        ret = os.system('"' + base_build_cmd + 'DemoApp' + '"')
        
        if ret != 0:
            return
        
        os.chdir('..')
        
        if not os.path.exists("android\\lib\\" + arch + "\\"):
            os.makedirs("android\\lib\\" + arch + "\\")
            
        shutil.copy2("build-android-" + arch + "\\src\\DemoApp\\libDemoApp.so", "android\\lib\\" + arch + "\\")
        
    # build java part
    os.system("android\\build.bat")
    shutil.copy2("android\\bin\\OccDemo.apk", "OccDemo.apk")
    
main()