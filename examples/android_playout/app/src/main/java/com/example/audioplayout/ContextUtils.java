package com.example.audioplayout;

import android.content.Context;

/**
 * Class for storing the application context and retrieving it in a static context. Similar to
 * org.chromium.base.ContextUtils.
 */
public class ContextUtils {
    private static final String TAG = "ContextUtils";
    private static Context applicationContext;

    /**
     * Stores the application context that will be returned by getApplicationContext. This is called
     * by PeerConnectionFactory.initialize. The application context must be set before creating
     * a PeerConnectionFactory and must not be modified while it is alive.
     */
    public static void initialize(Context applicationContext) {
        if (applicationContext == null) {
            throw new IllegalArgumentException(
                    "Application context cannot be null for ContextUtils.initialize.");
        }
        ContextUtils.applicationContext = applicationContext;
    }

    /**
     * Returns the stored application context.
     *
     * @deprecated crbug.com/webrtc/8937
     */
    @Deprecated
    public static Context getApplicationContext() {
        return applicationContext;
    }
}