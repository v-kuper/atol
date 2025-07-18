#include "atol.h"
#include <jni.h>
#include <string>
#include <mutex>
#include <cctype>
#include <cstdio>
#include "fptr10/fptr10.h"

// ============================================================================
//  ATOL JSI MODULE (Clean Version + X Report)
// ============================================================================

using namespace facebook;

// ---------------- Configuration Macros --------------------------------------
#ifndef ATOL_VERBOSE_LOG
#define ATOL_VERBOSE_LOG 1
#endif

// Включить/выключить явную передачу модели через setSingleSetting(int,...)
#ifndef ATOL_USE_MODEL_SETTING
#define ATOL_USE_MODEL_SETTING 1
#endif

#ifndef ATOL_MODEL_VALUE
#define ATOL_MODEL_VALUE "500"    // AUTO (при необходимости заменить)
#endif

// ---------------- Fallback (если нет fptr10.h) ------------------------------
// Значения ПРОВЕРЬТЕ с вашим fptr10.h!
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
#define LIBFPTR_PARAM_MODEL_NAME    1
#define LIBFPTR_PARAM_SHIFT_STATE   1054
#define LIBFPTR_PARAM_SUM           1055
#endif

// Fallback для отчётов (замените при несовпадении!)
#ifndef LIBFPTR_PARAM_REPORT_TYPE
#define LIBFPTR_PARAM_REPORT_TYPE   100 // проверьте реальное значение
#define LIBFPTR_RT_X                2   // часто X отчёт = 2
#endif

// Доп. параметры (имя кассира и электронный чек) – используем ID по прежней логике
#ifndef LIBFPTR_PARAM_CASHIER_NAME
#define LIBFPTR_PARAM_CASHIER_NAME  1021
#endif
#ifndef LIBFPTR_PARAM_RECEIPT_ELECTRONICALLY
#define LIBFPTR_PARAM_RECEIPT_ELECTRONICALLY 1203
#endif

// ---------------- External JNI hooks ----------------------------------------
extern JavaVM* getJavaVM();
extern jobject getGlobalFptr();

namespace atoljsi {

static std::mutex gFptrMutex;

// ---------------- Logging ---------------------------------------------------
static inline void logMsg(const char* tag, const std::string& msg) {
#if ATOL_VERBOSE_LOG
    fprintf(stderr, "[ATOL-JSI][%s] %s\n", tag, msg.c_str());
#else
    (void)tag; (void)msg;
#endif
}

// ---------------- Helpers ---------------------------------------------------
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

// ---------------- configureFptr --------------------------------------------
bool configureFptr(JNIEnv* env,
                   jobject fptr,
                   const std::string& rawAddress,
                   const std::string& rawPort,
                   const std::string& name,
                   std::string& errorMsg) {
    errorMsg.clear();
    if (!env || !fptr) { errorMsg = "Null env/fptr"; return false; }

    std::string address = trim(rawAddress);
    std::string port    = trim(rawPort);

    jclass fptrClass = env->GetObjectClass(fptr);
    if (!fptrClass) { errorMsg = "GetObjectClass failed"; return false; }

    jmethodID midApplySingleSettings = safeGetMethod(env, fptrClass, "applySingleSettings", "()I");
    jmethodID midSetParamStr         = safeGetMethod(env, fptrClass, "setParam", "(ILjava/lang/String;)V");
    jmethodID midSetParamBool        = safeGetMethod(env, fptrClass, "setParam", "(IZ)V");
    jmethodID midErrorCode           = safeGetMethod(env, fptrClass, "errorCode", "()I");
    jmethodID midErrorDesc           = safeGetMethod(env, fptrClass, "errorDescription", "()Ljava/lang/String;");

    if (!midApplySingleSettings) {
        errorMsg = "applySingleSettings missing";
        env->DeleteLocalRef(fptrClass);
        return false;
    }

    jmethodID midSetSingleInt = env->GetMethodID(fptrClass, "setSingleSetting", "(ILjava/lang/String;)V");
    bool hasIntVariant = true;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        hasIntVariant = false;
        midSetSingleInt = nullptr;
    }

