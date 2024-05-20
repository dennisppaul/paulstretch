#include <fstream>
#include <iostream>
#include <vector>

#include "BinauralBeats.h"
#include "ProcessedStretch.h"
#include "globals.h"

#include "Input/MP3InputS.h"

#define RENDER_LIKE_AUD

struct {
    int bufsize;
    double stretch;
    double onset_detection_sensitivity;
} process;

ProcessParameters ppar;
BinauralBeatsParameters bbpar;
FFTWindow window_type;
bool wav32bit;

using namespace std;

string Render(string inaudio, string outaudio) {
    InputS *ai = new MP3InputS;
    if (!ai->open(inaudio)) {
        return "Error: Could not open audio file (or file format not "
               "recognized) :" +
               inaudio;
    };
#ifdef USE_EXTRAS
    BinauralBeats bb(ai->info.samplerate);
    bb.pars = bbpar;
#endif

    int inbufsize = process.bufsize;

    if (inbufsize < 32)
        inbufsize = 32;
    short int *inbuf_i = new short int[inbufsize * 8];
    int outbufsize;
    struct {
        float *l, *r;
    } inbuf;
    ProcessedStretch *stretchl = new ProcessedStretch(
        process.stretch, inbufsize, window_type, false, ai->info.samplerate, 1);
    ProcessedStretch *stretchr = new ProcessedStretch(
        process.stretch, inbufsize, window_type, false, ai->info.samplerate, 2);
    stretchl->set_onset_detection_sensitivity(
        process.onset_detection_sensitivity);
    stretchr->set_onset_detection_sensitivity(
        process.onset_detection_sensitivity);
    stretchl->set_parameters(&ppar);
    stretchr->set_parameters(&ppar);

    outbufsize = stretchl->get_bufsize();
    int *outbuf = new int[outbufsize * 2];
    cout << "outbufsize(" << outbufsize << ") " << endl;

    int poolsize = stretchl->get_max_bufsize();
    cout << "poolsize(" << poolsize << ") " << endl;

    inbuf.l = new float[poolsize];
    inbuf.r = new float[poolsize];
    for (int i = 0; i < poolsize; i++)
        inbuf.l[i] = inbuf.r[i] = 0.0;

    int readsize = 0;
    const int pause_max_write = 65536;
    int pause_write = 0;
    bool firstbuf = true;

    // 32bit PCM, little-endian, stereo, 44100 Hz
    std::ofstream outfile(outaudio, std::ios::binary | std::ios::out);

    while (!ai->eof) {
        float in_pos = (float)ai->info.currentsample / (float)ai->info.nsamples;
        if (firstbuf) {
            readsize = stretchl->get_nsamples_for_fill();
            firstbuf = false;
        } else {
            readsize = stretchl->get_nsamples(0);
        };
        int readed = 0;
        if (readsize != 0) {
            readed = ai->read(readsize, inbuf_i);
        }

        cout << "readed(" << readed << ") " << flush;
        cout << "readsize(" << readsize << ") " << flush;

        for (int i = 0; i < readed; i++) {
            inbuf.l[i] = inbuf_i[i * 2] / 32768.0;
            inbuf.r[i] = inbuf_i[i * 2 + 1] / 32768.0;
        };
        float onset_l = stretchl->process(inbuf.l, readed);
        float onset_r = stretchr->process(inbuf.r, readed);

#ifdef USE_EXTRAS
        float onset = (onset_l > onset_r) ? onset_l : onset_r;
        stretchl->here_is_onset(onset);
        stretchr->here_is_onset(onset);
        bb.process(stretchl->out_buf, stretchr->out_buf, outbufsize,
                   in_pos * 100.0);

        int nskip = stretchl->get_skip_nsamples();
        if (nskip > 0)
            ai->skip(nskip);
        cout << "nskip(" << nskip << ") " << flush;
#endif

        cout << "outbufsize(" << outbufsize << ") " << flush;
        cout << "in_pos(" << in_pos << ") " << flush;

        for (int i = 0; i < outbufsize; i++) {
            float l = stretchl->out_buf[i];
            float r = stretchr->out_buf[i];
            if (l < -1.0)
                l = -1.0;
            else if (l > 1.0)
                l = 1.0;
            if (r < -1.0)
                r = -1.0;
            else if (r > 1.0)
                r = 1.0;
            outbuf[i * 2] = (int)(l * 32767.0 * 65536.0);
            outbuf[i * 2 + 1] = (int)(r * 32767.0 * 65536.0);
        };
        
        cout << "<end of frame>" << endl;
        outfile.write(reinterpret_cast<char *>(outbuf),
                      outbufsize * sizeof(int) * 2);
    }
    outfile.close();

    delete stretchl;
    delete stretchr;
    delete[] outbuf;
    delete[] inbuf_i;
    delete[] inbuf.l;
    delete[] inbuf.r;

    return "OK";
}

