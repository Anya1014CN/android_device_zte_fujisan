#include <jni.h>

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *, void *) {
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_zte_camera_imageprocess_JPEGEncodeHW_JPEG_1encode_1hw(
        JNIEnv *, jclass, jbyteArray, jint, jint, jint, jint, jcharArray, jbyteArray) {
    return 0;
}