    jmethodID midSetSingleStr = nullptr;
    if (!hasIntVariant) {
        midSetSingleStr = env->GetMethodID(fptrClass, "setSingleSetting",
                                           "(Ljava/lang/String;Ljava/lang/String;)V");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe(); env->ExceptionClear();
            errorMsg = "No setSingleSetting variant";
            env->DeleteLocalRef(fptrClass);
            return false;
        }
    }

    logMsg("CFG", std::string("variant=") + (hasIntVariant?"INT":"STR") +
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

    if (hasIntVariant) {
#if ATOL_USE_MODEL_SETTING
        if (!putInt(LIBFPTR_SETTING_MODEL, ATOL_MODEL_VALUE)) goto fail;
#endif
        if (!putInt(LIBFPTR_SETTING_PORT, std::to_string(LIBFPTR_PORT_TCPIP))) goto fail;
        if (!putInt(LIBFPTR_SETTING_IPADDRESS, address)) goto fail;
        if (!putInt(LIBFPTR_SETTING_IPPORT, port)) goto fail;
    } else {
#if ATOL_USE_MODEL_SETTING
        if (!putStr("Model", ATOL_MODEL_VALUE)) goto fail;
#endif
        if (!putStr("Port", "2")) goto fail; // чаще всего "2" == TCP/IP
        if (!putStr("IPAddress", address)) goto fail;
        if (!putStr("IPPort", port)) goto fail;

        // Не критично, игнорируем ошибки
        putStr("MonitoringEnabled", "false");
        putStr("WebServerEnabled",  "false");
        putStr("TelemetryEnabled",  "false");
    }

    {
        jint applyRc = env->CallIntMethod(fptr, midApplySingleSettings);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe(); env->ExceptionClear();
            errorMsg = "Exception applySingleSettings";
            goto fail;
        }
        if (applyRc != 0) {
            jint ec = midErrorCode ? env->CallIntMethod(fptr, midErrorCode) : -1;
            std::string desc = readError(env, fptr, midErrorDesc);
            errorMsg = "applySingleSettings rc=" + std::to_string(applyRc) +
                       " code=" + std::to_string(ec) +
                       (desc.empty()? "" : " desc=" + desc);
            goto fail;
        }
    }

    // Имя кассира (опционально)
    if (!name.empty() && midSetParamStr) {
        jstring jName = env->NewStringUTF(name.c_str());
        env->CallVoidMethod(fptr, midSetParamStr, LIBFPTR_PARAM_CASHIER_NAME, jName);
        env->DeleteLocalRef(jName);
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    }

    // Электронный чек
    if (midSetParamBool) {
        env->CallVoidMethod(fptr, midSetParamBool,
                            LIBFPTR_PARAM_RECEIPT_ELECTRONICALLY, JNI_TRUE);
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    }

    env->DeleteLocalRef(fptrClass);
    return true;

fail:
    logMsg("CFG-ERR", errorMsg);
    env->DeleteLocalRef(fptrClass);
    return false;
}

