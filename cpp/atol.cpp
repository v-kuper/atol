// ============================================================================
//  ATOL JSI MODULE (Unified Working Version)
//  Все методы + упрощённый configureFptr (как в «рабочем» варианте)
// ============================================================================

#include "atol.h"
#include <jni.h>
#include <string>
#include <mutex>
#include <cctype>
#include <cstdio>
#include <chrono>
#include <sstream>
#include <ctime>

using namespace facebook;

// --------------------------- CONFIG MACROS ----------------------------------

// Включить подробный лог (stderr)
#ifndef ATOL_VERBOSE_LOG
#define ATOL_VERBOSE_LOG 1
#endif

// Использовать установку модели (если мешает — поставьте 0)
#ifndef ATOL_USE_MODEL_SETTING
#define ATOL_USE_MODEL_SETTING 1
#endif

// Значение модели (авто)
#ifndef ATOL_MODEL_VALUE
#define ATOL_MODEL_VALUE "500"
#endif

// Для строкового API: чем задаём TCPIP – "TCPIP" (режим 1) или "2" (режим 2).
// Если раньше «рабочий» метод использовал "2", оставьте 2.
#ifndef ATOL_STRING_TCPIP_MODE
#define ATOL_STRING_TCPIP_MODE 2
#endif

// ------------------ FALLBACK КОНСТАНТ (проверьте со своим SDK) --------------

#ifndef LIBFPTR_SETTING_MODEL
#define LIBFPTR_SETTING_MODEL       0
#define LIBFPTR_SETTING_IPADDRESS   1
#define LIBFPTR_SETTING_IPPORT      2
#define LIBFPTR_SETTING_PORT        3
#define LIBFPTR_MODEL_ATOL_AUTO     500
#define LIBFPTR_PORT_TCPIP          2
#endif

#ifndef LIBFPTR_PARAM_DATA_TYPE
#define LIBFPTR_PARAM_DATA_TYPE     1043
#define LIBFPTR_DT_STATUS           1
#define LIBFPTR_DT_CASH_SUM         14
#define LIBFPTR_DT_SHIFT_STATE      16
#define LIBFPTR_DT_DATE_TIME        3
#define LIBFPTR_PARAM_MODEL_NAME    1
#define LIBFPTR_PARAM_SHIFT_STATE   1054
#define LIBFPTR_PARAM_SHIFT_NUMBER  1056
#define LIBFPTR_PARAM_SUM           1055
#define LIBFPTR_PARAM_DATE_TIME     1020
#define LIBFPTR_PARAM_JSON_DATA     65536
#define LIBFPTR_PARAM_REPORT_TYPE   1022
#define LIBFPTR_RT_CLOSE_SHIFT      1
#define LIBFPTR_RT_X                2
#endif

#ifndef LIBFPTR_PARAM_CASHIER_NAME
#define LIBFPTR_PARAM_CASHIER_NAME  1021
#endif

#ifndef LIBFPTR_PARAM_RECEIPT_ELECTRONICALLY
#define LIBFPTR_PARAM_RECEIPT_ELECTRONICALLY 1203
#endif

// -------------------- Внешние JNI функции (реализуете сами) -----------------
extern JavaVM* getJavaVM();
extern jobject getGlobalFptr();

namespace atoljsi {

static std::mutex gFptrMutex;

// --------------------------- LOGGING ----------------------------------------
static inline void logMsg(const char* tag, const std::string& msg) {
#if ATOL_VERBOSE_LOG
    fprintf(stderr, "[ATOL-JSI][%s] %s\n", tag, msg.c_str());
#else
    (void)tag; (void)msg;
#endif
}

// --------------------------- HELPERS ----------------------------------------
static jmethodID safeGetMethod(JNIEnv* env, jclass cls,
                               const char* name, const char* sig) {
    jmethodID mid = env->GetMethodID(cls, name, sig);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logMsg("JNI", std::string("Method not found: ") + name + " " + sig);
        return nullptr;
    }
    return mid;
}

static std::string trim(std::string s) {
    auto ws = [](unsigned char c){ return std::isspace(c); };
    while (!s.empty() && ws(s.front())) s.erase(s.begin());
    while (!s.empty() && ws(s.back()))  s.pop_back();
    return s;
}

static std::string readError(JNIEnv* env, jobject fptr, jmethodID midErrorDesc) {
    if (!midErrorDesc) return {};
    jstring jDesc = (jstring)env->CallObjectMethod(fptr, midErrorDesc);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
    std::string desc;
    if (jDesc) {
        const char* c = env->GetStringUTFChars(jDesc, nullptr);
        if (c) desc = c;
        env->ReleaseStringUTFChars(jDesc, c);
        env->DeleteLocalRef(jDesc);
    }
    return desc;
}

// ---- UTC epoch ms без timegm (для setDateTime) -----------------------------
static long long toEpochMsUtc(int Y,int M,int D,int h,int m,int s) {
    std::tm tmv{};
    tmv.tm_year = Y - 1900;
    tmv.tm_mon  = M - 1;
    tmv.tm_mday = D;
    tmv.tm_hour = h;
    tmv.tm_min  = m;
    tmv.tm_sec  = s;
    tmv.tm_isdst = -1;
    std::time_t local = std::mktime(&tmv);
#if defined(_WIN32)
    long tz = 0;
    _get_timezone(&tz);
    std::time_t utc = local + tz;
#else
    std::time_t utc = local - timezone;
#endif
    return (long long)utc * 1000LL;
}

// --------------------------- CONFIGURE FPTR ---------------------------------
// Упрощённая версия: сначала int-вариант, иначе строковый.
// В строковом варианте TCPIP -> "2" (по умолчанию), можно переключить.
static bool configureFptr(JNIEnv* env,
                          jobject fptr,
                          const std::string& rawAddress,
                          const std::string& rawPort,
                          const std::string& cashierName,
                          std::string& errorMsg) {
    errorMsg.clear();
    if (!env || !fptr) { errorMsg = "Null env/fptr"; return false; }

    std::string address = trim(rawAddress);
    std::string port    = trim(rawPort);

    jclass cls = env->GetObjectClass(fptr);
    if (!cls) { errorMsg = "GetObjectClass failed"; return false; }

    jmethodID midApplySingle = safeGetMethod(env, cls, "applySingleSettings", "()I");
    jmethodID midErrorCode   = safeGetMethod(env, cls, "errorCode", "()I");
    jmethodID midErrorDesc   = safeGetMethod(env, cls, "errorDescription", "()Ljava/lang/String;");
    jmethodID midSetParamStr = safeGetMethod(env, cls, "setParam", "(ILjava/lang/String;)V");
    jmethodID midSetParamBool= safeGetMethod(env, cls, "setParam", "(IZ)V");

    if (!midApplySingle) {
        errorMsg = "applySingleSettings missing";
        env->DeleteLocalRef(cls);
        return false;
    }

    // Пытаемся int-вариант
    jmethodID midSetSingleInt = env->GetMethodID(cls, "setSingleSetting", "(ILjava/lang/String;)V");
    bool hasIntVariant = true;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        hasIntVariant = false;
        midSetSingleInt = nullptr;
    }

    jmethodID midSetSingleStr = nullptr;
    if (!hasIntVariant) {
        midSetSingleStr = env->GetMethodID(cls, "setSingleSetting",
                                           "(Ljava/lang/String;Ljava/lang/String;)V");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe(); env->ExceptionClear();
            errorMsg = "No setSingleSetting variant";
            env->DeleteLocalRef(cls);
            return false;
        }
    }

    logMsg("CFG", std::string("variant=") + (hasIntVariant? "INT":"STR") +
                 " addr=" + address + ":" + port +
#if ATOL_USE_MODEL_SETTING
                 " model=" + std::string(ATOL_MODEL_VALUE)
#else
                 " model=OFF"
#endif
    );

    auto putInt = [&](int key, const std::string& v)->bool {
        jstring jVal = env->NewStringUTF(v.c_str());
        env->CallVoidMethod(fptr, midSetSingleInt, key, jVal);
        env->DeleteLocalRef(jVal);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe(); env->ExceptionClear();
            errorMsg = "setSingleSetting(int) key=" + std::to_string(key);
            return false;
        }
        return true;
    };
    auto putStr = [&](const char* key, const std::string& v)->bool {
        jstring jKey = env->NewStringUTF(key);
        jstring jVal = env->NewStringUTF(v.c_str());
        env->CallVoidMethod(fptr, midSetSingleStr, jKey, jVal);
        env->DeleteLocalRef(jKey);
        env->DeleteLocalRef(jVal);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe(); env->ExceptionClear();
            errorMsg = std::string("setSingleSetting(str) key=") + key;
            return false;
        }
        return true;
    };

    bool ok = false;

    if (hasIntVariant) {
#if ATOL_USE_MODEL_SETTING
        if (!putInt(LIBFPTR_SETTING_MODEL, ATOL_MODEL_VALUE)) goto attempt_string;
#endif
        if (!putInt(LIBFPTR_SETTING_PORT, std::to_string(LIBFPTR_PORT_TCPIP))) goto attempt_string;
        if (!putInt(LIBFPTR_SETTING_IPADDRESS, address)) goto attempt_string;
        if (!putInt(LIBFPTR_SETTING_IPPORT, port)) goto attempt_string;

        {
            jint rc = env->CallIntMethod(fptr, midApplySingle);
            if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); goto attempt_string; }
            if (rc == 0) {
                ok = true;
            } else {
                int ec = midErrorCode ? env->CallIntMethod(fptr, midErrorCode) : -1;
                std::string desc = readError(env, fptr, midErrorDesc);
                logMsg("CFG", "INT applySingleSettings rc=" + std::to_string(rc) +
                              " ec=" + std::to_string(ec) + (desc.empty()? "":" desc="+desc));
                goto attempt_string;
            }
        }
    }

