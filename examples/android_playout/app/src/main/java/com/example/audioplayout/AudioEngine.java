package com.example.audioplayout;

public class AudioEngine {
    /*
     * Loading our lib
     */
    static {
        System.loadLibrary("awaudio");
    }


    public static boolean CreateAudioEngine(AudioDefines.AudioLayer audioLayer) {
        return createAudioEngine(toNativeAudioLayer(audioLayer));
    }

    public static void DeleteAudioEngine() {
        deleteAudioEngine();
    }

    public static boolean CreateAudioPlayer(int streamType) {
        return createAudioPlayer(streamType);
    }

    public static void DeleteAudioPlayer() {
        deleteAudioPlayer();
    }

    public static void StartPlayFile(String filePath) {
        startPlayFile(filePath);
    }

    public static boolean SetPlayoutVolume(int volume) {
        return setPlayoutVolume(volume);
    }

    public static void StopPlay() {
        stopPlay();
    }

    private static int toNativeAudioLayer(AudioDefines.AudioLayer audioLayer) {
        int nativeAudioLayer = 0;
        if (audioLayer == AudioDefines.AudioLayer.kAndroidJavaAudio) {
            nativeAudioLayer = 1;
        }
        return nativeAudioLayer;
    }

    private static native boolean createAudioEngine(int audioLayer);
    private static native void deleteAudioEngine();

    private static native boolean createAudioPlayer(int streamType);
    private static native void deleteAudioPlayer();

    private static native void startPlayFile(String filePath);
    private static native boolean setPlayoutVolume(int volume);

    private static native void stopPlay();
}
