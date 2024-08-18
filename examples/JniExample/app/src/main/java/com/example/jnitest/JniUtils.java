package com.example.jnitest;

import android.util.Log;

public class JniUtils {
    static {
        System.loadLibrary("jnitest");
    }

    private static final String TAG = "JniUtils";
    private static int intFieldStatic = 3456;
    private int intField = 6543;

    public static native boolean booleanParam(boolean value);

    public static native byte byteParam(byte value);

    public static native char charParam(char value);

    public static native short shortParam(short value);
    public static native int intParam(int value);

    public static native long longParam(long value);

    public static native float floatParam(float value);

    public static native double doubleParam(double value);

    public static native boolean voidParam();

    public native int[] intArrayParam(int[] value);

    public native String stringParam(String value);

    public native String methodThrowException(String value) throws RuntimeException;

    public native boolean startNativeThread(int param);

    public native boolean stopNativeThread();

    private static String accessFromNativeStatic() {
        String str = "access from native static";
        Log.w(TAG, str);
        return str;
    }

    private String accessFromNative() {
        String str = "access from native";
        Log.w(TAG, str);
        return str;
    }

    private String accessFromNativeException() throws RuntimeException {
        String str = "access from native";
        Log.w(TAG, str);
        if (true) {
            throw new RuntimeException();
        }

        return str;
    }
}
