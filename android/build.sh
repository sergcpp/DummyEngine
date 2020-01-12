#!/bin/bash

prev_path=$(pwd)
cd "$(dirname "$0")"

rm -rf "bin"
rm -rf "gen"
#rm -rf "assets"
mkdir "bin"
mkdir "gen"
mkdir -p "assets"

ANDROID_REV=android-29
ANDROID_BUILD_TOOLS_VERS=29.0.2
ANDROID_AAPT_ADD="$ANDROID_HOME/build-tools/$ANDROID_BUILD_TOOLS_VERS/aapt add -v"
ANDROID_AAPT_PACK="$ANDROID_HOME/build-tools/$ANDROID_BUILD_TOOLS_VERS/aapt package -v -f -I $ANDROID_HOME/platforms/$ANDROID_REV/android.jar"
ANDROID_DX="$ANDROID_HOME/build-tools/$ANDROID_BUILD_TOOLS_VERS/dx --dex --verbose"
ANDROID_ZIPALIGN="$ANDROID_HOME/build-tools/$ANDROID_BUILD_TOOLS_VERS/zipalign"
JAVAC="$JAVA_HOME/bin/javac -classpath $ANDROID_HOME/platforms/$ANDROID_REV/android.jar"

APP_NAME=DummyApp
JAVAC_BUILD="$JAVAC -source 1.7 -target 1.7 -bootclasspath $JAVA_HOME/jre/lib/rt.jar -sourcepath 'src;gen;libs' -d ./bin"

#cp -rvu ../assets_android/* ./assets/
rsync -av --progress ../assets_android/* ./assets --exclude textures/bistro --exclude models/bistro --exclude models/new_test

$JAVAC_BUILD ./src/com/serg/dummyapp/*.java

$ANDROID_AAPT_PACK -M "AndroidManifest.xml" -A "assets" -S "res" -m -J "gen" -F "./bin/resources.ap_"

$ANDROID_DX --output="./bin/classes.dex" ./bin

cp -rv "./bin/resources.ap_" "./bin/$APP_NAME.ap_"

cd bin
$ANDROID_AAPT_ADD "$APP_NAME.ap_" "classes.dex"
cd ..

$ANDROID_AAPT_ADD "./bin/$APP_NAME.ap_" "lib/arm64-v8a/libDummyApp.so"

$JAVA_HOME/bin/jarsigner -keystore "./keystore/test_key.keystore" -storepass "123456" -keypass "123456" -tsa "http://timestamp.comodoca.com/rfc3161" -sigalg SHA1withRSA -digestalg SHA1 -signedjar "./bin/$APP_NAME.ap_" "./bin/$APP_NAME.ap_" "Test"

$JAVA_HOME/bin/jarsigner -keystore "./keystore/test_key.keystore" -verify "./bin/$APP_NAME.ap_"

$ANDROID_ZIPALIGN -f -v 4 "./bin/$APP_NAME.ap_" "./bin/$APP_NAME.apk"

#:EXIT
cd $prev_path
#ENDLOCAL
#exit /b %ERRORLEVEL%