package com.example.audiorecording;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder.AudioSource;
import android.os.Build;
import android.os.Process;

import androidx.annotation.Nullable;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.concurrent.TimeUnit;

public class AwAudioRecord {
    private static final boolean DEBUG = false;

    private static final String TAG = "AwAudioRecord";

    public static final int CHANNEL_IN_BACK_LEFT = 0x10000;
    public static final int CHANNEL_IN_BACK_RIGHT = 0x20000;
    public static final int CHANNEL_IN_CENTER = 0x40000;
    public static final int CHANNEL_IN_LOW_FREQUENCY = 0x100000;
    public static final int CHANNEL_IN_5POINT1 = (
            AudioFormat.CHANNEL_IN_LEFT | CHANNEL_IN_CENTER | AudioFormat.CHANNEL_IN_RIGHT | CHANNEL_IN_BACK_LEFT
                    | CHANNEL_IN_BACK_RIGHT | CHANNEL_IN_LOW_FREQUENCY);

    // Default audio data format is PCM 16 bit per sample.
    // Guaranteed to be supported by all devices.
    private static final int BITS_PER_SAMPLE = 16;

    // Requested size of each recorded buffer provided to the client.
    private static final int CALLBACK_BUFFER_SIZE_MS = 10;

    // Average number of callbacks per second.
    private static final int BUFFERS_PER_SECOND = 1000 / CALLBACK_BUFFER_SIZE_MS;

    // We ask for a native buffer size of BUFFER_SIZE_FACTOR * (minimum required
    // buffer size). The extra space is allocated to guard against glitches under
    // high load.
    private static final int BUFFER_SIZE_FACTOR = 2;

    // The AudioRecordJavaThread is allowed to wait for successful call to join()
    // but the wait times out afther this amount of time.
    private static final long AUDIO_RECORD_THREAD_JOIN_TIMEOUT_MS = 2000;

    private static final int DEFAULT_AUDIO_SOURCE = getDefaultAudioSource();
    private static int audioSource = DEFAULT_AUDIO_SOURCE;

    private final long nativeAudioRecord;

    private @Nullable AwAudioEffects effects;

    private ByteBuffer byteBuffer;

    private @Nullable AudioRecord audioRecord;
    private @Nullable AudioRecordThread audioThread;

    private static volatile boolean microphoneMute;
    private byte[] emptyBytes;

    // Audio recording error handler functions.
    public enum AudioRecordStartErrorCode {
        AUDIO_RECORD_START_EXCEPTION,
        AUDIO_RECORD_START_STATE_MISMATCH,
    }

    public static interface AwAudioRecordErrorCallback {
        void onAwAudioRecordInitError(String errorMessage);
        void onAwAudioRecordStartError(AudioRecordStartErrorCode errorCode, String errorMessage);
        void onAwAudioRecordError(String errorMessage);
    }

    private static @Nullable AwAudioRecordErrorCallback errorCallback;

    public static void setErrorCallback(AwAudioRecordErrorCallback errorCallback) {
        Logging.d(TAG, "Set error callback");
        AwAudioRecord.errorCallback = errorCallback;
    }

    /**
     * Contains audio sample information. Object is passed using {@link
     * AwAudioRecordSamplesReadyCallback}
     */
    public static class AudioSamples {
        /** See {@link AudioRecord#getAudioFormat()} */
        private final int audioFormat;
        /** See {@link AudioRecord#getChannelCount()} */
        private final int channelCount;
        /** See {@link AudioRecord#getSampleRate()} */
        private final int sampleRate;

        private final byte[] data;

        private AudioSamples(AudioRecord audioRecord, byte[] data) {
            this.audioFormat = audioRecord.getAudioFormat();
            this.channelCount = audioRecord.getChannelCount();
            this.sampleRate = audioRecord.getSampleRate();
            this.data = data;
        }

        public int getAudioFormat() {
            return audioFormat;
        }

        public int getChannelCount() {
            return channelCount;
        }

        public int getSampleRate() {
            return sampleRate;
        }

