#include <jni.h>

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *, void *) {
    return JNI_VERSION_1_6;
}

#define ZTE_NIGHT_STUB(name, sig) \
    extern "C" JNIEXPORT jint JNICALL name sig { return 0; }

ZTE_NIGHT_STUB(Java_com_zte_camera_imageprocess_ArcsoftNightProcess_1Jni_NightShot_1Create,
               (JNIEnv *, jclass))
ZTE_NIGHT_STUB(Java_com_zte_camera_imageprocess_ArcsoftNightProcess_1Jni_NightShot_1Destroy,
               (JNIEnv *, jclass))
ZTE_NIGHT_STUB(Java_com_zte_camera_imageprocess_ArcsoftNightProcess_1Jni_NightShot_1InitVariables,
               (JNIEnv *, jclass, jint))
ZTE_NIGHT_STUB(Java_com_zte_camera_imageprocess_ArcsoftNightProcess_1Jni_NightShot_1ProcessData,
               (JNIEnv *, jclass, jbyteArray))
ZTE_NIGHT_STUB(Java_com_zte_camera_imageprocess_ArcsoftNightProcess_1Jni_NightShot_1SendData,
               (JNIEnv *, jclass, jbyteArray, jint, jint))
ZTE_NIGHT_STUB(Java_com_zte_camera_imageprocess_ArcsoftNightProcess_1Jni_NightShot_1SetParam,
               (JNIEnv *, jclass, jint, jint, jint, jint))
