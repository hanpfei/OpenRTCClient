
#pragma once

#include <jni.h>

namespace awaudio {
namespace jni {

jint InitGlobalJniVariables(JavaVM* jvm);

// Return a |JNIEnv*| usable on this thread or NULL if this thread is detached.
JNIEnv* GetEnv();

JavaVM* GetJVM();

// Return a |JNIEnv*| usable on this thread.  Attaches to `g_jvm` if necessary.
JNIEnv* AttachCurrentThreadIfNeeded();

}  // namespace jni
}  // namespace awaudio