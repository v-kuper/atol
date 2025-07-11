package com.atol;

import androidx.annotation.NonNull;

import com.facebook.react.bridge.JavaScriptContextHolder;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactMethod;

public class AtolModule extends NativeAtolSpec {
    public static final String NAME = "Atol";

    public AtolModule(ReactApplicationContext reactContext) {
        super(reactContext);
    }

    @NonNull
    @Override
    public String getName() {
        return NAME;
    }

    static {
        try {
            System.loadLibrary("react-native-atol");
        } catch (Exception e) {
            // Handle loading error
        }
    }

    public static native void nativeInstall(long jsiPtr);
    public static native void nativeInitFptrJNI();

    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public boolean install() {
        try {
            JavaScriptContextHolder contextHolder = getReactApplicationContext().getJavaScriptContextHolder();
            if (contextHolder != null) {
                nativeInitFptrJNI();
                nativeInstall(contextHolder.get());
                return true;
            }
        } catch (Exception e) {
            return false;
        }
        return false;
    }
}
