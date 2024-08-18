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

JNIEXPORT jboolean JNICALL AudioEngine_createAudioPlayer(JNIEnv* env,
                                                         jclass type,
                                                         jint streamType) {
    if (!s_audio_engine) {
        LOGW("Audio engine has not been created for creating audio player!");
        return false;
    }
    return s_audio_engine->CreateAudioPlayer(streamType);
}

JNIEXPORT void JNICALL AudioEngine_deleteAudioPlayer(JNIEnv* env, jclass type) {
    if (!s_audio_engine) {
        LOGW("Audio engine has not been created for deleting audio player!");
        return;
    }
    s_audio_engine->DeleteAudioPlayer();
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

class AudioPlayFileDataCallbackImpl : public awaudio::AudioPlayerDataCallback {
public:
    AudioPlayFileDataCallbackImpl(const std::string& filePath)
            : file_path_(filePath),
              playout_samples_per_channel_(0),
              playout_audio_format_(awaudio::AUDIO_FORMAT_PCM_I16),
              playout_channels_(0),
              playout_samples_rate_hz_(0) {
        wav_reader_ = std::make_unique<awaudio::WavReader>(filePath);
        LOGI("Wav file %s, sample rate %d, num channels %zu, num samples %zu",
             file_path_.c_str(), wav_reader_->sample_rate(),
             wav_reader_->num_channels(), wav_reader_->num_samples());
    }

    virtual ~AudioPlayFileDataCallbackImpl() {}

    int32_t RequestPlayoutData(const size_t samples_per_channel,
                               const awaudio::audio_format_t format,
                               const size_t channels,
                               const uint32_t samples_rate_hz) override {
        playout_samples_per_channel_ = samples_per_channel;
        playout_audio_format_ = format;
        playout_channels_ = channels;
        playout_samples_rate_hz_ = samples_rate_hz;
        return playout_samples_per_channel_;
    }

    int32_t GetPlayoutData(void* audio_buffer) {
        int16_t* audio_data = static_cast<int16_t*>(audio_buffer);
        size_t samples_per_channel = playout_samples_per_channel_;
        size_t samples_count = samples_per_channel * playout_channels_;

        size_t file_channels = wav_reader_->num_channels();
        if (playout_channels_ == file_channels) {
            if (samples_count > 0) {
                size_t sample_read =
                        wav_reader_->ReadSamples(samples_count, audio_data);
                if (sample_read < samples_count) {
                    wav_reader_->Reset();
                }
            }
        } else {
            if (samples_count > 0) {
                size_t target_sample_count = samples_per_channel * file_channels;
                std::unique_ptr<int16_t[]> data_buffer(
                        new int16_t[target_sample_count]);
                size_t sample_read =
                        wav_reader_->ReadSamples(target_sample_count, data_buffer.get());
                if (sample_read < target_sample_count) {
                    wav_reader_->Reset();
                }

                if (playout_channels_ == 1 && file_channels == 2) {
                    stereoToMono(audio_data, data_buffer.get(), samples_per_channel);
                } else if (playout_channels_ == 2 && file_channels == 1) {
                    monoToStereo(audio_data, data_buffer.get(), samples_per_channel);
                } else {
                    for (size_t sample = 0; sample < playout_samples_per_channel_;
                         ++sample) {
                        for (size_t channel = 0; channel < playout_channels_; ++channel) {
                            audio_data[sample * playout_channels_ + channel] =
                                    data_buffer[sample * file_channels + channel % file_channels];
                        }
                    }
                }
            }
        }

        return samples_count / playout_channels_;
    }

private:
    std::string file_path_;
    std::unique_ptr<awaudio::WavReader> wav_reader_;

    size_t playout_samples_per_channel_;
    awaudio::audio_format_t playout_audio_format_;
    size_t playout_channels_;
    uint32_t playout_samples_rate_hz_;
};

JNIEXPORT void JNICALL AudioEngine_startPlayFile(JNIEnv* env,
                                                 jclass type,
                                                 jstring j_file_path) {
    if (!s_audio_engine) {
        LOGW("Audio engine has not been created for stopping!");
        return;
    }
    const char* file_path = env->GetStringUTFChars(j_file_path, NULL);

    auto data_callback =
            std::make_shared<AudioPlayFileDataCallbackImpl>(file_path);
    s_audio_engine->SetAudioPlayerDataCallback(data_callback);
    s_audio_engine->StartPlayout();

    if (file_path != NULL) {
        env->ReleaseStringUTFChars(j_file_path, file_path);
    }
}

JNIEXPORT jboolean JNICALL AudioEngine_setPlayoutVolume(JNIEnv* env,
                                                        jclass type,
                                                        jint volume) {
    if (!s_audio_engine) {
        LOGW("Audio engine has not been created for stopping!");
        return false;
    }
    return s_audio_engine->SetPlayoutVolume(volume);
}

JNIEXPORT void JNICALL AudioEngine_stopPlay(JNIEnv* env, jclass type) {
    if (!s_audio_engine) {
        LOGW("Audio engine has not been created for stopping!");
        return;
    }
    s_audio_engine->DeleteAudioPlayer();
}

// Dalvik VM type signatures
static const JNINativeMethod gMethods[] = {
        NATIVE_METHOD(AudioEngine, createAudioEngine, "(I)Z"),
        NATIVE_METHOD(AudioEngine, deleteAudioEngine, "()V"),
        NATIVE_METHOD(AudioEngine, createAudioPlayer, "(I)Z"),
        NATIVE_METHOD(AudioEngine, deleteAudioPlayer, "()V"),
        NATIVE_METHOD(AudioEngine, startPlayFile, "(Ljava/lang/String;)V"),
        NATIVE_METHOD(AudioEngine, setPlayoutVolume, "(I)Z"),
        NATIVE_METHOD(AudioEngine, stopPlay, "()V"),
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
    jniRegisterNativeMethods(env, "com/example/audioplayout/AudioEngine", gMethods,
                             NELEM(gMethods));
    awaudio::jni::InitGlobalJniVariables(vm);
    awaudio::JVM::Initialize(vm);

    return JNI_VERSION_1_6;
}
