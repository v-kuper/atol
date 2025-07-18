#include <jni.h>
#include <jsi/jsi.h>
#include "../cpp/atol.h"

using namespace facebook;

// Global variables
static jobject g_fptr = nullptr;
static JavaVM* g_javaVM = nullptr;

// Function to get JavaVM
JavaVM* getJavaVM() {
    return g_javaVM;
}

// Function to get global fptr
jobject getGlobalFptr() {
    return g_fptr;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_atol_AtolModule_nativeInstall(JNIEnv *env, jclass clazz, jlong jsiPtr) {
    // Store JavaVM for later use
    if (g_javaVM == nullptr) {
        env->GetJavaVM(&g_javaVM);
    }

    jsi::Runtime* runtime = reinterpret_cast<jsi::Runtime*>(jsiPtr);
    atoljsi::install(*runtime);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_atol_AtolModule_nativePassFptrToCpp(JNIEnv *env, jclass clazz) {
    // Store JavaVM for later use
    if (g_javaVM == nullptr) {
        env->GetJavaVM(&g_javaVM);
    }

    jclass atolClass = env->FindClass("com/atol/AtolModule");
    jmethodID getFptrMethod = env->GetStaticMethodID(atolClass, "getFptr", "()Lru/atol/drivers10/fptr/IFptr;");
    jobject localFptr = env->CallStaticObjectMethod(atolClass, getFptrMethod);

    if (g_fptr != nullptr) {
        env->DeleteGlobalRef(g_fptr); // cleanup old reference
    }
    g_fptr = env->NewGlobalRef(localFptr); // save as global reference
}
