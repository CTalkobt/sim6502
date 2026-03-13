#include "audio.h"
#include <SDL2/SDL.h>
#include <algorithm>

static SDL_AudioDeviceID g_audio_device = 0;
static AudioBuffer g_audio_buffer_l(16384);
static AudioBuffer g_audio_buffer_r(16384);

AudioBuffer::AudioBuffer(size_t size) : buffer(size), head(0), tail(0), count(0) {}

void AudioBuffer::push(float sample) {
    std::lock_guard<std::mutex> lock(mutex);
    if (count < buffer.size()) {
        buffer[head] = sample;
        head = (head + 1) % buffer.size();
        count++;
    }
}

float AudioBuffer::pop() {
    std::lock_guard<std::mutex> lock(mutex);
    if (count > 0) {
        float sample = buffer[tail];
        tail = (tail + 1) % buffer.size();
        count--;
        return sample;
    }
    return 0.0f;
}

size_t AudioBuffer::get_count() {
    std::lock_guard<std::mutex> lock(mutex);
    return count;
}

static void audio_callback(void* userdata, Uint8* stream, int len) {
    (void)userdata;
    float* fstream = (float*)stream;
    int samples = len / (sizeof(float) * 2);

    for (int i = 0; i < samples; i++) {
        fstream[i * 2 + 0] = g_audio_buffer_l.pop();
        fstream[i * 2 + 1] = g_audio_buffer_r.pop();
    }
}

void audio_init(int sample_rate) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return;

    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    desired.freq = sample_rate;
    desired.format = AUDIO_F32;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = audio_callback;

    g_audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (g_audio_device != 0) {
        SDL_PauseAudioDevice(g_audio_device, 0);
    }
}

void audio_close() {
    if (g_audio_device != 0) {
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }
}

void audio_push_sample(float left, float right) {
    g_audio_buffer_l.push(left);
    g_audio_buffer_r.push(right);
}
