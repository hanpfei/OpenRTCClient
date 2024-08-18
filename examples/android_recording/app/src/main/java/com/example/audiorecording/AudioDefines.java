package com.example.audiorecording;

public class AudioDefines {
    public enum AudioLayer {
        kPlatformDefaultAudio,
        kAndroidJavaAudio,
        kAndroidOpenSLESAudio,
        kAndroidJavaInputAndOpenSLESOutputAudio,
        kAndroidAAudioAudio,
        kAndroidJavaInputAndAAudioOutputAudio,
    };
}