#ifdef RENDER_LIKE_AUD
string Render_like_Aud(string inaudio, string outaudio) {
    InputS *ai = new MP3InputS;
    if (!ai->open(inaudio)) {
        return "Error: Could not open audio file (or file format not "
               "recognized): " +
               inaudio;
    }
    std::ofstream outfile(outaudio, std::ios::binary | std::ios::out);


    // from audacity/src/effects/Paulstretch.cpp:347 ff
      // This encloses all the allocations of buffers, including those in
      // the constructor of the PaulStretch object

    int inbufsize = process.bufsize;
    short int *inbuf_i = new short int[inbufsize * 8];

    //   PaulStretch stretch(amount, stretch_buf_size, rate);
    ProcessedStretch *stretch = new ProcessedStretch(process.stretch, inbufsize, window_type, false, ai->info.samplerate, 1);
    ProcessedStretch *stretchr = new ProcessedStretch(process.stretch, inbufsize, window_type, false, ai->info.samplerate, 2);
    stretch->set_parameters(&ppar);
    stretchr->set_parameters(&ppar);

    int outbufsize = stretch->get_bufsize();
    int *outbuf = new int[outbufsize * 2];
    struct {
        float *l, *r;
    } inbuf;

    auto nget = stretch->get_nsamples_for_fill();
    // auto bufsize = stretch->get_max_bufsize();
    // inbuf.l == buffer0
    // bufsize == poolsize
    // Floats buffer0{ bufsize };
    // float *bufferptr0 = buffer0.get();
    int poolsize = stretch->get_max_bufsize();
    inbuf.l = new float[poolsize];
    inbuf.r = new float[poolsize];
    for (int i = 0; i < poolsize; i++) {
        inbuf.l[i] = inbuf.r[i] = 0.0;
    }

    bool first_time = true;
    //   bool cancelled = false;

    const auto fade_len = std::min<size_t>(100, poolsize / 2 - 1);
    float *fade_track_smps = new float[fade_len];
    int start = 0;
    int s = 0;
    int len = ai->info.nsamples;
    cout << "len(" << len << ") " << flush;


        //  Floats fade_track_smps{ fade_len };
        //  decltype(len) s = 0;

    while (s < len) {
        // track.GetFloats(inbuf.l, start + s, nget);
        ai->seek(start + s);
        ai->read(nget, inbuf_i);
        cout << "start + s(" << (s) << ") " << flush;

        for (int i = 0; i < nget; i++) {
            inbuf.l[i] = inbuf_i[i * 2] / 32768.0;
            inbuf.r[i] = inbuf_i[i * 2 + 1] / 32768.0;
        }

        stretch->process(inbuf.l, nget);
        stretchr->process(inbuf.r, nget);

        if (first_time) {
            stretch->process(inbuf.l, 0);
            stretchr->process(inbuf.r, 0);
        };

        s += nget;

        if (first_time){//blend the start of the selection
            // track.GetFloats(fade_track_smps, start, fade_len);
            
            first_time = false;
            for (size_t i = 0; i < fade_len; i++){
                float fi = (float)i / (float)fade_len;
                stretch->out_buf[i] =
                    stretch->out_buf[i] * fi + (1.0 - fi) * 
                    fade_track_smps[i];
            }
        }
        if (s >= len){//blend the end of the selection
            // track.GetFloats(fade_track_smps, end - fade_len, fade_len);
            for (size_t i = 0; i < fade_len; i++){
                float fi = (float)i / (float)fade_len;
                auto i2 = poolsize / 2 - 1 - i;
                stretch->out_buf[i2] =
                    stretch->out_buf[i2] * fi + (1.0 - fi) *
                    fade_track_smps[fade_len - 1 - i];
            }
        }

        // outputTrack.Append((samplePtr)stretch.out_buf.get(), floatSample, stretch.out_bufsize);
        for (int i = 0; i < outbufsize; i++) {
            float l = stretch->out_buf[i];
            float r = stretchr->out_buf[i];
            if (l < -1.0)
                l = -1.0;
            else if (l > 1.0)
                l = 1.0;
            if (r < -1.0)
                r = -1.0;
            else if (r > 1.0)
                r = 1.0;
            outbuf[i * 2] = (int)(l * 32767.0 * 65536.0);
            outbuf[i * 2 + 1] = (int)(r * 32767.0 * 65536.0);
        }
        outfile.write(reinterpret_cast<char *>(outbuf), outbufsize * sizeof(int) * 2);
        cout << "<end of frame>" << endl;

        nget = stretch->get_nsamples(0);
        // if (TrackProgress(count,
        //    s.as_double() / len.as_double()
        // )) {
        //    cancelled = true;
        //    break;
        // }
    }
    delete stretch;
    delete stretchr;
    delete[] outbuf;
    delete[] inbuf_i;
    delete[] inbuf.l;
    delete[] inbuf.r;

    return "OK";
}
#endif