        public byte[] getData() {
            return data;
        }
    }

    /** Called when new audio samples are ready. This should only be set for debug purposes */
    public static interface AwAudioRecordSamplesReadyCallback {
        void onAwAudioRecordSamplesReady(AudioSamples samples);
    }

    private static @Nullable AwAudioRecordSamplesReadyCallback audioSamplesReadyCallback;

    public static void setOnAudioSamplesReady(AwAudioRecordSamplesReadyCallback callback) {
        audioSamplesReadyCallback = callback;
    }

    /**
     * Audio thread which keeps calling ByteBuffer.read() waiting for audio
     * to be recorded. Feeds recorded data to the native counterpart as a
     * periodic sequence of callbacks using DataIsRecorded().
     * This thread uses a Process.THREAD_PRIORITY_URGENT_AUDIO priority.
     */
    private class AudioRecordThread extends Thread {
        private volatile boolean keepAlive = true;

        public AudioRecordThread(String name) {
            super(name);
        }

        // TODO(titovartem) make correct fix during webrtc:9175
        @SuppressWarnings("ByteBufferBackingArray")
        @Override
        public void run() {
            Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
            Logging.d(TAG, "AudioRecordThread" + AwAudioUtils.getThreadInfo());
            assertTrue(audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING);

            long lastTime = System.nanoTime();
            while (keepAlive) {
                int bytesRead = audioRecord.read(byteBuffer, byteBuffer.capacity());
                if (bytesRead == byteBuffer.capacity()) {
                    if (microphoneMute) {
                        byteBuffer.clear();
                        byteBuffer.put(emptyBytes);
                    }
                    // It's possible we've been shut down during the read, and stopRecording() tried and
                    // failed to join this thread. To be a bit safer, try to avoid calling any native methods
                    // in case they've been unregistered after stopRecording() returned.
                    if (keepAlive) {
                        nativeDataIsRecorded(bytesRead, nativeAudioRecord);
                    }
                    if (audioSamplesReadyCallback != null) {
                        // Copy the entire byte buffer array.  Assume that the start of the byteBuffer is
                        // at index 0.
                        byte[] data = Arrays.copyOf(byteBuffer.array(), byteBuffer.capacity());
                        audioSamplesReadyCallback.onAwAudioRecordSamplesReady(
                                new AudioSamples(audioRecord, data));
                    }
                } else {
                    String errorMessage = "AudioRecord.read failed: " + bytesRead;
                    Logging.e(TAG, errorMessage);
                    if (bytesRead == AudioRecord.ERROR_INVALID_OPERATION) {
                        keepAlive = false;
                        reportAwAudioRecordError(errorMessage);
                    }
                }
                if (DEBUG) {
                    long nowTime = System.nanoTime();
                    long durationInMs = TimeUnit.NANOSECONDS.toMillis((nowTime - lastTime));
                    lastTime = nowTime;
                    Logging.d(TAG, "bytesRead[" + durationInMs + "] " + bytesRead);
                }
            }

            try {
                if (audioRecord != null) {
                    audioRecord.stop();
                }
            } catch (IllegalStateException e) {
                Logging.e(TAG, "AudioRecord.stop failed: " + e.getMessage());
            }
        }

        // Stops the inner thread loop and also calls AudioRecord.stop().
        // Does not block the calling thread.
        public void stopThread() {
            Logging.d(TAG, "stopThread");
            keepAlive = false;
        }
    }

    AwAudioRecord(long nativeAudioRecord) {
        Logging.d(TAG, "ctor" + AwAudioUtils.getThreadInfo());
        this.nativeAudioRecord = nativeAudioRecord;
        if (DEBUG) {
            AwAudioUtils.logDeviceInfo(TAG);
        }
        effects = AwAudioEffects.create();
    }

    private boolean enableBuiltInAEC(boolean enable) {
        Logging.d(TAG, "enableBuiltInAEC(" + enable + ')');
        if (effects == null) {
            Logging.e(TAG, "Built-in AEC is not supported on this platform");
            return false;
        }
        return effects.setAEC(enable);
    }

