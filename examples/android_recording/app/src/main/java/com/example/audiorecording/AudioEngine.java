package com.example.audiorecording;

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


    public static boolean CreateAudioRecorder() {
        return createAudioRecorder();
    }

    public static void StartRecordingFile(String filePath) {
        startRecordingFile(filePath);
    }

    public static void StopRecording() {
        stopRecording();
    }

    public static void DeleteAudioRecorder() {
        deleteAudioRecorder();
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

    private static native boolean createAudioRecorder();
    private static native void deleteAudioRecorder();

    private static native void startRecordingFile(String filePath);

    private static native void stopRecording();
}
