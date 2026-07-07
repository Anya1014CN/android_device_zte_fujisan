#include <jni.h>

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *, void *) {
    return JNI_VERSION_1_6;
}

#define ZTE_INT_STUB(name, sig) \
    extern "C" JNIEXPORT jint JNICALL name sig { return 0; }

ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_DoScalingNV21,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTERGBAToYAUVNV21Format,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint, jbyteArray))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTERGBAToYUVNV21,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEWaterMixProcessByNV21,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint, jint, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEYUV420toARGB8888,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEYUV420toRGBA8888,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEYUVModifyYvutoYuv,
             (JNIEnv *, jclass, jbyteArray, jint, jint, jbyteArray))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEYUVN12DePadding,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEYUVN12NoPaddingRotation,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEYUVNV21DataCrop,
             (JNIEnv *, jclass, jbyteArray, jint, jint, jbyteArray, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEdeInitWaterProcessing,
             (JNIEnv *, jclass))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEdoScalingNV21WithPadding,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEdoScalingNV21WithoutPadding,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEinitWaterProcessing,
             (JNIEnv *, jclass, jint, jint, jbyteArray, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEjeneralTrailsProcessWithYUVPadding,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTErgb2yuv,
             (JNIEnv *, jclass, jbyteArray, jint, jint, jobject))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEsimpleWaterMixProcessByNV21,
             (JNIEnv *, jclass, jbyteArray, jint, jint, jint, jint, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEstarTrailsProcessWithYUVPadding,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEwaterTrailsAverageProcessWithYUV,
             (JNIEnv *, jclass, jbyteArray, jint, jint, jbyteArray))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEwaterTrailsProcessWithYUVPadding,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jint, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_ZTEwaterTrailsSumProcessWithYUV,
             (JNIEnv *, jclass, jbyteArray, jint, jint, jbyteArray))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_alphaExposureImageProcess,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jbyteArray, jint, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_backgroundExposureImageProcess,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jbyteArray, jbyteArray, jint, jint))
ZTE_INT_STUB(Java_com_zte_camera_imageprocess_ZTEImagePost_multiExposureBase,
             (JNIEnv *, jclass, jbyteArray, jbyteArray, jbyteArray, jint, jint, jint))