attempt_string:
    if (!ok && !hasIntVariant) {
        // Строковый режим сразу
        ;
    } else if (!ok && hasIntVariant) {
        // Переходим на строковый fallback
        logMsg("CFG", "FALLBACK → string API");
        // Нужно ещё раз взять string-вариант
        midSetSingleStr = env->GetMethodID(cls, "setSingleSetting",
                                           "(Ljava/lang/String;Ljava/lang/String;)V");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe(); env->ExceptionClear();
            errorMsg = "String setSingleSetting not found";
            env->DeleteLocalRef(cls);
            return false;
        }
    }

    if (!ok) {
        // Строковый API
#if ATOL_USE_MODEL_SETTING
        if (!putStr("Model", ATOL_MODEL_VALUE)) goto fail;
#endif
        if (ATOL_STRING_TCPIP_MODE == 1) {
            if (!putStr("Port", "TCPIP")) goto fail;
        } else {
            if (!putStr("Port", "2")) goto fail; // Часто "2" == TCPIP
        }
        if (!putStr("IPAddress", address)) goto fail;
        if (!putStr("IPPort", port)) goto fail;

        jint rc = env->CallIntMethod(fptr, midApplySingle);
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); errorMsg="Exception applySingleSettings"; goto fail; }
        if (rc != 0) {
            int ec = midErrorCode ? env->CallIntMethod(fptr, midErrorCode) : -1;
            std::string desc = readError(env, fptr, midErrorDesc);
            errorMsg = "applySingleSettings rc=" + std::to_string(rc) +
                       " ec=" + std::to_string(ec) +
                       (desc.empty()? "":" desc="+desc);
            goto fail;
        }
        ok = true;
    }

    // Дополнительные параметры (не критично)
    if (!cashierName.empty() && midSetParamStr) {
        jstring jName = env->NewStringUTF(cashierName.c_str());
        env->CallVoidMethod(fptr, midSetParamStr, LIBFPTR_PARAM_CASHIER_NAME, jName);
        env->DeleteLocalRef(jName);
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    }
    if (midSetParamBool) {
        env->CallVoidMethod(fptr, midSetParamBool, LIBFPTR_PARAM_RECEIPT_ELECTRONICALLY, JNI_TRUE);
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    }

    env->DeleteLocalRef(cls);
    logMsg("CFG", "Settings applied (ok=1)");
    return true;

fail:
    logMsg("CFG-ERR", errorMsg);
    env->DeleteLocalRef(cls);
    return false;
}

// ---------------------- DEVICE INFO -----------------------------------------
static jsi::Object getDeviceInfo(JNIEnv* env, jobject fptr, jsi::Runtime& rt) {
    auto out = jsi::Object(rt);
    out.setProperty(rt, "isConnected", false);
    if (!env || !fptr) return out;

    jclass cls = env->GetObjectClass(fptr);
    if (!cls) return out;

    jmethodID midSetParamInt    = safeGetMethod(env, cls, "setParam", "(II)V");
    jmethodID midQueryData      = safeGetMethod(env, cls, "queryData", "()I");
    jmethodID midGetParamString = safeGetMethod(env, cls, "getParamString", "(I)Ljava/lang/String;");
    jmethodID midGetParamInt    = safeGetMethod(env, cls, "getParamInt", "(I)J");
    jmethodID midGetParamDouble = safeGetMethod(env, cls, "getParamDouble", "(I)D");
    jmethodID midErrorCode      = safeGetMethod(env, cls, "errorCode", "()I");

    if (midSetParamInt && midQueryData) {
        // STATUS
        env->CallVoidMethod(fptr, midSetParamInt, LIBFPTR_PARAM_DATA_TYPE, LIBFPTR_DT_STATUS);
        jint rc = env->CallIntMethod(fptr, midQueryData);
        jint ec = midErrorCode ? env->CallIntMethod(fptr, midErrorCode) : 0;
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (rc == 0 && ec == 0 && midGetParamString) {
            jstring jModel = (jstring)env->CallObjectMethod(fptr, midGetParamString, LIBFPTR_PARAM_MODEL_NAME);
            if (jModel && !env->ExceptionCheck()) {
                const char* c = env->GetStringUTFChars(jModel, nullptr);
                if (c) out.setProperty(rt, "modelName", jsi::String::createFromUtf8(rt, c));
                env->ReleaseStringUTFChars(jModel, c);
                env->DeleteLocalRef(jModel);
            } else if (env->ExceptionCheck()) env->ExceptionClear();
        }
        if (midGetParamInt) {
            jlong shiftState = env->CallLongMethod(fptr, midGetParamInt, LIBFPTR_PARAM_SHIFT_STATE);
            if (!env->ExceptionCheck())
                out.setProperty(rt, "shiftState", (double)shiftState);
            else env->ExceptionClear();
        }
        // CASH SUM
        env->CallVoidMethod(fptr, midSetParamInt, LIBFPTR_PARAM_DATA_TYPE, LIBFPTR_DT_CASH_SUM);
        rc = env->CallIntMethod(fptr, midQueryData);
        ec = midErrorCode ? env->CallIntMethod(fptr, midErrorCode) : 0;
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (rc == 0 && ec == 0 && midGetParamDouble) {
            jdouble cash = env->CallDoubleMethod(fptr, midGetParamDouble, LIBFPTR_PARAM_SUM);
            if (!env->ExceptionCheck())
                out.setProperty(rt, "cashSum", (double)cash);
            else env->ExceptionClear();
        }
    }
    env->DeleteLocalRef(cls);
    out.setProperty(rt, "isConnected", true);
    return out;
}