    private boolean enableBuiltInNS(boolean enable) {
        Logging.d(TAG, "enableBuiltInNS(" + enable + ')');
        if (effects == null) {
            Logging.e(TAG, "Built-in NS is not supported on this platform");
            return false;
        }
        return effects.setNS(enable);
    }

    private int initRecording(int sampleRate, int channels) {
        Logging.w(TAG, "initRecording(sampleRate=" + sampleRate + ", channels=" + channels + ")");
        if (audioRecord != null) {
            reportAwAudioRecordInitError("InitRecording called twice without StopRecording.");
            return -1;
        }
        final int bytesPerFrame = channels * (BITS_PER_SAMPLE / 8);
        final int framesPerBuffer = sampleRate / BUFFERS_PER_SECOND;
        byteBuffer = ByteBuffer.allocateDirect(bytesPerFrame * framesPerBuffer);
        Logging.d(TAG, "byteBuffer.capacity: " + byteBuffer.capacity());
        emptyBytes = new byte[byteBuffer.capacity()];
        // Rather than passing the ByteBuffer with every callback (requiring
        // the potentially expensive GetDirectBufferAddress) we simply have the
        // the native class cache the address to the memory once.
        nativeCacheDirectBufferAddress(byteBuffer, nativeAudioRecord);

        // Get the minimum buffer size required for the successful creation of
        // an AudioRecord object, in byte units.
        // Note that this size doesn't guarantee a smooth recording under load.
        final int channelConfig = channelCountToConfiguration(channels);
        int minBufferSize =
                AudioRecord.getMinBufferSize(sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT);
        if (minBufferSize == AudioRecord.ERROR || minBufferSize == AudioRecord.ERROR_BAD_VALUE) {
            reportAwAudioRecordInitError("AudioRecord.getMinBufferSize failed: " + minBufferSize);
            return -1;
        }
        Logging.d(TAG, "AudioRecord.getMinBufferSize: " + minBufferSize);

        // Use a larger buffer size than the minimum required when creating the
        // AudioRecord instance to ensure smooth recording under load. It has been
        // verified that it does not increase the actual recording latency.
        int bufferSizeInBytes = Math.max(BUFFER_SIZE_FACTOR * minBufferSize, byteBuffer.capacity());
        Logging.d(TAG, "bufferSizeInBytes: " + bufferSizeInBytes);
        try {
            audioRecord = new AudioRecord(audioSource, sampleRate, channelConfig,
                    AudioFormat.ENCODING_PCM_16BIT, bufferSizeInBytes);
        } catch (IllegalArgumentException e) {
            reportAwAudioRecordInitError("AudioRecord ctor error: " + e.getMessage());
            releaseAudioResources();
            return -1;
        }
        if (audioRecord == null || audioRecord.getState() != AudioRecord.STATE_INITIALIZED) {
            reportAwAudioRecordInitError("Failed to create a new AudioRecord instance");
            releaseAudioResources();
            return -1;
        }
        if (effects != null) {
            effects.enable(audioRecord.getAudioSessionId());
        }
        logMainParameters();
        logMainParametersExtended();
        return framesPerBuffer;
    }

    private boolean startRecording() {
        Logging.d(TAG, "startRecording");
        assertTrue(audioRecord != null);
        assertTrue(audioThread == null);
        try {
            audioRecord.startRecording();
        } catch (IllegalStateException e) {
            reportAwAudioRecordStartError(AudioRecordStartErrorCode.AUDIO_RECORD_START_EXCEPTION,
                    "AudioRecord.startRecording failed: " + e.getMessage());
            return false;
        }
        if (audioRecord.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
            reportAwAudioRecordStartError(
                    AudioRecordStartErrorCode.AUDIO_RECORD_START_STATE_MISMATCH,
                    "AudioRecord.startRecording failed - incorrect state :"
                            + audioRecord.getRecordingState());
            return false;
        }
        audioThread = new AudioRecordThread("AudioRecordJavaThread");
        audioThread.start();
        return true;
    }

