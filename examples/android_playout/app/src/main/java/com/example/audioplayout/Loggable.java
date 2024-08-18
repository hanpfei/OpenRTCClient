package com.example.audioplayout;


/**
 * Java interface for WebRTC logging. The default implementation uses webrtc.Logging.
 *
 * When injected, the Loggable will receive logging from both Java and native.
 */
public interface Loggable {
    public void onLogMessage(String message, Logging.Severity severity, String tag);
}