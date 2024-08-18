/*
 * Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <SLES/OpenSLES_Android.h>
#include <jni.h>
#include <sys/types.h>

#include <cassert>
#include <cstring>

#include "android_debug.h"
#include "audio_engine.h"
#include "jni_helper.h"
#include "utility/jvm.h"
#include "utility/jvm_android.h"
#include "utility/wav_file.h"

static std::unique_ptr<awaudio::AudioEngine> s_audio_engine;

JNIEXPORT jboolean JNICALL AudioEngine_createAudioEngine(JNIEnv* env,
                                                         jclass type,
                                                         jint j_audio_layer) {
    if (s_audio_engine) {
        LOGW("Audio engine has been created!");
        return true;
    }

    auto audio_engine = std::make_unique<awaudio::AudioEngine>();
    if (!audio_engine->Initialize(
            static_cast<awaudio::AudioLayer>(j_audio_layer))) {
        return false;
    }
    s_audio_engine = std::move(audio_engine);
    return true;
}

JNIEXPORT void JNICALL AudioEngine_deleteAudioEngine(JNIEnv* env, jclass type) {
    if (s_audio_engine) {
        s_audio_engine.reset();
    }
}

JNIEXPORT jboolean JNICALL AudioEngine_createAudioRecorder(JNIEnv* env,
                                                           jclass type) {
    if (!s_audio_engine) {
        LOGW("Audio engine has not been created for creating audio recorder!");
        return false;
    }
    return s_audio_engine->CreateAudioRecorder();
}

JNIEXPORT void JNICALL AudioEngine_deleteAudioRecorder(JNIEnv* env,
                                                       jclass type) {
    if (!s_audio_engine) {
        LOGW("Audio engine has not been created for deleting audio recorder!");
        return;
    }
    s_audio_engine->DeleteAudioRecorder();
}

static void monoToStereo(int16_t* dst,
                         int16_t* src,
                         size_t samples_per_channel) {
    for (size_t i = 0; i < samples_per_channel; ++i) {
        dst[2 * i] = src[i];
        dst[2 * i + 1] = src[i];
    }
}

static void stereoToMono(int16_t* dst,
                         int16_t* src,
                         size_t samples_per_channel) {
    for (size_t i = 0; i < samples_per_channel; ++i) {
        dst[i] = (src[i * 2] + src[i * 2 + 1]) / 2;
    }
}

class AudioRecordingFileDataCallbackImpl : public awaudio::AudioRecorderDataCallback {
public:
    AudioRecordingFileDataCallbackImpl(const std::string& filePath)
            : file_path_(filePath),
              record_audio_format_(awaudio::AUDIO_FORMAT_PCM_I16),
              record_channels_(0),
              record_samples_rate_hz_(0) {
    }

    virtual ~AudioRecordingFileDataCallbackImpl() {}

    int32_t DeliverRecordedData(const void* audio_buffer,
                                const size_t samples_per_channel,
                                const awaudio::audio_format_t format,
                                const size_t channels,
                                const uint32_t samples_rate_hz,
                                const uint32_t delay_mS,
                                uint32_t& new_mic_level) override {
        if (!wav_writer_) {
            wav_writer_ = std::make_unique<awaudio::WavWriter>(file_path_, samples_rate_hz, channels);
            LOGI("Wav file %s, sample rate %d, num channels %zu, num samples %zu",
                 file_path_.c_str(), wav_writer_->sample_rate(),
                 wav_writer_->num_channels(), wav_writer_->num_samples());
        }
        record_audio_format_ = format;
        record_channels_ = channels;
        record_samples_rate_hz_ = samples_rate_hz;

        size_t samples_count = samples_per_channel * channels;
        wav_writer_->WriteSamples((const int16_t *)audio_buffer, samples_count);

        return 0;
    }

private:
    std::string file_path_;
    std::unique_ptr<awaudio::WavWriter> wav_writer_;

    size_t record_samples_per_channel_;
    awaudio::audio_format_t record_audio_format_;
    size_t record_channels_;
    uint32_t record_samples_rate_hz_;
};

JNIEXPORT void JNICALL AudioEngine_startRecordingFile(JNIEnv* env,
                                                 jclass type,
                                                 jstring j_file_path) {
    if (!s_audio_engine) {
        LOGW("Audio engine has not been created for stopping!");
        return;
    }
    const char* file_path = env->GetStringUTFChars(j_file_path, NULL);

    auto data_callback =
            std::make_shared<AudioRecordingFileDataCallbackImpl>(file_path);
    s_audio_engine->SetAudioRecorderDataCallback(data_callback);
    s_audio_engine->StartRecording();

    if (file_path != NULL) {
        env->ReleaseStringUTFChars(j_file_path, file_path);
    }
}

JNIEXPORT void JNICALL AudioEngine_stopRecording(JNIEnv* env, jclass type) {
    if (!s_audio_engine) {
        LOGW("Audio engine has not been created for stopping!");
        return;
    }
    s_audio_engine->DeleteAudioRecorder();
}

// Dalvik VM type signatures
static const JNINativeMethod gMethods[] = {
        NATIVE_METHOD(AudioEngine, createAudioEngine, "(I)Z"),
        NATIVE_METHOD(AudioEngine, deleteAudioEngine, "()V"),
        NATIVE_METHOD(AudioEngine, createAudioRecorder, "()Z"),
        NATIVE_METHOD(AudioEngine, deleteAudioRecorder, "()V"),
        NATIVE_METHOD(AudioEngine, startRecordingFile, "(Ljava/lang/String;)V"),
        NATIVE_METHOD(AudioEngine, stopRecording, "()V"),
};

int jniRegisterNativeMethods(JNIEnv* env,
                             const char* classPathName,
                             const JNINativeMethod* nativeMethods,
                             jint nMethods) {
    jclass clazz;
    clazz = env->FindClass(classPathName);
    if (clazz == NULL) {
        LOGW("Native registration unable to find class '%s'", classPathName);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, nativeMethods, nMethods) < 0) {
        LOGW("RegisterNatives failed for '%s'", classPathName);
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM* vm, void* /* reserved */) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("JavaVM::GetEnv() failed");
        abort();
    }
    jniRegisterNativeMethods(env, "com/example/audiorecording/AudioEngine", gMethods,
                             NELEM(gMethods));
    awaudio::jni::InitGlobalJniVariables(vm);
    awaudio::JVM::Initialize(vm);

    return JNI_VERSION_1_6;
}
