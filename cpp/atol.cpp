#include "atol.h"
// #include "./fptr10/fptr10.h"
#include <jni.h>

using namespace facebook;

namespace atoljsi {

void install(jsi::Runtime &rt) {
    // Создаем главный объект, который будет содержать все наши функции
    auto atolObject = jsi::Object(rt);

    // =====================================================================
    // ФУНКЦИЯ 1: reverseString - переворачивает строку задом наперед
    // =====================================================================
    auto reverseString = jsi::Function::createFromHostFunction(
        rt,                                                    // JSI Runtime
        jsi::PropNameID::forAscii(rt, "reverseString"),       // Имя функции
        1,                                                     // Количество параметров
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count > 0) {
                // Получаем строку из первого аргумента
                auto input = arguments[0].asString(runtime).utf8(runtime);
                // Создаем обратную строку используя reverse iterator
                std::string reversed(input.rbegin(), input.rend());
                // Возвращаем результат как JSI строку
                return jsi::String::createFromUtf8(runtime, reversed);
            }
            return jsi::Value::undefined(); // Если аргументов нет
        });

    // =====================================================================
    // ФУНКЦИЯ 2: getNumbers - возвращает массив чисел [1, 2, 3]
    // =====================================================================
    auto getNumbers = jsi::Function::createFromHostFunction(
        rt,
        jsi::PropNameID::forAscii(rt, "getNumbers"),
        0,                                                     // Не принимает параметров
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            // Создаем массив из 3 элементов
            auto array = jsi::Array(runtime, 3);
            // Заполняем массив числами 1, 2, 3
            array.setValueAtIndex(runtime, 0, jsi::Value(1));
            array.setValueAtIndex(runtime, 1, jsi::Value(2));
            array.setValueAtIndex(runtime, 2, jsi::Value(3));
            return array;
        });

    // =====================================================================
    // ФУНКЦИЯ 3: getObject - возвращает объект с информацией о модуле
    // =====================================================================
    auto getObject = jsi::Function::createFromHostFunction(
        rt,
        jsi::PropNameID::forAscii(rt, "getObject"),
        0,                                                     // Не принимает параметров
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            // Создаем новый объект
            auto obj = jsi::Object(runtime);
            // Добавляем свойства в объект
            obj.setProperty(runtime, "name", jsi::String::createFromUtf8(runtime, "Atol"));
            obj.setProperty(runtime, "version", jsi::String::createFromUtf8(runtime, "1.0.0"));
            return obj;
        });

    // =====================================================================
    // ФУНКЦИЯ 4: callMeLater - демонстрация работы с callback'ами
    // =====================================================================
    auto callMeLater = jsi::Function::createFromHostFunction(
        rt,
        jsi::PropNameID::forAscii(rt, "callMeLater"),
        2,                                                     // Принимает 2 параметра
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count >= 2) {
                // Первый аргумент - success callback функция
                auto successCallback = arguments[0].asObject(runtime).asFunction(runtime);
                // Имитируем асинхронный вызов - просто сразу вызываем success callback
                // В реальном приложении здесь был бы асинхронный код
                successCallback.call(runtime);
            }
            return jsi::Value::undefined();
        });

    // =====================================================================
    // ФУНКЦИЯ 5: promiseNumber - создает Promise, который удваивает число
    // =====================================================================
    auto promiseNumber = jsi::Function::createFromHostFunction(
        rt,
        jsi::PropNameID::forAscii(rt, "promiseNumber"),
        1,                                                     // Принимает 1 параметр (число)
        [](jsi::Runtime& runtime,
           const jsi::Value& /*thisValue*/,
           const jsi::Value* arguments,
           size_t count) -> jsi::Value {

            if (count == 0) return jsi::Value::undefined();

            // Получаем число из первого аргумента
            double number = arguments[0].asNumber();

            // Получаем конструктор Promise из глобального объекта JavaScript
            auto Promise = runtime.global().getPropertyAsFunction(runtime, "Promise");

            // Создаем executor функцию для Promise
            auto promiseCtor = jsi::Function::createFromHostFunction(
                runtime,
                jsi::PropNameID::forAscii(runtime, "executor"),
                2,                                             // resolve и reject callbacks
                [number](jsi::Runtime& rt,
                         const jsi::Value& /*thisVal*/,
                         const jsi::Value* args,
                         size_t count) -> jsi::Value {

                    if (count < 2) return jsi::Value::undefined();

                    // Создаем shared_ptr для resolve функции (для использования в callback)
                    auto resolvePtr = std::make_shared<jsi::Function>(args[0].getObject(rt).getFunction(rt));

                    // Создаем callback функцию для setTimeout
                    auto callback = jsi::Function::createFromHostFunction(
                        rt,
                        jsi::PropNameID::forAscii(rt, "callback"),
                        0,
                        [resolvePtr, number](jsi::Runtime& runtime,
                                           const jsi::Value& /*thisVal*/,
                                           const jsi::Value* /*args*/,
                                           size_t /*cnt*/) -> jsi::Value {
                            // Вызываем resolve с удвоенным числом
                            resolvePtr->call(runtime, jsi::Value(number * 2));
                            return jsi::Value::undefined();
                        });

                    // Используем setTimeout для имитации асинхронности (задержка 1000мс)
                    rt.global()
                        .getPropertyAsFunction(rt, "setTimeout")
                        .call(rt, callback, jsi::Value(1000));

                    return jsi::Value::undefined();
                });

            // Создаем и возвращаем Promise с нашим executor'ом
            return Promise.callAsConstructor(runtime, promiseCtor);
        });

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

                        JNIEnv *env = nullptr;
                        jni::getJavaVM()->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
                        if (!env) {
                            reject->call(rt, jsi::String::createFromUtf8(rt, "JNI error"));
                            return jsi::Value::undefined();
                        }

                        jclass fptrClass = env->GetObjectClass(g_fptr);

                        // Настройка
                        jmethodID setSetting = env->GetMethodID(fptrClass, "setSingleSetting",
                                                                "(Ljava/lang/String;Ljava/lang/String;)V");
                        jmethodID applySettings = env->GetMethodID(fptrClass, "applySingleSettings", "()V");
                        jmethodID setParam = env->GetMethodID(fptrClass, "setParam", "(ILjava/lang/String;)V");
                        jmethodID open = env->GetMethodID(fptrClass, "open", "()V");
                        jmethodID isOpened = env->GetMethodID(fptrClass, "isOpened", "()Z");
                        jmethodID errorCode = env->GetMethodID(fptrClass, "errorCode", "()I");
                        jmethodID errorDescription = env->GetMethodID(fptrClass, "errorDescription", "()Ljava/lang/String;");

                        jstring jModel = env->NewStringUTF("0"); // ATOL_AUTO
                        jstring jAddr = env->NewStringUTF(address.c_str());
                        jstring jPort = env->NewStringUTF(port.c_str());
                        jstring jTcp = env->NewStringUTF("2"); // TCP/IP
                        jstring jName = env->NewStringUTF(name.c_str());

                        // Устанавливаем настройки
                        env->CallVoidMethod(g_fptr, setSetting, env->NewStringUTF("model"), jModel);
                        env->CallVoidMethod(g_fptr, setSetting, env->NewStringUTF("ipaddress"), jAddr);
                        env->CallVoidMethod(g_fptr, setSetting, env->NewStringUTF("ipport"), jPort);
                        env->CallVoidMethod(g_fptr, setSetting, env->NewStringUTF("port"), jTcp);

                        // Применяем настройки
                        env->CallVoidMethod(g_fptr, applySettings);

                        // Устанавливаем имя устройства, если передано
                        if (!name.empty()) {
                            env->CallVoidMethod(g_fptr, setParam, 1021, jName);
                        }

                        // Печать электронно
                        jmethodID setBoolParam = env->GetMethodID(fptrClass, "setParam", "(IZ)V");
                        env->CallVoidMethod(g_fptr, setBoolParam, 1203, true); // LIBFPTR_PARAM_RECEIPT_ELECTRONICALLY = 1203

                        // Подключение
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
    // РЕГИСТРАЦИЯ ФУНКЦИЙ: добавляем все функции в главный объект
    // =====================================================================
    atolObject.setProperty(rt, "reverseString", std::move(reverseString));
    atolObject.setProperty(rt, "getNumbers", std::move(getNumbers));
    atolObject.setProperty(rt, "getObject", std::move(getObject));
    atolObject.setProperty(rt, "callMeLater", std::move(callMeLater));
    atolObject.setProperty(rt, "promiseNumber", std::move(promiseNumber));
    atolObject.setProperty(rt, "connect", std::move(connect));

    // =====================================================================
    // УСТАНОВКА ГЛОБАЛЬНОЙ ПЕРЕМЕННОЙ: делаем модуль доступным в JavaScript
    // =====================================================================
    // Устанавливаем объект как глобальную переменную __atolModule__
    // Теперь из JavaScript можно вызывать: __atolModule__.reverseString("hello")
    rt.global().setProperty(rt, "__atolModule__", std::move(atolObject));
}

}
