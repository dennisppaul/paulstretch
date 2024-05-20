#include <fstream>
#include <iostream>
#include <vector>

#include "ProcessedStretch.h"
#include "Input/MP3InputS.h"

using namespace std;

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
            cout << "." << flush;
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

string render_file(string inaudio, string outaudio) {
    InputS* ai = new MP3InputS;
    if (!ai->open(inaudio)) {
        return "Error: Could not open audio file (or file format not "
            "recognized): " +
            inaudio;
    }

    // 32bit PCM, little-endian, mono, 44100 Hz
    std::ofstream outfile(outaudio, std::ios::binary | std::ios::out);

    int mBufferSize = 44100 * 0.25;
    short int* mReadBuffer = new short int[mBufferSize * 8];
    PaulStretch stretch(8, mBufferSize, 44100);

    while (!ai->eof) {
        /**
         * steps to use PaulStretch
         * 1. get required samples
         * 2. fill input buffer with number of required samples
         * 3. process input buffer
         * 4. use output buffer samples
        */
        int mNumRequestedSamples = stretch.get_required_samples();
        cout << "get_required_samples: " << mNumRequestedSamples << endl;
        int mNumReadSamples = ai->read(mNumRequestedSamples, mReadBuffer);
#ifdef USE_FILL_BUFFER
        float* mInputBuffer = new float[mNumReadSamples];
        for (int i = 0; i < mNumReadSamples; i++) {
            mInputBuffer[i] = mReadBuffer[i * 2] / 32768.0;
            stretch.get_input_buffer()[i] = mReadBuffer[i * 2] / 32768.0;
        }
        stretch.fill_input_buffer(mInputBuffer, mNumRequestedSamples);
        delete[] mInputBuffer;
#else
        for (int i = 0; i < mNumReadSamples; i++) {
            stretch.get_input_buffer()[i] = mReadBuffer[i * 2] / 32768.0;
        }
#endif
        vector<float> mSamples = stretch.process();

        cout << "processed samples: " << mSamples.size() << endl;
        int* outbuf = new int[mSamples.size()];
        for (int i = 0; i < mSamples.size(); i++) {
            float l = mSamples[i];
            if (l < -1.0)
                l = -1.0;
            else if (l > 1.0)
                l = 1.0;
            outbuf[i] = (int)(l * 32767.0 * 65536.0);
        };
        outfile.write(reinterpret_cast<char*>(outbuf), mSamples.size() * sizeof(int));
    }
    outfile.close();

    return "OK";
}

int main(int argc, char** argv) {
    cout << "+++ paulstretch" << endl << endl;
    if (argc == 3) {
        cout << render_file(argv[1], argv[2]) << endl;
    }
}
