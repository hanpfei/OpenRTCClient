
#include <jni.h>
#include <sys/types.h>

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <thread>

#include "android_debug.h"
#include "jni_helper.h"

static JavaVM *jvm;

static std::atomic<bool> stopped = {false};
static std::unique_ptr<std::thread> native_thread;
static jobject javaObject;
static jfieldID intStaticFieldID;
static jfieldID intFieldID;
static jmethodID staticMethodID;
static jmethodID methodID;
static jmethodID excepMethodID;

JNIEXPORT jboolean JNICALL JniUtils_booleanParam(JNIEnv* env,
                                                 jclass type,
                                                 jboolean value) {
    LOGI("Boolean param value %d", value);
    return !value;
}

JNIEXPORT jbyte JNICALL JniUtils_byteParam(JNIEnv* env,
                                              jclass type,
                                              jbyte value) {
    LOGI("Byte param value %d", value);
    return value + 20;
}

JNIEXPORT jchar JNICALL JniUtils_charParam(JNIEnv* env,
                                           jclass type,
                                           jint value) {
    LOGI("Char param value %d", value);
    return value + 20;
}

JNIEXPORT jshort JNICALL JniUtils_shortParam(JNIEnv* env,
                                             jclass type,
                                             jshort value) {
    LOGI("Short param value %d", value);
    return value + 20;
}

JNIEXPORT jint JNICALL JniUtils_intParam(JNIEnv* env,
                                         jclass type,
                                         jint value) {
    LOGI("Int param value %d", value);
    return value + 20;
}

JNIEXPORT jlong JNICALL JniUtils_longParam(JNIEnv* env,
                                           jclass type,
                                           jlong value) {
    LOGI("Long param value %ld", value);
    return value + 20;
}

JNIEXPORT jfloat JNICALL JniUtils_floatParam(JNIEnv* env,
                                             jclass type,
                                             jfloat value) {
    LOGI("Float param value %f", value);
    return value + 20.0f;
}

JNIEXPORT jdouble JNICALL JniUtils_doubleParam(JNIEnv* env,
                                               jclass type,
                                               jdouble value) {
    LOGI("Double param value %f", value);
    return value + 20.0;
}

JNIEXPORT jboolean JNICALL JniUtils_voidParam(JNIEnv* env,
                                              jclass type) {
    LOGI("Void param value");
    return true;
}

JNIEXPORT jintArray JNICALL JniUtils_intArrayParam(JNIEnv* env,
                                                  jobject thisObject,
                                                  jintArray intArray) {

    jsize array_length = env->GetArrayLength(intArray);
    LOGI("array_length %d", array_length);
    jint *data_elems = env->GetIntArrayElements(intArray, NULL);

    std::unique_ptr<jint[]> localArray(new jint[array_length]);
    for (int i = 0; i < array_length; ++i) {
        localArray[i] = data_elems[i] + 30;
    }
    jintArray localJArray = env->NewIntArray(array_length);
    env->SetIntArrayRegion(localJArray, 0, array_length, localArray.get());

    env->ReleaseIntArrayElements(intArray, data_elems, 0);
    return localJArray;
}

JNIEXPORT jstring JNICALL JniUtils_stringParam(JNIEnv* env,
                                               jobject thisObject,
                                               jstring str) {
    const char* nativeStr = env->GetStringUTFChars(str, NULL);
    LOGI("String got from java %s", nativeStr);
    jstring strFromNative = env->NewStringUTF("String from native");
    env->ReleaseStringUTFChars(str, nativeStr);

    return strFromNative;
}

JNIEXPORT jstring JNICALL JniUtils_methodThrowException(JNIEnv* env,
                                               jobject thisObject,
                                               jstring str) {
    const char* nativeStr = env->GetStringUTFChars(str, NULL);
    LOGI("String got from java %s", nativeStr);
    jstring strFromNative = env->NewStringUTF("String from native");
    env->ReleaseStringUTFChars(str, nativeStr);

    jclass cls;
    cls = env->FindClass("java/lang/RuntimeException");
    if(cls){
        env->ThrowNew(cls,"Exception from native");
    }

    return strFromNative;
}