// ----------------------- PROMISE TEMPLATE -----------------------------------
template<typename Body>
static jsi::Value makePromise(jsi::Runtime& rt, Body&& body) {
    auto Promise = rt.global().getPropertyAsFunction(rt, "Promise");
    auto executor = jsi::Function::createFromHostFunction(
        rt, jsi::PropNameID::forAscii(rt, "executor"), 2,
        [body = std::forward<Body>(body)](jsi::Runtime& r2,
                                          const jsi::Value&,
                                          const jsi::Value* p,
                                          size_t pc)->jsi::Value {
            if (pc < 2) return jsi::Value::undefined();
            auto resolve = std::make_shared<jsi::Function>(p[0].getObject(r2).getFunction(r2));
            auto reject  = std::make_shared<jsi::Function>(p[1].getObject(r2).getFunction(r2));

            std::lock_guard<std::mutex> lg(gFptrMutex);
            JavaVM* jvm = getJavaVM();
            if (!jvm) {
                reject->call(r2, jsi::String::createFromUtf8(r2, "No JavaVM"));
                return jsi::Value::undefined();
            }

            JNIEnv* env = nullptr; bool attached = false;
            if (jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
#if defined(__ANDROID__)
                if (jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
                    reject->call(r2, jsi::String::createFromUtf8(r2, "AttachCurrentThread failed"));
                    return jsi::Value::undefined();
                }
                attached = true;
#else
                reject->call(r2, jsi::String::createFromUtf8(r2, "Cannot get JNIEnv"));
                return jsi::Value::undefined();
#endif
            }

            jobject fptr = getGlobalFptr();
            if (!fptr) {
                if (attached) jvm->DetachCurrentThread();
                reject->call(r2, jsi::String::createFromUtf8(r2, "FPTR not initialized"));
                return jsi::Value::undefined();
            }

            try {
                body(r2, *resolve, *reject, env, fptr);
            } catch (const std::exception& ex) {
                reject->call(r2, jsi::String::createFromUtf8(r2, ex.what()));
            } catch (...) {
                reject->call(r2, jsi::String::createFromUtf8(r2, "Unknown native exception"));
            }

            if (attached) jvm->DetachCurrentThread();
            return jsi::Value::undefined();
        });
    return Promise.callAsConstructor(rt, executor);
}

// ------------------------- LOW-LEVEL HELPERS --------------------------------
static bool tryPrintText(JNIEnv* env, jobject fptr, int& errCode, std::string& desc) {
    jclass cls = env->GetObjectClass(fptr);
    if (!cls) return false;
    jmethodID midPrintText   = safeGetMethod(env, cls, "printText", "()I");
    jmethodID midErrorCode   = safeGetMethod(env, cls, "errorCode", "()I");
    jmethodID midErrorDesc   = safeGetMethod(env, cls, "errorDescription","()Ljava/lang/String;");
    if (!midPrintText || !midErrorCode || !midErrorDesc) { env->DeleteLocalRef(cls); return false; }
    jint rc = env->CallIntMethod(fptr, midPrintText);
    bool hadEx = env->ExceptionCheck();
    if (hadEx) { env->ExceptionDescribe(); env->ExceptionClear(); }
    errCode = env->CallIntMethod(fptr, midErrorCode);
    desc = readError(env, fptr, midErrorDesc);
    env->DeleteLocalRef(cls);
    return (!hadEx && rc >= 0 && errCode == 0);
}

static bool queryCashSum(JNIEnv* env, jobject fptr, double& cash, int& err, std::string& desc) {
    jclass cls = env->GetObjectClass(fptr);
    if (!cls) return false;
    jmethodID midSetParamInt    = safeGetMethod(env, cls, "setParam", "(II)V");
    jmethodID midQueryData      = safeGetMethod(env, cls, "queryData", "()I");
    jmethodID midGetParamDouble = safeGetMethod(env, cls, "getParamDouble", "(I)D");
    jmethodID midErrorCode      = safeGetMethod(env, cls, "errorCode", "()I");
    jmethodID midErrorDesc      = safeGetMethod(env, cls, "errorDescription","()Ljava/lang/String;");
    if (!midSetParamInt || !midQueryData || !midGetParamDouble ||
        !midErrorCode || !midErrorDesc) {
        env->DeleteLocalRef(cls); return false;
    }
    env->CallVoidMethod(fptr, midSetParamInt, LIBFPTR_PARAM_DATA_TYPE, LIBFPTR_DT_CASH_SUM);
    jint rc = env->CallIntMethod(fptr, midQueryData);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    err = env->CallIntMethod(fptr, midErrorCode);
    desc = readError(env, fptr, midErrorDesc);
    if (rc == 0 && err == 0) {
        cash = env->CallDoubleMethod(fptr, midGetParamDouble, LIBFPTR_PARAM_SUM);
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); env->DeleteLocalRef(cls); return false; }
        env->DeleteLocalRef(cls);
        return true;
    }
    env->DeleteLocalRef(cls);
    return false;
}

// -------------------------- fnConnect (Promise) -----------------------------
static jsi::Value fnConnect(jsi::Runtime& rt,
                            const std::string& addr,
                            const std::string& port,
                            const std::string& name) {
    return makePromise(rt, [addr,port,name](jsi::Runtime& r2,
                                            jsi::Function& resolve,
                                            jsi::Function& reject,
                                            JNIEnv* env,
                                            jobject fptr) {

        jclass cls = env->GetObjectClass(fptr);
        if (!cls) {
            reject.call(r2, jsi::String::createFromUtf8(r2, "GetObjectClass failed"));
            return;
        }

        jmethodID midOpen      = safeGetMethod(env, cls, "open", "()I");
        jmethodID midIsOpened  = safeGetMethod(env, cls, "isOpened", "()Z");
        jmethodID midErrorCode = safeGetMethod(env, cls, "errorCode", "()I");
        jmethodID midErrorDesc = safeGetMethod(env, cls, "errorDescription","()Ljava/lang/String;");

        if (!midOpen || !midIsOpened || !midErrorCode || !midErrorDesc) {
            env->DeleteLocalRef(cls);
            reject.call(r2, jsi::String::createFromUtf8(r2, "Missing methods"));
            return;
        }

        std::string cfgErr;
        if (!configureFptr(env, fptr, addr, port, name, cfgErr)) {
            env->DeleteLocalRef(cls);
            reject.call(r2, jsi::String::createFromUtf8(r2, "Config failed: " + cfgErr));
            return;
        }

        jint openRc = env->CallIntMethod(fptr, midOpen);
        bool hadEx = env->ExceptionCheck();
        if (hadEx) { env->ExceptionDescribe(); env->ExceptionClear(); }
        jboolean opened = env->CallBooleanMethod(fptr, midIsOpened);
        jint errCode = env->CallIntMethod(fptr, midErrorCode);
        std::string desc = readError(env, fptr, midErrorDesc);

        logMsg("OPEN", "openRc=" + std::to_string(openRc) +
                       " opened=" + std::to_string((int)opened) +
                       " err=" + std::to_string(errCode) +
                       (desc.empty()? "" : " desc=" + desc));

        if (hadEx || !opened || openRc != 0 || errCode != 0) {
            env->DeleteLocalRef(cls);
            std::string msg = "Connection failed";
            if (!desc.empty()) msg += ": " + desc;
            msg += " (openRc=" + std::to_string(openRc) + ", errCode=" + std::to_string(errCode) + ")";
            reject.call(r2, jsi::String::createFromUtf8(r2, msg));
            return;
        }

        auto info = getDeviceInfo(env, fptr, r2);
        env->DeleteLocalRef(cls);
        resolve.call(r2, info);
    });
}

