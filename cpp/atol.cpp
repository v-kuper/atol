#include "atol.h"

using namespace facebook;

namespace atoljsi {

void install(jsi::Runtime &rt) {
    // Создаем объект с функциями
    auto atolObject = jsi::Object(rt);

    // reverseString function
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

    // getNumbers function
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

    // getObject function
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

    // callMeLater function (с callback)
    auto callMeLater = jsi::Function::createFromHostFunction(
        rt,
        jsi::PropNameID::forAscii(rt, "callMeLater"),
        2,
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count >= 2) {
                // Симулируем асинхронный вызов - просто вызываем success callback
                auto successCallback = arguments[0].asObject(runtime).asFunction(runtime);
                successCallback.call(runtime);
            }
            return jsi::Value::undefined();
        });

    // promiseNumber function
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

            // Grab JS Promise constructor
            auto Promise = runtime.global().getPropertyAsFunction(runtime, "Promise");

            // Executor
            auto promiseCtor = jsi::Function::createFromHostFunction(
                runtime,
                jsi::PropNameID::forAscii(runtime, "executor"),
                2,
                [number](jsi::Runtime& rt,
                         const jsi::Value& /*thisVal*/,
                         const jsi::Value* args,
                         size_t count) -> jsi::Value {
                    if (count < 2) return jsi::Value::undefined();

                    // move-only -> shared_ptr wrapper
                    auto resolvePtr =
                        std::make_shared<jsi::Function>(args[0].getObject(rt).getFunction(rt));
                    /* auto rejectPtr =
                        std::make_shared<jsi::Function>(args[1].getObject(rt).getFunction(rt)); */

                    auto callback = jsi::Function::createFromHostFunction(
                        rt,
                        jsi::PropNameID::forAscii(rt, "callback"),
                        0,
                        [resolvePtr,
                         /* rejectPtr, */ number](jsi::Runtime& runtime,
                                                  const jsi::Value& /*thisVal*/,
                                                  const jsi::Value* /*args*/,
                                                  size_t /*cnt*/) -> jsi::Value {
                            // resolve(number * 2) after timeout
                            resolvePtr->call(runtime, jsi::Value(number * 2));
                            return jsi::Value::undefined();
                        });

                    rt.global()
                        .getPropertyAsFunction(rt, "setTimeout")
                        .call(rt, callback, jsi::Value(100));

                    return jsi::Value::undefined();
                });

            return Promise.callAsConstructor(runtime, promiseCtor);
        });

    atolObject.setProperty(rt, "reverseString", std::move(reverseString));
    atolObject.setProperty(rt, "getNumbers", std::move(getNumbers));
    atolObject.setProperty(rt, "getObject", std::move(getObject));
    atolObject.setProperty(rt, "callMeLater", std::move(callMeLater));
    atolObject.setProperty(rt, "promiseNumber", std::move(promiseNumber));

    // Устанавливаем глобальную переменную с правильным именем
    rt.global().setProperty(rt, "__atolModule__", std::move(atolObject));
}

}