    private boolean stopRecording() {
        Logging.d(TAG, "stopRecording");
        assertTrue(audioThread != null);
        audioThread.stopThread();
        if (!ThreadUtils.joinUninterruptibly(audioThread, AUDIO_RECORD_THREAD_JOIN_TIMEOUT_MS)) {
            Logging.e(TAG, "Join of AudioRecordJavaThread timed out");
            AwAudioUtils.logAudioState(TAG);
        }
        audioThread = null;
        if (effects != null) {
            effects.release();
        }
        releaseAudioResources();
        return true;
    }

    private void logMainParameters() {
        Logging.d(TAG, "AudioRecord: "
                + "session ID: " + audioRecord.getAudioSessionId() + ", "
                + "channels: " + audioRecord.getChannelCount() + ", "
                + "sample rate: " + audioRecord.getSampleRate());
    }

    private void logMainParametersExtended() {
        if (Build.VERSION.SDK_INT >= 23) {
            Logging.d(TAG, "AudioRecord: "
                    // The frame count of the native AudioRecord buffer.
                    + "buffer size in frames: " + audioRecord.getBufferSizeInFrames());
        }
    }

    // Helper method which throws an exception  when an assertion has failed.
    private static void assertTrue(boolean condition) {
        if (!condition) {
            throw new AssertionError("Expected condition to be true");
        }
    }

    private int channelCountToConfiguration(int channels) {
        int channelConfig = AudioFormat.CHANNEL_IN_MONO;
        switch (channels) {
            case 1:
                channelConfig = AudioFormat.CHANNEL_IN_MONO;
                break;
            case 2:
                channelConfig = AudioFormat.CHANNEL_IN_STEREO;
                break;
            case 6:
                channelConfig = CHANNEL_IN_5POINT1;
        }
        return channelConfig;
    }

    private native void nativeCacheDirectBufferAddress(ByteBuffer byteBuffer, long nativeAudioRecord);

    private native void nativeDataIsRecorded(int bytes, long nativeAudioRecord);

    @SuppressWarnings("NoSynchronizedMethodCheck")
    public static synchronized void setAudioSource(int source) {
        Logging.w(TAG, "Audio source is changed from: " + audioSource
                + " to " + source);
        audioSource = source;
    }

    private static int getDefaultAudioSource() {
        return AudioSource.VOICE_COMMUNICATION;
    }

    // Sets all recorded samples to zero if `mute` is true, i.e., ensures that
    // the microphone is muted.
    public static void setMicrophoneMute(boolean mute) {
        Logging.w(TAG, "setMicrophoneMute(" + mute + ")");
        microphoneMute = mute;
    }

    // Releases the native AudioRecord resources.
    private void releaseAudioResources() {
        Logging.d(TAG, "releaseAudioResources");
        if (audioRecord != null) {
            audioRecord.release();
            audioRecord = null;
        }
    }

    private void reportAwAudioRecordInitError(String errorMessage) {
        Logging.e(TAG, "Init recording error: " + errorMessage);
        AwAudioUtils.logAudioState(TAG);
        if (errorCallback != null) {
            errorCallback.onAwAudioRecordInitError(errorMessage);
        }
    }

    private void reportAwAudioRecordStartError(
            AudioRecordStartErrorCode errorCode, String errorMessage) {
        Logging.e(TAG, "Start recording error: " + errorCode + ". " + errorMessage);
        AwAudioUtils.logAudioState(TAG);
        if (errorCallback != null) {
            errorCallback.onAwAudioRecordStartError(errorCode, errorMessage);
        }
    }

    private void reportAwAudioRecordError(String errorMessage) {
        Logging.e(TAG, "Run-time recording error: " + errorMessage);
        AwAudioUtils.logAudioState(TAG);
        if (errorCallback != null) {
            errorCallback.onAwAudioRecordError(errorMessage);
        }
    }
}
