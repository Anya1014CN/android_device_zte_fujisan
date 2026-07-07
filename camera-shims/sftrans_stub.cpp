#include <jni.h>

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *, void *) {
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_zte_camera_carrier_DirectRenderer_enter(JNIEnv *, jclass) {
}

extern "C" JNIEXPORT void JNICALL
Java_com_zte_camera_carrier_DirectRenderer_render(
        JNIEnv *, jclass, jbyteArray, jint, jint, jint) {
}

extern "C" JNIEXPORT void JNICALL
Java_com_zte_camera_carrier_DirectRenderer_renderIntArray(
        JNIEnv *, jclass, jintArray, jint, jint, jint) {
}

extern "C" JNIEXPORT void JNICALL
Java_com_zte_camera_carrier_DirectRenderer_stop(JNIEnv *, jclass) {
}
