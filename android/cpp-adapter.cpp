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

static jobject g_fptr = nullptr;

extern "C"
JNIEXPORT void JNICALL
Java_com_atol_AtolModule_nativePassFptrToCpp(JNIEnv *env, jclass clazz) {
    jclass atolClass = env->FindClass("com/atol/AtolModule");
    jmethodID getFptrMethod = env->GetStaticMethodID(atolClass, "getFptr", "()Lru/atol/drivers10/fptr/Fptr;");
    jobject localFptr = env->CallStaticObjectMethod(atolClass, getFptrMethod);

    if (g_fptr != nullptr) {
        env->DeleteGlobalRef(g_fptr); // очистка старой ссылки
    }
    g_fptr = env->NewGlobalRef(localFptr); // сохраняем как глобальную ссылку
}
