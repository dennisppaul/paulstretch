#pragma once

#include <vector>
#include <iostream>

#include "Stretch.h"

class PaulStretch {
public:
    PaulStretch(int stretch, int buffer_size, int samplerate) :
        pInputBufferSize(buffer_size),
        pStretch(stretch),
        pSampleRate(samplerate) {
        stretcher = new ProcessedStretch(stretch,
            buffer_size,
            W_HANN,
            false,
            samplerate, 0);
        stretcher->set_parameters(&pParameters);
        stretcher->set_onset_detection_sensitivity(0.0);
        outbufsize = stretcher->get_bufsize();
        const int mPoolSize = stretcher->get_max_bufsize();

        pInputBuffer = new float[mPoolSize];
        for (int i = 0; i < mPoolSize; i++) {
            pInputBuffer[i] = 0.0;
        }

        pRequiredSamples = stretcher->get_nsamples_for_fill();
    }

    ~PaulStretch() {
        delete stretcher;
        delete pInputBuffer;
    }

    void fill_input_buffer(float* input_buffer, int number_of_samples) {
        for (int i = 0; i < number_of_samples; i++) {
            pInputBuffer[i] = input_buffer[i];
        }
    }

    float* get_input_buffer() {
        return pInputBuffer;
    }

    std::vector<float> process() {
        std::vector<float> mSamples;
        do {
            stretcher->process(pInputBuffer, pRequiredSamples);
            pRequiredSamples = 0;
            for (int i = 0; i < outbufsize; i++) {
                mSamples.push_back(stretcher->out_buf[i]);
            }
            pRequiredSamples = stretcher->get_nsamples(0);
        } while (pRequiredSamples == 0);
        return mSamples;
    }

    int get_required_samples() {
        return pRequiredSamples;
    }

private:
    ProcessedStretch* stretcher;
    ProcessParameters pParameters;
    int pInputBufferSize;
    int pStretch;
    int pSampleRate;
    int outbufsize;
    float* pInputBuffer;
    int pRequiredSamples;
};