string Render_to_End(string inaudio, string outaudio) {
    InputS *ai = new MP3InputS;
    if (!ai->open(inaudio)) {
        return "Error: Could not open audio file (or file format not "
               "recognized): " +
               inaudio;
    }

    int inbufsize = process.bufsize;
    if (inbufsize < 32)
        inbufsize = 32;
    short int *inbuf_i = new short int[inbufsize * 8];
    int outbufsize;
    struct {
        float *l, *r;
    } inbuf;
    ProcessedStretch *stretchl = new ProcessedStretch(
        process.stretch, inbufsize, window_type, false, ai->info.samplerate, 1);
    ProcessedStretch *stretchr = new ProcessedStretch(
        process.stretch, inbufsize, window_type, false, ai->info.samplerate, 2);
    stretchl->set_onset_detection_sensitivity(
        process.onset_detection_sensitivity);
    stretchr->set_onset_detection_sensitivity(
        process.onset_detection_sensitivity);
    stretchl->set_parameters(&ppar);
    stretchr->set_parameters(&ppar);

    outbufsize = stretchl->get_bufsize();
    int *outbuf = new int[outbufsize * 2];

    int poolsize = stretchl->get_max_bufsize();
    inbuf.l = new float[poolsize];
    inbuf.r = new float[poolsize];
    for (int i = 0; i < poolsize; i++)
        inbuf.l[i] = inbuf.r[i] = 0.0;

    int readsize = 0;
    bool firstbuf = true;
    bool eof_reached = false; // Track EOF

    // 32bit PCM, little-endian, stereo, 44100 Hz
    std::ofstream outfile(outaudio, std::ios::binary | std::ios::out);
    cout << "number of source samples: " << ai->info.nsamples << endl;

    while (!eof_reached || readsize > 0) {
        if (firstbuf) {
            readsize = stretchl->get_nsamples_for_fill();
            firstbuf = false;
        } else {
            readsize = stretchl->get_nsamples(0);
        }
        cout << "readsize(" << readsize << ") " << flush;

        int readed = 0;
        if (readsize != 0) {
            readed = ai->read(readsize, inbuf_i);
        }
        cout << "readed(" << readed << ") " << flush;

        if (ai->eof) {
            eof_reached = true;
            // readsize = readed;
            cout << "EOF continue processing" << endl;
        }

        for (int i = 0; i < readed; i++) {
            inbuf.l[i] = inbuf_i[i * 2] / 32768.0;
            inbuf.r[i] = inbuf_i[i * 2 + 1] / 32768.0;
        }
        float onset_l = stretchl->process(inbuf.l, readed);
        float onset_r = stretchr->process(inbuf.r, readed);

        for (int i = 0; i < outbufsize; i++) {
            float l = stretchl->out_buf[i];
            float r = stretchr->out_buf[i];
            if (l < -1.0)
                l = -1.0;
            else if (l > 1.0)
                l = 1.0;
            if (r < -1.0)
                r = -1.0;
            else if (r > 1.0)
                r = 1.0;
            outbuf[i * 2] = (int)(l * 32767.0 * 65536.0);
            outbuf[i * 2 + 1] = (int)(r * 32767.0 * 65536.0);
        }
        outfile.write(reinterpret_cast<char *>(outbuf),
                      outbufsize * sizeof(int) * 2);
        cout << "<end of frame>" << endl;
    }
    outfile.close();
    if (ai->eof) {
        cout << "<end of file>" << endl;
    }

    delete stretchl;
    delete stretchr;
    delete[] outbuf;
    delete[] inbuf_i;
    delete[] inbuf.l;
    delete[] inbuf.r;

    return "OK";
}

