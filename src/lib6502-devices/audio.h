#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <vector>
#include <mutex>

class AudioBuffer {
private:
    std::vector<float> buffer;
    size_t head;
    size_t tail;
    size_t count;
    std::mutex mutex;

public:
    AudioBuffer(size_t size = 4096);
    void push(float sample);
    float pop();
    size_t get_count();
};

void audio_init(int sample_rate = 44100);
void audio_close();
void audio_push_sample(float left, float right);

#endif