static void thread_func() {
    int count = 0;
    LOGI("Message from native thread");
    while (!stopped.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        LOGI("Message from native thread %d", count);
        ++count;
    }

    JNIEnv *jniEnv;
    jvm->AttachCurrentThread(&jniEnv, NULL);
    jclass clsObject = jniEnv->GetObjectClass(javaObject);

    jint valueIntStatic = jniEnv->GetStaticIntField(clsObject, intStaticFieldID);
    jint valueInt = jniEnv->GetIntField(javaObject, intFieldID);

    LOGI("thread_func static int field %d, int field %d", valueIntStatic, valueInt);

    jniEnv->SetStaticIntField(clsObject, intStaticFieldID, 35791);
    jniEnv->SetIntField(javaObject, intFieldID, 19753);

    valueIntStatic = jniEnv->GetStaticIntField(clsObject, intStaticFieldID);
    valueInt = jniEnv->GetIntField(javaObject, intFieldID);

    LOGI("thread_func (2): static int field %d, int field %d", valueIntStatic, valueInt);

    jniEnv->CallObjectMethod(javaObject, methodID);
    jniEnv->CallStaticObjectMethod(clsObject, staticMethodID);

    jniEnv->CallObjectMethod(javaObject, excepMethodID);
    jthrowable exception = jniEnv->ExceptionOccurred();
    if (exception != NULL) {
        // 处理异常
        jniEnv->ExceptionDescribe();
        jniEnv->ExceptionClear();
    }

    jvm->DetachCurrentThread();
    LOGI("Thread stopped");
}

JNIEXPORT jboolean JNICALL JniUtils_startNativeThread(JNIEnv* env,
                                                      jobject thisObject,
                                                      jint param) {
    jclass clsObject = env->GetObjectClass(thisObject);
    intStaticFieldID = env->GetStaticFieldID(clsObject, "intFieldStatic", "I");
    intFieldID = env->GetFieldID(clsObject, "intField", "I");

    staticMethodID = env->GetStaticMethodID(clsObject, "accessFromNativeStatic", "()Ljava/lang/String;");
    methodID = env->GetMethodID(clsObject, "accessFromNative", "()Ljava/lang/String;");

    excepMethodID = env->GetMethodID(clsObject, "accessFromNativeException", "()Ljava/lang/String;");

    jint valueIntStatic = env->GetStaticIntField(clsObject, intStaticFieldID);
    jint valueInt = env->GetIntField(thisObject, intFieldID);

    LOGI("startNativeThread param %d, static int field %d, int field %d", param, valueIntStatic, valueInt);

    javaObject = env->NewGlobalRef(thisObject);

    if (!native_thread) {
        native_thread = std::make_unique<std::thread>(thread_func);
    }

    return true;
}

JNIEXPORT jboolean JNICALL JniUtils_stopNativeThread(JNIEnv* env,
                                                     jobject thisObject) {
    LOGI("stopNativeThread");
    stopped = true;
    if (native_thread) {
        native_thread->join();
    }
    native_thread.reset();

    env->DeleteGlobalRef(javaObject);

    return true;
}

// Dalvik VM type signatures
static const JNINativeMethod gMethods[] = {
        NATIVE_METHOD(JniUtils, booleanParam, "(Z)Z"),
        NATIVE_METHOD(JniUtils, byteParam, "(B)B"),
        NATIVE_METHOD(JniUtils, charParam, "(C)C"),
        NATIVE_METHOD(JniUtils, shortParam, "(S)S"),
        NATIVE_METHOD(JniUtils, intParam, "(I)I"),
        NATIVE_METHOD(JniUtils, longParam, "(J)J"),
        NATIVE_METHOD(JniUtils, floatParam, "(F)F"),
        NATIVE_METHOD(JniUtils, doubleParam, "(D)D"),
        NATIVE_METHOD(JniUtils, voidParam, "()Z"),
        NATIVE_METHOD(JniUtils, intArrayParam, "([I)[I"),
        NATIVE_METHOD(JniUtils, stringParam, "(Ljava/lang/String;)Ljava/lang/String;"),
        NATIVE_METHOD(JniUtils, methodThrowException, "(Ljava/lang/String;)Ljava/lang/String;"),

        NATIVE_METHOD(JniUtils, startNativeThread, "(I)Z"),
        NATIVE_METHOD(JniUtils, stopNativeThread, "()Z"),
};

int jniRegisterNativeMethods(JNIEnv* env,
                             const char* classPathName,
                             const JNINativeMethod* nativeMethods,
                             jint nMethods) {
    jclass clazz;
    clazz = env->FindClass(classPathName);
    if (clazz == NULL) {
        LOGW("Native registration unable to find class '%s'", classPathName);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, nativeMethods, nMethods) < 0) {
        LOGW("RegisterNatives failed for '%s'", classPathName);
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM* vm, void* /* reserved */) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("JavaVM::GetEnv() failed");
        abort();
    }
    jniRegisterNativeMethods(env, "com/example/jnitest/JniUtils", gMethods,
                             NELEM(gMethods));
    jvm = vm;
    return JNI_VERSION_1_6;
}