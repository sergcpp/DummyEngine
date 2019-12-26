#!/usr/bin/env python3

import os
import shutil

def main():
    ANDROID_HOME = os.environ['ANDROID_HOME']
    #JAVA_HOME = os.environ['JAVA_HOME']
    NDK_ROOT = os.environ['NDK_ROOT']
    ANDROID_CMAKE = os.environ['ANDROID_CMAKE']
    
    #archs = [ "armeabi", "armeabi-v7a", "arm64-v8a", "x86", "x86_64" ]
    #archs = [ "armeabi-v7a", "arm64-v8a", "x86", "x86_64" ]
    #archs = [ "arm64-v8a", "x86" ]
    archs = [ "arm64-v8a" ]
    #archs = [ "x86_64" ]
    
    cmake_bin = os.path.join(ANDROID_CMAKE, 'bin', 'cmake');
    ninja_bin = os.path.join(ANDROID_CMAKE, 'bin', 'ninja');
    cmake_toolchain = os.path.join(NDK_ROOT, 'build', 'cmake', 'android.toolchain.cmake')

    #base_cmake_cmd = cmake_bin + ' -G"Android Gradle - Ninja" .. -DANDROID_NDK="' + NDK_ROOT + '" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="' + ninja_bin + '" -DCMAKE_TOOLCHAIN_FILE="' + cmake_toolchain + '" -DANDROID_NATIVE_API_LEVEL=9 -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static -DANDROID_CPP_FEATURES="rtti exceptions" -DANDROID_TOOLCHAIN=clang '
    base_cmake_cmd = cmake_bin + ' -G"Ninja" .. -DANDROID_NDK="' + NDK_ROOT + '" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="' + ninja_bin + '" -DCMAKE_TOOLCHAIN_FILE="' + cmake_toolchain + '" -DANDROID_NATIVE_API_LEVEL=9 -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static -DANDROID_CPP_FEATURES="rtti exceptions" -DANDROID_TOOLCHAIN=clang '

    # -DANDROID_ABI=armeabi-v7a
    
    # prepare assets
    app_bin = os.path.join(os.path.curdir, 'DummyApp');
    os.system(app_bin + ' --prepare_assets android --norun')
    
    # build native part
    for arch in archs:
        if not os.path.exists("build-android-" + arch):
            os.makedirs("build-android-" + arch)
            os.chdir("build-android-" + arch)
            os.system(base_cmake_cmd + '-DANDROID_ABI=' + arch + (' -DANDROID_ARM_NEON=TRUE' if arch == 'armeabi-v7a' else ''))      
            os.chdir('..')
        
        os.chdir("build-android-" + arch)
        ret = os.system(ninja_bin + ' DummyApp')
        
        if ret != 0:
            return
        
        os.chdir('..')
        
        if not os.path.exists(os.path.join('android', 'lib', arch)):
            os.makedirs(os.path.join('android', 'lib', arch))
            
        shutil.copy2(os.path.join(os.path.curdir, 'build-android-' + arch, 'src', 'DummyApp', 'libDummyApp.so'), os.path.join(os.path.curdir, 'android', 'lib', arch))
        
    # build java part
    if os.name == 'nt':
        os.system("android\\build.bat")
    else:
        os.system("android/build.sh")
    shutil.copy2(os.path.join('android', 'bin', 'DummyApp.apk'), 'DummyApp.apk')
    
main()