// M_PaulStretch(8, 44100*0.25, 44100)
class M_PaulStretch {
public:
    M_PaulStretch(int stretch, int buffer_size, int samplerate) : 
                pInputBufferSize(buffer_size), 
                pStretch(stretch),
                pSampleRate(samplerate) {
        stretcher = new ProcessedStretch( stretch, 
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

    ~M_PaulStretch() {
        delete stretcher;
        delete pInputBuffer;
    }

    void fill_input_buffer(float* input_buffer, int number_of_samples) {
        for (int i = 0; i < number_of_samples; i++) {
            pInputBuffer[i] = input_buffer[i];
        }
    }

    float *get_input_buffer() {
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
        } while(pRequiredSamples == 0);
        return mSamples;
    }

    int get_required_samples() {
        return pRequiredSamples;
    }

private:
    ProcessedStretch *stretcher;
    ProcessParameters pParameters;
    int pInputBufferSize;
    int pStretch;
    int pSampleRate;
    int outbufsize;
    float* pInputBuffer;
    int pRequiredSamples;
};

string Render_Self(string inaudio, string outaudio) {
    InputS *ai = new MP3InputS;
    if (!ai->open(inaudio)) {
        return "Error: Could not open audio file (or file format not "
               "recognized): " +
               inaudio;
    }

    // 32bit PCM, little-endian, mono, 44100 Hz
    std::ofstream outfile(outaudio, std::ios::binary | std::ios::out);

    int mBufferSize = 44100*0.25;
    short int *mReadBuffer = new short int[mBufferSize * 8];
    M_PaulStretch stretch(8, mBufferSize, 44100);
    while (!ai->eof) {
        int mNumRequestedSamples = stretch.get_required_samples();
        cout << "get_required_samples: " << mNumRequestedSamples << endl;
        int mNumReadSamples = ai->read(mNumRequestedSamples, mReadBuffer);
        // float* mInputBuffer = new float[mNumReadSamples];
        // for (int i = 0; i < mNumReadSamples; i++) {
        //     mInputBuffer[i] = mReadBuffer[i * 2] / 32768.0;
        //    stretch.get_input_buffer()[i] = mReadBuffer[i * 2] / 32768.0;
        // }
        // stretch.fill_input_buffer(mInputBuffer, mNumRequestedSamples);
        // delete[] mInputBuffer;
        // vector<float> mSamples = stretch.process();
        for (int i = 0; i < mNumReadSamples; i++) {
            stretch.get_input_buffer()[i] = mReadBuffer[i * 2] / 32768.0;
        }
        vector<float> mSamples = stretch.process();

        cout << "processed samples: " << mSamples.size() << endl;
        int *outbuf = new int[mSamples.size()];
        for (int i = 0; i < mSamples.size(); i++) {
            float l = mSamples[i];
            if (l < -1.0)
                l = -1.0;
            else if (l > 1.0)
                l = 1.0;
            outbuf[i] = (int)(l * 32767.0 * 65536.0);
        };
        outfile.write(reinterpret_cast<char *>(outbuf), mSamples.size() * sizeof(int));
    }
    outfile.close();

    return "OK";
}

int main(int argc, char **argv) {
    wav32bit = false;

    process.bufsize = 44100 * 0.25; // 0.25 seconds
    process.stretch = 8.0;
    process.onset_detection_sensitivity = 0.0;

    window_type = W_HANN;

    cout << "+++ paulstretch" << endl << endl;
    if (argc == 3) {
        #ifdef RENDER_LIKE_AUD
        cout << Render_Self(argv[1], argv[2]) << endl;
        #else
        cout << Render_to_End(argv[1], argv[2]) << endl;
        #endif
    }
}
