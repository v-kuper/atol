#pragma once

#include <jsi/jsi.h>
#include <jni.h>

// Declare the JavaVM getter function
extern JavaVM* getJavaVM();

// Declare the global fptr getter function
extern jobject getGlobalFptr();

namespace atoljsi {
    void install(facebook::jsi::Runtime &rt);
}
