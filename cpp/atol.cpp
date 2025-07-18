#include "atol.h"
#include <jni.h>

using namespace facebook;

// External declarations from JNI bridge
extern JavaVM* getJavaVM();
extern jobject getGlobalFptr();

namespace atoljsi {

void install(jsi::Runtime &rt) {
    // Create main object that will contain all our functions
    auto atolObject = jsi::Object(rt);

    // =====================================================================
    // FUNCTION 1: reverseString - reverses string backwards
    // =====================================================================
    auto reverseString = jsi::Function::createFromHostFunction(
        rt,
        jsi::PropNameID::forAscii(rt, "reverseString"),
        1,
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count > 0) {
                auto input = arguments[0].asString(runtime).utf8(runtime);
                std::string reversed(input.rbegin(), input.rend());
                return jsi::String::createFromUtf8(runtime, reversed);
            }
            return jsi::Value::undefined();
        });

    // =====================================================================
    // FUNCTION 2: getNumbers - returns array [1, 2, 3]
    // =====================================================================
    auto getNumbers = jsi::Function::createFromHostFunction(
        rt,
        jsi::PropNameID::forAscii(rt, "getNumbers"),
        0,
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            auto array = jsi::Array(runtime, 3);
            array.setValueAtIndex(runtime, 0, jsi::Value(1));
            array.setValueAtIndex(runtime, 1, jsi::Value(2));
            array.setValueAtIndex(runtime, 2, jsi::Value(3));
            return array;
        });

    // =====================================================================
    // FUNCTION 3: getObject - returns object with module info
    // =====================================================================
    auto getObject = jsi::Function::createFromHostFunction(
        rt,
        jsi::PropNameID::forAscii(rt, "getObject"),
        0,
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            auto obj = jsi::Object(runtime);
            obj.setProperty(runtime, "name", jsi::String::createFromUtf8(runtime, "Atol"));
            obj.setProperty(runtime, "version", jsi::String::createFromUtf8(runtime, "1.0.0"));
            return obj;
        });

    // =====================================================================
    // FUNCTION 4: callMeLater - callback demonstration
    // =====================================================================
    auto callMeLater = jsi::Function::createFromHostFunction(
        rt,
        jsi::PropNameID::forAscii(rt, "callMeLater"),
        2,
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count >= 2) {
                auto successCallback = arguments[0].asObject(runtime).asFunction(runtime);
                successCallback.call(runtime);
            }
            return jsi::Value::undefined();
        });

    // =====================================================================
    // FUNCTION 5: promiseNumber - creates Promise that doubles number
    // =====================================================================
    auto promiseNumber = jsi::Function::createFromHostFunction(
        rt,
        jsi::PropNameID::forAscii(rt, "promiseNumber"),
        1,
        [](jsi::Runtime& runtime,
           const jsi::Value& /*thisValue*/,
           const jsi::Value* arguments,
           size_t count) -> jsi::Value {

            if (count == 0) return jsi::Value::undefined();

            double number = arguments[0].asNumber();
            auto Promise = runtime.global().getPropertyAsFunction(runtime, "Promise");

            auto promiseCtor = jsi::Function::createFromHostFunction(
                runtime,
                jsi::PropNameID::forAscii(runtime, "executor"),
                2,
                [number](jsi::Runtime& rt,
                         const jsi::Value& /*thisVal*/,
                         const jsi::Value* args,
                         size_t count) -> jsi::Value {

                    if (count < 2) return jsi::Value::undefined();

                    auto resolvePtr = std::make_shared<jsi::Function>(args[0].getObject(rt).getFunction(rt));

                    auto callback = jsi::Function::createFromHostFunction(
                        rt,
                        jsi::PropNameID::forAscii(rt, "callback"),
                        0,
                        [resolvePtr, number](jsi::Runtime& runtime,
                                           const jsi::Value& /*thisVal*/,
                                           const jsi::Value* /*args*/,
                                           size_t /*cnt*/) -> jsi::Value {
                            resolvePtr->call(runtime, jsi::Value(number * 2));
                            return jsi::Value::undefined();
                        });

                    rt.global()
                        .getPropertyAsFunction(rt, "setTimeout")
                        .call(rt, callback, jsi::Value(1000));

                    return jsi::Value::undefined();
                });

            return Promise.callAsConstructor(runtime, promiseCtor);
        });

    // =====================================================================
    // FUNCTION 6: connect - ATOL printer connection
    // =====================================================================
    auto connect = jsi::Function::createFromHostFunction(
        rt,
        jsi::PropNameID::forAscii(rt, "connect"),
        3,
        [](jsi::Runtime &runtime,
           const jsi::Value &,
           const jsi::Value *args,
           size_t count) -> jsi::Value {

            if (count < 3 || !args[0].isString() || !args[1].isString() || !args[2].isString()) {
                return jsi::Value::undefined();
            }

            std::string address = args[0].asString(runtime).utf8(runtime);
            std::string port = args[1].asString(runtime).utf8(runtime);
            std::string name = args[2].asString(runtime).utf8(runtime);

            jsi::Function Promise = runtime.global().getPropertyAsFunction(runtime, "Promise");

            auto promiseFn = jsi::Function::createFromHostFunction(
                runtime,
                jsi::PropNameID::forAscii(runtime, "executor"),
                2,
                [address, port, name](jsi::Runtime &rt,
                                      const jsi::Value &,
                                      const jsi::Value *promiseArgs,
                                      size_t promiseCount) -> jsi::Value {
                    if (promiseCount < 2) return jsi::Value::undefined();

                    auto resolve = std::make_shared<jsi::Function>(promiseArgs[0].getObject(rt).getFunction(rt));
                    auto reject = std::make_shared<jsi::Function>(promiseArgs[1].getObject(rt).getFunction(rt));

                    // Get JNI environment
                    JNIEnv *env = nullptr;
                    JavaVM* javaVM = getJavaVM();

                    if (!javaVM) {
                        reject->call(rt, jsi::String::createFromUtf8(rt, "JavaVM not available"));
                        return jsi::Value::undefined();
                    }

                    int result = javaVM->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
                    if (result != JNI_OK) {
                        reject->call(rt, jsi::String::createFromUtf8(rt, "Failed to get JNI environment"));
                        return jsi::Value::undefined();
                    }

                    jobject g_fptr = getGlobalFptr();
                    if (!g_fptr) {
                        reject->call(rt, jsi::String::createFromUtf8(rt, "FPTR object not initialized"));
                        return jsi::Value::undefined();
                    }

                    jclass fptrClass = env->GetObjectClass(g_fptr);

                    // Get method IDs
                    jmethodID setSetting = env->GetMethodID(fptrClass, "setSingleSetting",
                                                            "(Ljava/lang/String;Ljava/lang/String;)V");
                    jmethodID applySettings = env->GetMethodID(fptrClass, "applySingleSettings", "()V");
                    jmethodID setParam = env->GetMethodID(fptrClass, "setParam", "(ILjava/lang/String;)V");
                    jmethodID setBoolParam = env->GetMethodID(fptrClass, "setParam", "(IZ)V");
                    jmethodID open = env->GetMethodID(fptrClass, "open", "()V");
                    jmethodID isOpened = env->GetMethodID(fptrClass, "isOpened", "()Z");
                    jmethodID errorCode = env->GetMethodID(fptrClass, "errorCode", "()I");
                    jmethodID errorDescription = env->GetMethodID(fptrClass, "errorDescription", "()Ljava/lang/String;");

                    // Create Java strings
                    jstring jModel = env->NewStringUTF("0"); // ATOL_AUTO
                    jstring jAddr = env->NewStringUTF(address.c_str());
                    jstring jPort = env->NewStringUTF(port.c_str());
                    jstring jTcp = env->NewStringUTF("2"); // TCP/IP
                    jstring jName = env->NewStringUTF(name.c_str());

                    // Set configuration
                    env->CallVoidMethod(g_fptr, setSetting, env->NewStringUTF("model"), jModel);
                    env->CallVoidMethod(g_fptr, setSetting, env->NewStringUTF("ipaddress"), jAddr);
                    env->CallVoidMethod(g_fptr, setSetting, env->NewStringUTF("ipport"), jPort);
                    env->CallVoidMethod(g_fptr, setSetting, env->NewStringUTF("port"), jTcp);

                    // Apply settings
                    env->CallVoidMethod(g_fptr, applySettings);

                    // Set device name if provided
                    if (!name.empty()) {
                        env->CallVoidMethod(g_fptr, setParam, 1021, jName);
                    }

                    // Set electronic receipt
                    env->CallVoidMethod(g_fptr, setBoolParam, 1203, true); // LIBFPTR_PARAM_RECEIPT_ELECTRONICALLY = 1203

                    // Connect
                    env->CallVoidMethod(g_fptr, open);

                    jboolean opened = env->CallBooleanMethod(g_fptr, isOpened);
                    jint err = env->CallIntMethod(g_fptr, errorCode);

                    if (!opened || err != 0) {
                        jstring jErrStr = (jstring) env->CallObjectMethod(g_fptr, errorDescription);
                        const char *cstr = env->GetStringUTFChars(jErrStr, nullptr);
                        std::string error = std::string(cstr);
                        env->ReleaseStringUTFChars(jErrStr, cstr);
                        reject->call(rt, jsi::String::createFromUtf8(rt, "Fptr error: " + error));
                        return jsi::Value::undefined();
                    }

                    resolve->call(rt, jsi::String::createFromUtf8(rt, "Connected"));
                    return jsi::Value::undefined();
                });

            return Promise.callAsConstructor(runtime, promiseFn);
        });

    // =====================================================================
    // REGISTER FUNCTIONS: add all functions to main object
    // =====================================================================
    atolObject.setProperty(rt, "reverseString", std::move(reverseString));
    atolObject.setProperty(rt, "getNumbers", std::move(getNumbers));
    atolObject.setProperty(rt, "getObject", std::move(getObject));
    atolObject.setProperty(rt, "callMeLater", std::move(callMeLater));
    atolObject.setProperty(rt, "promiseNumber", std::move(promiseNumber));
    atolObject.setProperty(rt, "connect", std::move(connect));

    // =====================================================================
    // SET GLOBAL VARIABLE: make module accessible from JavaScript
    // =====================================================================
    rt.global().setProperty(rt, "__atolModule__", std::move(atolObject));
}

}
