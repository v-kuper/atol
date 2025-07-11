#include <jni.h>
#include <jsi/jsi.h>
#include "../cpp/atol.h"

using namespace facebook;

extern "C"
JNIEXPORT void JNICALL
Java_com_atol_AtolModule_nativeInstall(JNIEnv *env, jclass clazz, jlong jsiPtr) {
    jsi::Runtime* runtime = reinterpret_cast<jsi::Runtime*>(jsiPtr);
    atoljsi::install(*runtime);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_atol_AtolModule_nativeInitFptrJNI(JNIEnv *env, jclass clazz) {
    // Пустая функция инициализации, если нужна
}