// --------------------------- INSTALL (все методы) ---------------------------
void install(jsi::Runtime &rt) {
    auto mod = jsi::Object(rt);

    // --- Utilities ----------------------------------------------------------
    mod.setProperty(rt, "reverseString",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"reverseString"),1,
            [](jsi::Runtime& r, const jsi::Value&, const jsi::Value* a,size_t c)->jsi::Value {
                if (c && a[0].isString()) {
                    auto s = a[0].asString(r).utf8(r);
                    std::string rev(s.rbegin(), s.rend());
                    return jsi::String::createFromUtf8(r, rev);
                }
                return jsi::Value::undefined();
            }));
    mod.setProperty(rt, "getNumbers",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"getNumbers"),0,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value*,size_t)->jsi::Value {
                jsi::Array arr(r,3);
                arr.setValueAtIndex(r,0,1);
                arr.setValueAtIndex(r,1,2);
                arr.setValueAtIndex(r,2,3);
                return arr;
            }));
    mod.setProperty(rt, "getObject",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"getObject"),0,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value*,size_t)->jsi::Value {
                auto o = jsi::Object(r);
                o.setProperty(r,"name", jsi::String::createFromUtf8(r,"Atol"));
                o.setProperty(r,"version", jsi::String::createFromUtf8(r,"1.0.0"));
                return o;
            }));

    // --- close() ------------------------------------------------------------
    mod.setProperty(rt, "close",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"close"),0,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value*,size_t)->jsi::Value {
                std::lock_guard<std::mutex> lg(gFptrMutex);
                JavaVM* jvm = getJavaVM(); if (!jvm) return jsi::Value::undefined();
                JNIEnv* env=nullptr; bool attached=false;
                if (jvm->GetEnv((void**)&env, JNI_VERSION_1_6)!=JNI_OK) {
#if defined(__ANDROID__)
                    if (jvm->AttachCurrentThread(&env,nullptr)!=JNI_OK) return jsi::Value::undefined();
                    attached=true;
#else
                    return jsi::Value::undefined();
#endif
                }
                jobject fptr = getGlobalFptr();
                if (env && fptr) {
                    jclass cls = env->GetObjectClass(fptr);
                    if (cls) {
                        jmethodID midClose = safeGetMethod(env, cls, "close", "()I");
                        if (midClose) {
                            env->CallIntMethod(fptr, midClose);
                            if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
                        }
                        env->DeleteLocalRef(cls);
                    }
                }
                if (attached) jvm->DetachCurrentThread();
                return jsi::Value::undefined();
            }));

    // --- connect(address, port, name) ---------------------------------------
    mod.setProperty(rt, "connect",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"connect"),3,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value* a,size_t c)->jsi::Value {
                if (c<3 || !a[0].isString() || !a[1].isString() || !a[2].isString())
                    return jsi::Value::undefined();
                return fnConnect(r,
                                 a[0].asString(r).utf8(r),
                                 a[1].asString(r).utf8(r),
                                 a[2].asString(r).utf8(r));
            }));

    // --- reconnect(address, port, name) -------------------------------------
    mod.setProperty(rt, "reconnect",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"reconnect"),3,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value* a,size_t c)->jsi::Value {
                if (c<3 || !a[0].isString() || !a[1].isString() || !a[2].isString())
                    return jsi::Value::undefined();
                {
                    std::lock_guard<std::mutex> lg(gFptrMutex);
                    JavaVM* jvm = getJavaVM(); if (!jvm) return jsi::Value::undefined();
                    JNIEnv* env=nullptr;
                    if (jvm->GetEnv((void**)&env, JNI_VERSION_1_6)==JNI_OK) {
                        jobject fptr = getGlobalFptr();
                        if (fptr) {
                            jclass cls = env->GetObjectClass(fptr);
                            if (cls) {
                                jmethodID midClose = safeGetMethod(env, cls, "close", "()I");
                                if (midClose) {
                                    env->CallIntMethod(fptr, midClose);
                                    if (env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }
                                }
                                env->DeleteLocalRef(cls);
                            }
                        }
                    }
                }
                return fnConnect(r,
                                 a[0].asString(r).utf8(r),
                                 a[1].asString(r).utf8(r),
                                 a[2].asString(r).utf8(r));
            }));

    // --- checkConnectionStatus() --------------------------------------------
    mod.setProperty(rt, "checkConnectionStatus",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"checkConnectionStatus"),0,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value*,size_t)->jsi::Value {
                return makePromise(r, [](jsi::Runtime& r2,
                                         jsi::Function& resolve,
                                         jsi::Function& reject,
                                         JNIEnv* env,
                                         jobject fptr) {
                    jclass cls = env->GetObjectClass(fptr);
                    if (!cls) { reject.call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed")); return; }
                    jmethodID midIsOpened = safeGetMethod(env, cls, "isOpened", "()Z");
                    if (!midIsOpened) { env->DeleteLocalRef(cls); reject.call(r2, jsi::String::createFromUtf8(r2,"No isOpened")); return; }
                    jboolean opened = env->CallBooleanMethod(fptr, midIsOpened);
                    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
                    env->DeleteLocalRef(cls);
                    auto o = jsi::Object(r2);
                    o.setProperty(r2,"isConnected",(bool)opened);
                    resolve.call(r2,o);
                });
            }));

    // --- heartbeat() --------------------------------------------------------
    mod.setProperty(rt, "heartbeat",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"heartbeat"),0,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value*,size_t)->jsi::Value {
                return makePromise(r, [](jsi::Runtime& r2,
                                         jsi::Function& resolve,
                                         jsi::Function& reject,
                                         JNIEnv* env,
                                         jobject fptr) {
                    int ec=0; std::string desc;
                    if (!tryPrintText(env, fptr, ec, desc)) {
                        reject.call(r2, jsi::String::createFromUtf8(r2,
                            "Heartbeat failed: " + desc + " (errCode=" + std::to_string(ec) + ")"));
                        return;
                    }
                    auto o = jsi::Object(r2);
                    o.setProperty(r2, "ok", true);
                    resolve.call(r2, o);
                });
            }));

    // --- getShiftStatus() ---------------------------------------------------
    mod.setProperty(rt, "getShiftStatus",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"getShiftStatus"),0,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value*,size_t)->jsi::Value {
                return makePromise(r, [](jsi::Runtime& r2,
                                         jsi::Function& resolve,
                                         jsi::Function& reject,
                                         JNIEnv* env,
                                         jobject fptr){
                    jclass cls = env->GetObjectClass(fptr);
                    if(!cls){ reject.call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed")); return; }
                    jmethodID midSetParamInt  = safeGetMethod(env, cls, "setParam", "(II)V");
                    jmethodID midQueryData    = safeGetMethod(env, cls, "queryData", "()I");
                    jmethodID midGetParamInt  = safeGetMethod(env, cls, "getParamInt", "(I)J");
                    jmethodID midGetParamDouble = safeGetMethod(env, cls, "getParamDouble", "(I)D");
                    jmethodID midGetParamDate = safeGetMethod(env, cls, "getParamDateTime", "(I)Ljava/util/Date;");
                    jmethodID midErrorCode    = safeGetMethod(env, cls, "errorCode", "()I");
                    jmethodID midErrorDesc    = safeGetMethod(env, cls, "errorDescription","()Ljava/lang/String;");
                    if(!midSetParamInt||!midQueryData||!midGetParamInt||!midGetParamDouble||
                       !midGetParamDate||!midErrorCode||!midErrorDesc){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,"Missing shift methods"));
                        return;
                    }

                    auto ask = [&](int dt)->bool {
                        env->CallVoidMethod(fptr, midSetParamInt, LIBFPTR_PARAM_DATA_TYPE, dt);
                        jint rc = env->CallIntMethod(fptr, midQueryData);
                        if (env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }
                        jint ec = env->CallIntMethod(fptr, midErrorCode);
                        return (ec==0 && rc==0);
                    };

                    if(!ask(LIBFPTR_DT_SHIFT_STATE)) {
                        std::string desc = readError(env,fptr,midErrorDesc);
                        int ec = env->CallIntMethod(fptr, midErrorCode);
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,
                            "Shift state query failed: " + desc + " ("+std::to_string(ec)+")"));
                        return;
                    }
                    jlong shiftState = env->CallLongMethod(fptr, midGetParamInt, LIBFPTR_PARAM_SHIFT_STATE);
                    jlong shiftNumber= env->CallLongMethod(fptr, midGetParamInt, LIBFPTR_PARAM_SHIFT_NUMBER);

                    double cashSum = 0.0; int dumErr=0; std::string ddesc;
                    queryCashSum(env, fptr, cashSum, dumErr, ddesc);

                    env->CallVoidMethod(fptr, midSetParamInt, LIBFPTR_PARAM_DATA_TYPE, LIBFPTR_DT_DATE_TIME);
                    env->CallIntMethod(fptr, midQueryData);
                    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
                    jobject jDate = env->CallObjectMethod(fptr, midGetParamDate, LIBFPTR_PARAM_DATE_TIME);
                    long long epochMs = 0;
                    if (jDate && !env->ExceptionCheck()) {
                        jclass dateCls = env->GetObjectClass(jDate);
                        jmethodID midGetTime = env->GetMethodID(dateCls,"getTime","()J");
                        if (midGetTime) {
                            jlong t = env->CallLongMethod(jDate, midGetTime);
                            if (!env->ExceptionCheck()) epochMs = (long long)t;
                            else env->ExceptionClear();
                        }
                        env->DeleteLocalRef(dateCls);
                        env->DeleteLocalRef(jDate);
                    } else if (env->ExceptionCheck()) env->ExceptionClear();

                    env->DeleteLocalRef(cls);
                    auto o = jsi::Object(r2);
                    o.setProperty(r2,"shiftState",(double)shiftState);
                    o.setProperty(r2,"shiftNumber",(double)shiftNumber);
                    o.setProperty(r2,"cashSum", (double)cashSum);
                    o.setProperty(r2,"dateTime",(double)epochMs);
                    resolve.call(r2,o);
                });
            }));

    // --- openShift(cashierName) ---------------------------------------------
    mod.setProperty(rt, "openShift",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"openShift"),1,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value* a,size_t c)->jsi::Value {
                if(c<1 || !a[0].isString()) return jsi::Value::undefined();
                std::string cashier = a[0].asString(r).utf8(r);
                return makePromise(r, [cashier](jsi::Runtime& r2,
                                                jsi::Function& resolve,
                                                jsi::Function& reject,
                                                JNIEnv* env,
                                                jobject fptr){
                    jclass cls = env->GetObjectClass(fptr);
                    if(!cls){ reject.call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed")); return; }
                    jmethodID midSetParamStr  = safeGetMethod(env, cls, "setParam", "(ILjava/lang/String;)V");
                    jmethodID midOperatorLogin= safeGetMethod(env, cls, "operatorLogin", "()I");
                    jmethodID midOpenShift    = safeGetMethod(env, cls, "openShift", "()I");
                    jmethodID midPrintText    = safeGetMethod(env, cls, "printText", "()I");
                    jmethodID midSetParamInt  = safeGetMethod(env, cls, "setParam", "(II)V");
                    jmethodID midQueryData    = safeGetMethod(env, cls, "queryData", "()I");
                    jmethodID midGetParamInt  = safeGetMethod(env, cls, "getParamInt", "(I)J");
                    jmethodID midErrorCode    = safeGetMethod(env, cls, "errorCode", "()I");
                    jmethodID midErrorDesc    = safeGetMethod(env, cls, "errorDescription","()Ljava/lang/String;");
                    if(!midSetParamStr||!midOperatorLogin||!midOpenShift||!midPrintText||
                       !midSetParamInt||!midQueryData||!midGetParamInt||!midErrorCode||!midErrorDesc){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,"Missing openShift methods"));
                        return;
                    }

                    jstring jName = env->NewStringUTF(cashier.c_str());
                    env->CallVoidMethod(fptr, midSetParamStr, LIBFPTR_PARAM_CASHIER_NAME, jName);
                    env->DeleteLocalRef(jName);
                    if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }

                    env->CallIntMethod(fptr, midOperatorLogin);
                    jint rc = env->CallIntMethod(fptr, midOpenShift);
                    bool hadEx = env->ExceptionCheck();
                    if(hadEx){ env->ExceptionDescribe(); env->ExceptionClear(); }
                    int err = env->CallIntMethod(fptr, midErrorCode);
                    std::string desc = readError(env,fptr,midErrorDesc);
                    if (hadEx || rc!=0 || err!=0) {
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,
                           "openShift failed: "+desc+" (rc="+std::to_string(rc)+", err="+std::to_string(err)+")"));
                        return;
                    }
                    env->CallIntMethod(fptr, midPrintText);
                    if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }

                    env->CallVoidMethod(fptr, midSetParamInt, LIBFPTR_PARAM_DATA_TYPE, LIBFPTR_DT_SHIFT_STATE);
                    env->CallIntMethod(fptr, midQueryData);
                    jlong shiftState = env->CallLongMethod(fptr, midGetParamInt, LIBFPTR_PARAM_SHIFT_STATE);
                    jlong shiftNumber= env->CallLongMethod(fptr, midGetParamInt, LIBFPTR_PARAM_SHIFT_NUMBER);
                    if(env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }

                    auto o = jsi::Object(r2);
                    o.setProperty(r2,"shiftState",(double)shiftState);
                    o.setProperty(r2,"shiftNumber",(double)shiftNumber);
                    o.setProperty(r2,"message", jsi::String::createFromUtf8(r2,
                        shiftState==1? "Shift opened." : "Failed to open the shift."));
                    env->DeleteLocalRef(cls);
                    resolve.call(r2,o);
                });
            }));

    // --- closeShift(cashierName) --------------------------------------------
    mod.setProperty(rt, "closeShift",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"closeShift"),1,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value* a,size_t c)->jsi::Value {
                if(c<1||!a[0].isString()) return jsi::Value::undefined();
                std::string cashier = a[0].asString(r).utf8(r);
                return makePromise(r, [cashier](jsi::Runtime& r2,
                                                jsi::Function& resolve,
                                                jsi::Function& reject,
                                                JNIEnv* env,
                                                jobject fptr){
                    jclass cls = env->GetObjectClass(fptr);
                    if(!cls){ reject.call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed")); return; }

                    jmethodID midSetParamStr  = safeGetMethod(env, cls, "setParam", "(ILjava/lang/String;)V");
                    jmethodID midOperatorLogin= safeGetMethod(env, cls, "operatorLogin", "()I");
                    jmethodID midSetParamInt  = safeGetMethod(env, cls, "setParam", "(II)V");
                    jmethodID midReport       = safeGetMethod(env, cls, "report", "()I");
                    jmethodID midPrintText    = safeGetMethod(env, cls, "printText", "()I");
                    jmethodID midContinuePrint= safeGetMethod(env, cls, "continuePrint", "()I");
                    jmethodID midGetParamBool = safeGetMethod(env, cls, "getParamBool", "(I)Z");
                    jmethodID midQueryData    = safeGetMethod(env, cls, "queryData", "()I");
                    jmethodID midGetParamInt  = safeGetMethod(env, cls, "getParamInt", "(I)J");
                    jmethodID midErrorCode    = safeGetMethod(env, cls, "errorCode", "()I");
                    jmethodID midErrorDesc    = safeGetMethod(env, cls, "errorDescription","()Ljava/lang/String;");
                    if(!midSetParamStr||!midOperatorLogin||!midSetParamInt||!midReport||
                       !midPrintText||!midGetParamBool||!midQueryData||!midGetParamInt||
                       !midErrorCode||!midErrorDesc){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,"Missing closeShift methods"));
                        return;
                    }

                    jstring jName = env->NewStringUTF(cashier.c_str());
                    env->CallVoidMethod(fptr, midSetParamStr, LIBFPTR_PARAM_CASHIER_NAME, jName);
                    env->DeleteLocalRef(jName);
                    if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }
                    env->CallIntMethod(fptr, midOperatorLogin);

                    env->CallVoidMethod(fptr, midSetParamInt, LIBFPTR_PARAM_REPORT_TYPE, LIBFPTR_RT_CLOSE_SHIFT);
                    jint rc = env->CallIntMethod(fptr, midReport);
                    bool hadEx = env->ExceptionCheck();
                    if(hadEx){ env->ExceptionDescribe(); env->ExceptionClear(); }
                    int err = env->CallIntMethod(fptr, midErrorCode);
                    std::string desc = readError(env,fptr,midErrorDesc);
                    if(hadEx||rc!=0||err!=0){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,
                            "closeShift report failed: "+desc+" (rc="+std::to_string(rc)+", err="+std::to_string(err)+")"));
                        return;
                    }

                    env->CallIntMethod(fptr, midPrintText);
                    if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }

                    jboolean printed = env->CallBooleanMethod(fptr, midGetParamBool,  1060 /*LIBFPTR_PARAM_DOCUMENT_PRINTED (fallback)*/ );
                    if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }
                    if(!printed && midContinuePrint) {
                        for(int i=0;i<20;i++){
                            jint rcc = env->CallIntMethod(fptr, midContinuePrint);
                            if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); break; }
                            if(rcc>=0) break;
                        }
                    }

                    env->CallVoidMethod(fptr, midSetParamInt, LIBFPTR_PARAM_DATA_TYPE, LIBFPTR_DT_SHIFT_STATE);
                    env->CallIntMethod(fptr, midQueryData);
                    jlong shiftState = env->CallLongMethod(fptr, midGetParamInt, LIBFPTR_PARAM_SHIFT_STATE);
                    if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }

                    auto o = jsi::Object(r2);
                    o.setProperty(r2,"shiftState",(double)shiftState);
                    o.setProperty(r2,"message", jsi::String::createFromUtf8(r2,
                        shiftState==0?"Shift closed.":"Failed to close shift."));
                    env->DeleteLocalRef(cls);
                    resolve.call(r2,o);
                });
            }));

    // --- cashIncome(amount, cashier) ----------------------------------------
    mod.setProperty(rt, "cashIncome",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"cashIncome"),2,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value* a,size_t c)->jsi::Value {
                if(c<2 || !a[0].isNumber() || !a[1].isString()) return jsi::Value::undefined();
                double amount = a[0].asNumber();
                std::string cashier = a[1].asString(r).utf8(r);
                return makePromise(r, [amount,cashier](jsi::Runtime& r2,
                                                       jsi::Function& resolve,
                                                       jsi::Function& reject,
                                                       JNIEnv* env,
                                                       jobject fptr){
                    jclass cls = env->GetObjectClass(fptr);
                    if(!cls){ reject.call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed")); return; }
                    jmethodID midSetParamStr = safeGetMethod(env, cls, "setParam", "(ILjava/lang/String;)V");
                    jmethodID midOperatorLogin= safeGetMethod(env, cls,"operatorLogin","()I");
                    jmethodID midSetParamDouble = safeGetMethod(env, cls, "setParam", "(ID)V");
                    jmethodID midCashIncome = safeGetMethod(env, cls, "cashIncome", "()I");
                    jmethodID midPrintText  = safeGetMethod(env, cls, "printText", "()I");
                    jmethodID midErrorCode  = safeGetMethod(env, cls, "errorCode", "()I");
                    jmethodID midErrorDesc  = safeGetMethod(env, cls, "errorDescription", "()Ljava/lang/String;");
                    if(!midSetParamStr||!midOperatorLogin||!midSetParamDouble||!midCashIncome||
                       !midPrintText||!midErrorCode||!midErrorDesc){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,"Missing cashIncome methods"));
                        return;
                    }
                    jstring jName = env->NewStringUTF(cashier.c_str());
                    env->CallVoidMethod(fptr, midSetParamStr, LIBFPTR_PARAM_CASHIER_NAME, jName);
                    env->DeleteLocalRef(jName);
                    env->CallIntMethod(fptr, midOperatorLogin);

                    env->CallVoidMethod(fptr, midSetParamDouble, LIBFPTR_PARAM_SUM, (jdouble)amount);
                    jint rc = env->CallIntMethod(fptr, midCashIncome);
                    bool hadEx = env->ExceptionCheck();
                    if(hadEx){ env->ExceptionDescribe(); env->ExceptionClear(); }
                    int err = env->CallIntMethod(fptr, midErrorCode);
                    std::string desc = readError(env,fptr,midErrorDesc);
                    if(hadEx||rc!=0||err!=0){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,
                          "cashIncome failed: "+desc+" (rc="+std::to_string(rc)+", err="+std::to_string(err)+")"));
                        return;
                    }
                    env->CallIntMethod(fptr, midPrintText);
                    if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }

                    double cashSum=0; int ec=0; std::string d;
                    queryCashSum(env,fptr,cashSum,ec,d);
                    auto o = jsi::Object(r2);
                    o.setProperty(r2,"cashSum",(double)cashSum);
                    o.setProperty(r2,"message", jsi::String::createFromUtf8(r2,"Cash income recorded."));
                    env->DeleteLocalRef(cls);
                    resolve.call(r2,o);
                });
            }));

    // --- cashOutcome(amount, cashier) ---------------------------------------
    mod.setProperty(rt, "cashOutcome",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"cashOutcome"),2,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value* a,size_t c)->jsi::Value {
                if(c<2 || !a[0].isNumber() || !a[1].isString()) return jsi::Value::undefined();
                double amount = a[0].asNumber();
                std::string cashier = a[1].asString(r).utf8(r);
                return makePromise(r, [amount,cashier](jsi::Runtime& r2,
                                                       jsi::Function& resolve,
                                                       jsi::Function& reject,
                                                       JNIEnv* env,
                                                       jobject fptr){
                    jclass cls = env->GetObjectClass(fptr);
                    if(!cls){ reject.call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed")); return; }
                    jmethodID midSetParamStr = safeGetMethod(env, cls, "setParam", "(ILjava/lang/String;)V");
                    jmethodID midOperatorLogin= safeGetMethod(env, cls,"operatorLogin","()I");
                    jmethodID midSetParamDouble = safeGetMethod(env, cls, "setParam", "(ID)V");
                    jmethodID midCashOutcome = safeGetMethod(env, cls, "cashOutcome", "()I");
                    jmethodID midPrintText  = safeGetMethod(env, cls, "printText", "()I");
                    jmethodID midErrorCode  = safeGetMethod(env, cls, "errorCode", "()I");
                    jmethodID midErrorDesc  = safeGetMethod(env, cls, "errorDescription", "()Ljava/lang/String;");
                    if(!midSetParamStr||!midOperatorLogin||!midSetParamDouble||!midCashOutcome||
                       !midPrintText||!midErrorCode||!midErrorDesc){
                        env->DeleteLocalRef(cls); reject.call(r2, jsi::String::createFromUtf8(r2,"Missing cashOutcome methods")); return;
                    }
                    jstring jName = env->NewStringUTF(cashier.c_str());
                    env->CallVoidMethod(fptr, midSetParamStr, LIBFPTR_PARAM_CASHIER_NAME, jName);
                    env->DeleteLocalRef(jName);
                    env->CallIntMethod(fptr, midOperatorLogin);

                    env->CallVoidMethod(fptr, midSetParamDouble, LIBFPTR_PARAM_SUM, (jdouble)amount);
                    jint rc = env->CallIntMethod(fptr, midCashOutcome);
                    bool hadEx = env->ExceptionCheck();
                    if(hadEx){ env->ExceptionDescribe(); env->ExceptionClear(); }
                    int err = env->CallIntMethod(fptr, midErrorCode);
                    std::string desc = readError(env,fptr,midErrorDesc);
                    if(hadEx||rc!=0||err!=0){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,
                          "cashOutcome failed: "+desc+" (rc="+std::to_string(rc)+", err="+std::to_string(err)+")"));
                        return;
                    }
                    env->CallIntMethod(fptr, midPrintText);
                    if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }

                    double cashSum=0; int ec=0; std::string d;
                    queryCashSum(env,fptr,cashSum,ec,d);
                    auto o = jsi::Object(r2);
                    o.setProperty(r2,"cashSum",(double)cashSum);
                    o.setProperty(r2,"message", jsi::String::createFromUtf8(r2,"Cash outcome recorded."));
                    env->DeleteLocalRef(cls);
                    resolve.call(r2,o);
                });
            }));

    // --- processJson(jsonTask) ----------------------------------------------
    mod.setProperty(rt, "processJson",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"processJson"),1,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value* a,size_t c)->jsi::Value {
                if(c<1 || !a[0].isString()) return jsi::Value::undefined();
                std::string task = a[0].asString(r).utf8(r);
                return makePromise(r, [task](jsi::Runtime& r2,
                                             jsi::Function& resolve,
                                             jsi::Function& reject,
                                             JNIEnv* env,
                                             jobject fptr){
                    jclass cls = env->GetObjectClass(fptr);
                    if(!cls){ reject.call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed")); return; }

                    jmethodID midSetParamStr  = safeGetMethod(env, cls, "setParam", "(ILjava/lang/String;)V");
                    jmethodID midValidateJson = safeGetMethod(env, cls, "validateJson", "()I");
                    jmethodID midProcessJson  = safeGetMethod(env, cls, "processJson", "()I");
                    jmethodID midGetParamStr  = safeGetMethod(env, cls, "getParamString", "(I)Ljava/lang/String;");
                    jmethodID midErrorCode    = safeGetMethod(env, cls, "errorCode", "()I");
                    jmethodID midErrorDesc    = safeGetMethod(env, cls, "errorDescription","()Ljava/lang/String;");
                    if(!midSetParamStr||!midValidateJson||!midProcessJson||!midGetParamStr||
                       !midErrorCode||!midErrorDesc){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,"Missing JSON methods"));
                        return;
                    }
                    jstring jTask = env->NewStringUTF(task.c_str());
                    env->CallVoidMethod(fptr, midSetParamStr, LIBFPTR_PARAM_JSON_DATA, jTask);
                    env->DeleteLocalRef(jTask);
                    env->CallIntMethod(fptr, midValidateJson);
                    if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }

                    // Повторно для process
                    jstring jTask2 = env->NewStringUTF(task.c_str());
                    env->CallVoidMethod(fptr, midSetParamStr, LIBFPTR_PARAM_JSON_DATA, jTask2);
                    env->DeleteLocalRef(jTask2);

                    jint rc = env->CallIntMethod(fptr, midProcessJson);
                    bool hadEx = env->ExceptionCheck();
                    if(hadEx){ env->ExceptionDescribe(); env->ExceptionClear(); }
                    int err = env->CallIntMethod(fptr, midErrorCode);
                    std::string desc = readError(env,fptr,midErrorDesc);
                    if(hadEx||rc!=0||err!=0){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,
                           "processJson failed: "+desc+" (rc="+std::to_string(rc)+", err="+std::to_string(err)+")"));
                        return;
                    }

                    jstring jRes = (jstring)env->CallObjectMethod(fptr, midGetParamStr, LIBFPTR_PARAM_JSON_DATA);
                    std::string raw;
                    if(jRes){
                        const char* c = env->GetStringUTFChars(jRes,nullptr);
                        if(c) raw = c;
                        env->ReleaseStringUTFChars(jRes,c);
                        env->DeleteLocalRef(jRes);
                    }
                    env->DeleteLocalRef(cls);

                    auto o = jsi::Object(r2);
                    o.setProperty(r2,"rawResult", jsi::String::createFromUtf8(r2, raw));
                    o.setProperty(r2,"jsonParsable", !raw.empty() && (raw.front()=='{' || raw.front()=='['));
                    resolve.call(r2,o);
                });
            }));

    // --- sellProduct(jsonTask) ----------------------------------------------
    mod.setProperty(rt, "sellProduct",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"sellProduct"),1,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value* a,size_t c)->jsi::Value {
                if(c<1 || !a[0].isString()) return jsi::Value::undefined();
                std::string task = a[0].asString(r).utf8(r);
                return makePromise(r, [task](jsi::Runtime& r2,
                                             jsi::Function& resolve,
                                             jsi::Function& reject,
                                             JNIEnv* env,
                                             jobject fptr){
                    jclass cls = env->GetObjectClass(fptr);
                    if(!cls){ reject.call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed")); return; }

                    jmethodID midSetParamStr  = safeGetMethod(env, cls, "setParam", "(ILjava/lang/String;)V");
                    jmethodID midValidateJson = safeGetMethod(env, cls, "validateJson", "()I");
                    jmethodID midProcessJson  = safeGetMethod(env, cls, "processJson", "()I");
                    jmethodID midGetParamStr  = safeGetMethod(env, cls, "getParamString", "(I)Ljava/lang/String;");
                    jmethodID midCancelReceipt= safeGetMethod(env, cls, "cancelReceipt", "()I");
                    jmethodID midPrintText    = safeGetMethod(env, cls, "printText", "()I");
                    jmethodID midErrorCode    = safeGetMethod(env, cls, "errorCode", "()I");
                    jmethodID midErrorDesc    = safeGetMethod(env, cls, "errorDescription","()Ljava/lang/String;");
                    if(!midSetParamStr||!midValidateJson||!midProcessJson||!midGetParamStr||
                       !midPrintText||!midErrorCode||!midErrorDesc) {
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,"Missing sellProduct methods"));
                        return;
                    }

                    auto rejectWithCancel = [&](const std::string& msg){
                        if(midCancelReceipt){
                            env->CallIntMethod(fptr, midCancelReceipt);
                            if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }
                        }
                        reject.call(r2, jsi::String::createFromUtf8(r2,msg));
                    };

                    jstring jTask = env->NewStringUTF(task.c_str());
                    env->CallVoidMethod(fptr, midSetParamStr, LIBFPTR_PARAM_JSON_DATA, jTask);
                    env->DeleteLocalRef(jTask);
                    env->CallIntMethod(fptr, midValidateJson);
                    if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }

                    jstring jTask2 = env->NewStringUTF(task.c_str());
                    env->CallVoidMethod(fptr, midSetParamStr, LIBFPTR_PARAM_JSON_DATA, jTask2);
                    env->DeleteLocalRef(jTask2);

                    jint rc = env->CallIntMethod(fptr, midProcessJson);
                    bool hadEx = env->ExceptionCheck();
                    if(hadEx){ env->ExceptionDescribe(); env->ExceptionClear(); }
                    int err = env->CallIntMethod(fptr, midErrorCode);
                    std::string desc = readError(env,fptr,midErrorDesc);
                    if(hadEx||rc!=0||err!=0){
                        env->DeleteLocalRef(cls);
                        rejectWithCancel("sellProduct processJson failed: "+desc+
                                         " (rc="+std::to_string(rc)+", err="+std::to_string(err)+")");
                        return;
                    }

                    env->CallIntMethod(fptr, midPrintText);
                    if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }

                    jstring jRes = (jstring)env->CallObjectMethod(fptr, midGetParamStr, LIBFPTR_PARAM_JSON_DATA);
                    std::string raw;
                    if(jRes){
                        const char* c = env->GetStringUTFChars(jRes,nullptr);
                        if(c) raw=c;
                        env->ReleaseStringUTFChars(jRes,c);
                        env->DeleteLocalRef(jRes);
                    }

                    env->DeleteLocalRef(cls);
                    auto o = jsi::Object(r2);
                    o.setProperty(r2,"rawResult", jsi::String::createFromUtf8(r2, raw));
                    o.setProperty(r2,"printed", true);
                    resolve.call(r2,o);
                });
            }));

    // --- setDateTime("yyyy-MM-dd HH:mm:ss") ---------------------------------
    mod.setProperty(rt, "setDateTime",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"setDateTime"),1,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value* a,size_t c)->jsi::Value {
                if(c<1||!a[0].isString()) return jsi::Value::undefined();
                std::string dt = a[0].asString(r).utf8(r);
                return makePromise(r, [dt](jsi::Runtime& r2,
                                           jsi::Function& resolve,
                                           jsi::Function& reject,
                                           JNIEnv* env,
                                           jobject fptr){

                    int Y,M,D,h,m,s;
                    if (sscanf(dt.c_str(), "%d-%d-%d %d:%d:%d", &Y,&M,&D,&h,&m,&s)!=6) {
                        reject.call(r2, jsi::String::createFromUtf8(r2,"Bad date format, expected yyyy-MM-dd HH:mm:ss"));
                        return;
                    }
                    long long ms = toEpochMsUtc(Y,M,D,h,m,s);

                    jclass cls = env->GetObjectClass(fptr);
                    if(!cls){ reject.call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed")); return; }
                    jmethodID midSetParamLong = safeGetMethod(env, cls, "setParam", "(IJ)V");
                    jmethodID midWriteDateTime= safeGetMethod(env, cls, "writeDateTime", "()I");
                    jmethodID midErrorCode    = safeGetMethod(env, cls, "errorCode", "()I");
                    jmethodID midErrorDesc    = safeGetMethod(env, cls, "errorDescription","()Ljava/lang/String;");
                    if(!midSetParamLong||!midWriteDateTime||!midErrorCode||!midErrorDesc){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,"Missing setDateTime methods"));
                        return;
                    }

                    env->CallVoidMethod(fptr, midSetParamLong, LIBFPTR_PARAM_DATE_TIME, (jlong)ms);
                    jint rc = env->CallIntMethod(fptr, midWriteDateTime);
                    bool hadEx = env->ExceptionCheck();
                    if(hadEx){ env->ExceptionDescribe(); env->ExceptionClear(); }
                    int err = env->CallIntMethod(fptr, midErrorCode);
                    std::string desc = readError(env,fptr,midErrorDesc);
                    if(hadEx||rc!=0||err!=0){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,
                            "setDateTime failed: "+desc+" (rc="+std::to_string(rc)+", err="+std::to_string(err)+")"));
                        return;
                    }
                    env->DeleteLocalRef(cls);
                    auto o = jsi::Object(r2);
                    o.setProperty(r2,"message", jsi::String::createFromUtf8(r2,"Custom date and time set successfully."));
                    resolve.call(r2,o);
                });
            }));

    // --- disconnect() -------------------------------------------------------
    mod.setProperty(rt, "disconnect",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"disconnect"),0,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value*,size_t)->jsi::Value {
                return makePromise(r, [](jsi::Runtime& r2,
                                         jsi::Function& resolve,
                                         jsi::Function& reject,
                                         JNIEnv* env,
                                         jobject fptr){
                    jclass cls = env->GetObjectClass(fptr);
                    if(!cls){ reject.call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed")); return; }
                    jmethodID midIsOpened = safeGetMethod(env, cls, "isOpened", "()Z");
                    jmethodID midClose    = safeGetMethod(env, cls, "close", "()I");
                    if(!midIsOpened||!midClose){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,"Missing disconnect methods"));
                        return;
                    }
                    jboolean opened = env->CallBooleanMethod(fptr, midIsOpened);
                    if(opened){
                        env->CallIntMethod(fptr, midClose);
                        if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }
                    }
                    env->DeleteLocalRef(cls);
                    auto o = jsi::Object(r2);
                    o.setProperty(r2,"disconnected", true);
                    resolve.call(r2,o);
                });
            }));

    // --- printXReport() -----------------------------------------------------
    mod.setProperty(rt, "printXReport",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"printXReport"),0,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value*,size_t)->jsi::Value {
                return makePromise(r, [](jsi::Runtime& r2,
                                         jsi::Function& resolve,
                                         jsi::Function& reject,
                                         JNIEnv* env,
                                         jobject fptr){
                    jclass cls = env->GetObjectClass(fptr);
                    if(!cls){ reject.call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed")); return; }

                    jmethodID midSetParamInt = safeGetMethod(env, cls, "setParam", "(II)V");
                    jmethodID midReport      = safeGetMethod(env, cls, "report", "()I");
                    jmethodID midErrorCode   = safeGetMethod(env, cls, "errorCode", "()I");
                    jmethodID midErrorDesc   = safeGetMethod(env, cls, "errorDescription","()Ljava/lang/String;");
                    if(!midSetParamInt||!midReport||!midErrorCode||!midErrorDesc){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,"Missing X report methods"));
                        return;
                    }

                    env->CallVoidMethod(fptr, midSetParamInt, LIBFPTR_PARAM_REPORT_TYPE, LIBFPTR_RT_X);
                    if(env->ExceptionCheck()){ env->ExceptionDescribe(); env->ExceptionClear(); }

                    jint rc = env->CallIntMethod(fptr, midReport);
                    bool hadEx = env->ExceptionCheck();
                    if(hadEx){ env->ExceptionDescribe(); env->ExceptionClear(); }
                    int err = env->CallIntMethod(fptr, midErrorCode);
                    std::string desc = readError(env,fptr,midErrorDesc);
                    if(hadEx||rc!=0||err!=0){
                        env->DeleteLocalRef(cls);
                        reject.call(r2, jsi::String::createFromUtf8(r2,
                          "X report failed: "+desc+" (rc="+std::to_string(rc)+", err="+std::to_string(err)+")"));
                        return;
                    }
                    auto o = jsi::Object(r2);
                    o.setProperty(r2,"rc",(double)rc);
                    o.setProperty(r2,"errCode",(double)err);
                    o.setProperty(r2,"description", jsi::String::createFromUtf8(r2, desc));
                    o.setProperty(r2,"reportType", jsi::String::createFromUtf8(r2,"X"));
                    env->DeleteLocalRef(cls);
                    resolve.call(r2,o);
                });
            }));

    // --- publish module -----------------------------------------------------
    rt.global().setProperty(rt, "__atolModule__", mod);
    logMsg("INIT","ATOL JSI module installed (unified working version).");
}

} // namespace atoljsi
