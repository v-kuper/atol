package com.atol;

import androidx.annotation.NonNull;

import com.facebook.react.bridge.JavaScriptContextHolder;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactMethod;
import ru.atol.drivers10.fptr.Fptr;
import ru.atol.drivers10.fptr.IFptr;

public class AtolModule extends NativeAtolSpec {
    public static final String NAME = "Atol";

    private static IFptr fptr;

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
            System.loadLibrary("fptr10");
            fptr = new Fptr(null, null);
        } catch (Exception e) {
            // Handle loading error
        }
    }

    public static IFptr getFptr() {
        return fptr;
    }

    public static native void nativeInstall(long jsiPtr);
    public static native void nativePassFptrToCpp();

    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public boolean install() {
        try {
            JavaScriptContextHolder contextHolder = getReactApplicationContext().getJavaScriptContextHolder();
            if (contextHolder != null) {
                nativePassFptrToCpp();
                nativeInstall(contextHolder.get());
                return true;
            }
        } catch (Exception e) {
            return false;
        }
        return false;
    }
}