// ---------------- getDeviceInfo --------------------------------------------
jsi::Object getDeviceInfo(JNIEnv* env, jobject fptr, jsi::Runtime& rt) {
    auto out = jsi::Object(rt);
    if (!env || !fptr) { out.setProperty(rt, "isConnected", false); return out; }

    jclass cls = env->GetObjectClass(fptr);
    if (!cls) { out.setProperty(rt, "isConnected", false); return out; }

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
        if (env->ExceptionCheck()) { env->ExceptionClear(); }

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

// ---------------- JSI install ----------------------------------------------
void install(jsi::Runtime &rt) {
    auto mod = jsi::Object(rt);

    // ---- Utility / test functions -----------------------------------------
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

    // ---- close() ----------------------------------------------------------
    mod.setProperty(rt, "close",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"close"),0,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value*,size_t)->jsi::Value {
                std::lock_guard<std::mutex> lg(gFptrMutex);
                JavaVM* jvm = getJavaVM(); if (!jvm) return jsi::Value::undefined();
                JNIEnv* env=nullptr; bool attached=false;
                if (jvm->GetEnv((void**)&env, JNI_VERSION_1_6)!=JNI_OK) {
#if defined(__ANDROID__)
                    if (jvm->AttachCurrentThread(&env,nullptr)!=JNI_OK)
                        return jsi::Value::undefined();
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

    // ---- connect(address, port, name) -> Promise(deviceInfo) --------------
    mod.setProperty(rt, "connect",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"connect"),3,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value* a,size_t c)->jsi::Value {
                if (c<3 || !a[0].isString() || !a[1].isString() || !a[2].isString())
                    return jsi::Value::undefined();
                std::string addr = trim(a[0].asString(r).utf8(r));
                std::string port = trim(a[1].asString(r).utf8(r));
                std::string name = trim(a[2].asString(r).utf8(r));

                auto Promise = r.global().getPropertyAsFunction(r,"Promise");
                auto executor = jsi::Function::createFromHostFunction(
                    r, jsi::PropNameID::forAscii(r,"executor"),2,
                    [addr,port,name](jsi::Runtime& r2,const jsi::Value&,const jsi::Value* p,size_t pc)->jsi::Value {
                        if (pc<2) return jsi::Value::undefined();
                        auto resolve = std::make_shared<jsi::Function>(p[0].getObject(r2).getFunction(r2));
                        auto reject  = std::make_shared<jsi::Function>(p[1].getObject(r2).getFunction(r2));

                        std::lock_guard<std::mutex> lg(gFptrMutex);
                        JavaVM* jvm = getJavaVM();
                        if (!jvm) { reject->call(r2, jsi::String::createFromUtf8(r2,"No JavaVM")); return jsi::Value::undefined(); }
                        JNIEnv* env=nullptr; bool attached=false;
                        if (jvm->GetEnv((void**)&env, JNI_VERSION_1_6)!=JNI_OK) {
#if defined(__ANDROID__)
                            if (jvm->AttachCurrentThread(&env,nullptr)!=JNI_OK) {
                                reject->call(r2, jsi::String::createFromUtf8(r2,"AttachCurrentThread failed"));
                                return jsi::Value::undefined();
                            }
                            attached=true;
#else
                            reject->call(r2, jsi::String::createFromUtf8(r2,"Cannot get JNIEnv"));
                            return jsi::Value::undefined();
#endif
                        }

                        jobject fptr = getGlobalFptr();
                        if (!fptr) {
                            reject->call(r2, jsi::String::createFromUtf8(r2,"FPTR not initialized"));
                            if (attached) jvm->DetachCurrentThread();
                            return jsi::Value::undefined();
                        }

                        std::string cfgErr;
                        if (!configureFptr(env,fptr,addr,port,name,cfgErr)) {
                            std::string msg="Failed to configure";
                            if(!cfgErr.empty()) msg+=": "+cfgErr;
                            reject->call(r2, jsi::String::createFromUtf8(r2,msg));
                            if (attached) jvm->DetachCurrentThread();
                            return jsi::Value::undefined();
                        }

                        jclass cls = env->GetObjectClass(fptr);
                        if (!cls) {
                            reject->call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed"));
                            if (attached) jvm->DetachCurrentThread();
                            return jsi::Value::undefined();
                        }

                        jmethodID midOpen      = safeGetMethod(env, cls, "open", "()I");
                        jmethodID midIsOpened  = safeGetMethod(env, cls, "isOpened", "()Z");
                        jmethodID midErrorCode = safeGetMethod(env, cls, "errorCode", "()I");
                        jmethodID midErrorDesc = safeGetMethod(env, cls, "errorDescription","()Ljava/lang/String;");
                        if(!midOpen||!midIsOpened||!midErrorCode||!midErrorDesc){
                            env->DeleteLocalRef(cls);
                            reject->call(r2, jsi::String::createFromUtf8(r2,"Missing methods"));
                            if (attached) jvm->DetachCurrentThread();
                            return jsi::Value::undefined();
                        }

                        jint openRc = env->CallIntMethod(fptr, midOpen);
                        bool hadEx = env->ExceptionCheck();
                        if (hadEx){ env->ExceptionDescribe(); env->ExceptionClear(); }
                        jboolean opened = env->CallBooleanMethod(fptr, midIsOpened);
                        jint errCode = env->CallIntMethod(fptr, midErrorCode);
                        std::string desc = readError(env, fptr, midErrorDesc);
                        logMsg("OPEN","openRc="+std::to_string(openRc)+" errCode="+std::to_string(errCode)+
                                       (desc.empty()?"":" desc="+desc));
                        if (hadEx || !opened || openRc!=0 || errCode!=0) {
                            std::string msg="Connection failed";
                            if(!desc.empty()) msg+=": "+desc;
                            msg+=" (openRc="+std::to_string(openRc)+", errCode="+std::to_string(errCode)+")";
                            env->DeleteLocalRef(cls);
                            reject->call(r2, jsi::String::createFromUtf8(r2,msg));
                            if (attached) jvm->DetachCurrentThread();
                            return jsi::Value::undefined();
                        }

                        auto info = getDeviceInfo(env,fptr,r2);
                        env->DeleteLocalRef(cls);
                        resolve->call(r2, info);
                        if (attached) jvm->DetachCurrentThread();
                        return jsi::Value::undefined();
                    });
                return Promise.callAsConstructor(r, executor);
            }));

    // ---- printXReport() -> Promise({rc, errCode, description}) -------------
    mod.setProperty(rt, "printXReport",
        jsi::Function::createFromHostFunction(
            rt, jsi::PropNameID::forAscii(rt,"printXReport"), 0,
            [](jsi::Runtime& r,const jsi::Value&,const jsi::Value*,size_t)->jsi::Value {
                auto Promise = r.global().getPropertyAsFunction(r,"Promise");
                auto executor = jsi::Function::createFromHostFunction(
                    r, jsi::PropNameID::forAscii(r,"executor"),2,
                    [](jsi::Runtime& r2,const jsi::Value&,const jsi::Value* p,size_t pc)->jsi::Value {
                        if (pc<2) return jsi::Value::undefined();
                        auto resolve = std::make_shared<jsi::Function>(p[0].getObject(r2).getFunction(r2));
                        auto reject  = std::make_shared<jsi::Function>(p[1].getObject(r2).getFunction(r2));

                        std::lock_guard<std::mutex> lg(gFptrMutex);
                        JavaVM* jvm = getJavaVM(); if(!jvm){ reject->call(r2, jsi::String::createFromUtf8(r2,"No JavaVM")); return jsi::Value::undefined(); }
                        JNIEnv* env=nullptr; bool attached=false;
                        if (jvm->GetEnv((void**)&env, JNI_VERSION_1_6)!=JNI_OK) {
#if defined(__ANDROID__)
                            if (jvm->AttachCurrentThread(&env,nullptr)!=JNI_OK) {
                                reject->call(r2, jsi::String::createFromUtf8(r2,"AttachCurrentThread failed"));
                                return jsi::Value::undefined();
                            }
                            attached=true;
#else
                            reject->call(r2, jsi::String::createFromUtf8(r2,"Cannot get JNIEnv"));
                            return jsi::Value::undefined();
#endif
                        }

                        jobject fptr = getGlobalFptr();
                        if(!fptr){
                            reject->call(r2, jsi::String::createFromUtf8(r2,"FPTR not initialized"));
                            if(attached) jvm->DetachCurrentThread();
                            return jsi::Value::undefined();
                        }

                        jclass cls = env->GetObjectClass(fptr);
                        if(!cls){
                            reject->call(r2, jsi::String::createFromUtf8(r2,"GetObjectClass failed"));
                            if(attached) jvm->DetachCurrentThread();
                            return jsi::Value::undefined();
                        }

                        jmethodID midSetParamInt    = safeGetMethod(env, cls, "setParam", "(II)V");
                        jmethodID midReport         = safeGetMethod(env, cls, "report", "()I");
                        jmethodID midErrorCode      = safeGetMethod(env, cls, "errorCode", "()I");
                        jmethodID midErrorDesc      = safeGetMethod(env, cls, "errorDescription","()Ljava/lang/String;");
                        if(!midSetParamInt||!midReport||!midErrorCode||!midErrorDesc){
                            env->DeleteLocalRef(cls);
                            reject->call(r2, jsi::String::createFromUtf8(r2,"Missing methods for report"));
                            if(attached) jvm->DetachCurrentThread();
                            return jsi::Value::undefined();
                        }

                        // Тип отчёта: X
                        env->CallVoidMethod(fptr, midSetParamInt, LIBFPTR_PARAM_REPORT_TYPE, LIBFPTR_RT_X);
                        if(env->ExceptionCheck()){
                            env->ExceptionDescribe(); env->ExceptionClear();
                            env->DeleteLocalRef(cls);
                            reject->call(r2, jsi::String::createFromUtf8(r2,"Exception setParam(reportType)"));
                            if(attached) jvm->DetachCurrentThread();
                            return jsi::Value::undefined();
                        }

                        jint rc = env->CallIntMethod(fptr, midReport);
                        bool hadEx = env->ExceptionCheck();
                        if (hadEx){ env->ExceptionDescribe(); env->ExceptionClear(); }
                        jint err = env->CallIntMethod(fptr, midErrorCode);
                        std::string desc = readError(env, fptr, midErrorDesc);

                        if (hadEx || rc!=0 || err!=0) {
                            std::string msg = "X report failed";
                            if(!desc.empty()) msg += ": "+desc;
                            msg += " (rc="+std::to_string(rc)+", err="+std::to_string(err)+")";
                            env->DeleteLocalRef(cls);
                            reject->call(r2, jsi::String::createFromUtf8(r2,msg));
                            if(attached) jvm->DetachCurrentThread();
                            return jsi::Value::undefined();
                        }

                        auto obj = jsi::Object(r2);
                        obj.setProperty(r2, "rc", (double)rc);
                        obj.setProperty(r2, "errCode", (double)err);
                        obj.setProperty(r2, "description",
                                        jsi::String::createFromUtf8(r2, desc));
                        obj.setProperty(r2, "reportType", jsi::String::createFromUtf8(r2,"X"));

                        env->DeleteLocalRef(cls);
                        resolve->call(r2, obj);

                        if(attached) jvm->DetachCurrentThread();
                        return jsi::Value::undefined();
                    });
                return Promise.callAsConstructor(r, executor);
            }));

    // ---- publish -----------------------------------------------------------
    rt.global().setProperty(rt, "__atolModule__", mod);
}

} // namespace atoljsi
