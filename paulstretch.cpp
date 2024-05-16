#include <iostream>
#include <fstream>

#include "globals.h"
#include "BinauralBeats.h"
#include "ProcessedStretch.h"

#include "Input/MP3InputS.h"

struct {
    int bufsize;
    double stretch;
    double onset_detection_sensitivity;
}process;

ProcessParameters ppar;
BinauralBeatsParameters    bbpar;
FFTWindow window_type;
bool wav32bit;

using namespace std;

string Render(string inaudio,string outaudio) {
    InputS *ai = new MP3InputS;
    if (!ai->open(inaudio)){
        return "Error: Could not open audio file (or file format not recognized) :"+inaudio;
    };
    BinauralBeats bb(ai->info.samplerate);
    bb.pars=bbpar;

    int inbufsize=process.bufsize;

    if (inbufsize<32) inbufsize=32;
    short int *inbuf_i=new short int[inbufsize*8];
    int outbufsize;
    struct{
        float *l,*r;
    }inbuf;
    ProcessedStretch *stretchl=new ProcessedStretch(process.stretch,inbufsize,window_type,false,ai->info.samplerate,1);
    ProcessedStretch *stretchr=new ProcessedStretch(process.stretch,inbufsize,window_type,false,ai->info.samplerate,2);
    stretchl->set_onset_detection_sensitivity(process.onset_detection_sensitivity);
    stretchr->set_onset_detection_sensitivity(process.onset_detection_sensitivity);
    stretchl->set_parameters(&ppar);
    stretchr->set_parameters(&ppar);

    outbufsize=stretchl->get_bufsize();
    int *outbuf=new int[outbufsize*2];
    cout << "outbufsize(" << outbufsize << ") " << endl;

    int poolsize=stretchl->get_max_bufsize();
    cout << "poolsize(" << poolsize << ") " << endl;

    inbuf.l=new float[poolsize];
    inbuf.r=new float[poolsize];
    for (int i=0;i<poolsize;i++) inbuf.l[i]=inbuf.r[i]=0.0;

    int readsize=0;
    const int pause_max_write=65536;
    int pause_write=0;
    bool firstbuf=true;

    // 32bit PCM, little-endian, stereo, 44100 Hz
    std::ofstream outfile(outaudio, std::ios::binary | std::ios::out);

    while(!ai->eof){
        float in_pos=(float) ai->info.currentsample/(float)ai->info.nsamples;
        if (firstbuf){
            readsize=stretchl->get_nsamples_for_fill();
            firstbuf=false;
        }else{
            readsize=stretchl->get_nsamples(in_pos*100.0);
        };
        int readed=0;
        if (readsize!=0) readed=ai->read(readsize,inbuf_i);

        cout << "readed(" << readed << ") " << flush;
        cout << "readsize(" << readsize << ") " << flush;

        for (int i=0;i<readed;i++) {
            inbuf.l[i]=inbuf_i[i*2]/32768.0;
            inbuf.r[i]=inbuf_i[i*2+1]/32768.0;
        };
        float onset_l=stretchl->process(inbuf.l,readed);
        float onset_r=stretchr->process(inbuf.r,readed);
        float onset=(onset_l>onset_r)?onset_l:onset_r;
        stretchl->here_is_onset(onset);
        stretchr->here_is_onset(onset);
        bb.process(stretchl->out_buf,stretchr->out_buf,outbufsize,in_pos*100.0);

        int nskip=stretchl->get_skip_nsamples();
        if (nskip>0) ai->skip(nskip);
        cout << "nskip(" << nskip << ") " << flush;
        cout << "outbufsize(" << outbufsize << ") " << flush;
        cout << "in_pos(" << in_pos << ") " << flush;

        for (int i=0;i<outbufsize;i++) {
            float l=stretchl->out_buf[i];
            float r=stretchr->out_buf[i];
            if (l<-1.0) l=-1.0;
            else if (l>1.0) l=1.0;
            if (r<-1.0) r=-1.0;
            else if (r>1.0) r=1.0;
            outbuf[i*2]=(int)(l*32767.0*65536.0);
            outbuf[i*2+1]=(int)(r*32767.0*65536.0);
        };
        cout << "<end of frame>" << endl;
        outfile.write(reinterpret_cast<char*>(outbuf), outbufsize * sizeof(int) * 2);
    }
    outfile.close();

    delete stretchl;
    delete stretchr;
    delete []outbuf;
    delete []inbuf_i;
    delete []inbuf.l;
    delete []inbuf.r;

    return "OK";
}

int main(int argc, char **argv) {
    wav32bit=false;

    process.bufsize=16384;// / 32;
    process.stretch=8.0;
    process.onset_detection_sensitivity=0.0;

    window_type=W_HANN;

    cout << "+++ paulstretch" << endl << endl;
    cout << Render("../test/input.mp3", "../test/output.raw") << endl;
}
