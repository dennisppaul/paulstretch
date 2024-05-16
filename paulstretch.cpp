#include <iostream>
#include <fstream>

#include "globals.h"
#include "BinauralBeats.h"
#include "ProcessedStretch.h"

#include "Input/MP3InputS.h"
// #include "Output/VorbisOutputS.h"

struct {
    int bufsize;
    double stretch;
    double onset_detection_sensitivity;
}process;

struct {
    REALTYPE render_percent;
    bool cancel_render;
}info;

ProcessParameters ppar;
BinauralBeatsParameters	bbpar;
FFTWindow window_type;
bool wav32bit;
REALTYPE volume;
// const int samplerate = 480000;
// FILE_TYPE outtype    = FILE_WAV;

using namespace std;

string Render(string inaudio,string outaudio,FILE_TYPE outtype,FILE_TYPE intype,REALTYPE pos1,REALTYPE pos2){
	if (pos2<pos1){
		REALTYPE tmp=pos2;
		pos2=pos1;
		pos1=tmp;
	};
	InputS *ai=NULL;
	switch(intype){
// 		case FILE_VORBIS:ai=new VorbisInputS;
// 						 break;
		case FILE_MP3:ai=new MP3InputS;
					  break;
// 		default:ai=new AInputS;
	};
// 	AOutputS ao;
// 	VorbisOutputS vorbisout;
	info.cancel_render=false;
	if (!ai->open(inaudio)){
		return "Error: Could not open audio file (or file format not recognized) :"+inaudio;
	};
	BinauralBeats bb(ai->info.samplerate);
	bb.pars=bbpar;
// 	if (outtype==FILE_WAV) ao.newfile(outaudio,ai->info.samplerate,wav32bit);
// 	if (outtype==FILE_VORBIS) vorbisout.newfile(outaudio,ai->info.samplerate);

	ai->seek(pos1);

	int inbufsize=process.bufsize;

	if (inbufsize<32) inbufsize=32;
	short int *inbuf_i=new short int[inbufsize*8];
	int outbufsize;
	struct{
		REALTYPE *l,*r;
	}inbuf;
	ProcessedStretch *stretchl=new ProcessedStretch(process.stretch,inbufsize,window_type,false,ai->info.samplerate,1);
	ProcessedStretch *stretchr=new ProcessedStretch(process.stretch,inbufsize,window_type,false,ai->info.samplerate,2);
	stretchl->set_onset_detection_sensitivity(process.onset_detection_sensitivity);
	stretchr->set_onset_detection_sensitivity(process.onset_detection_sensitivity);
	stretchl->set_parameters(&ppar);
	stretchr->set_parameters(&ppar);

	outbufsize=stretchl->get_bufsize();
	int *outbuf=new int[outbufsize*2];

	int poolsize=stretchl->get_max_bufsize();

	inbuf.l=new REALTYPE[poolsize];
	inbuf.r=new REALTYPE[poolsize];
	for (int i=0;i<poolsize;i++) inbuf.l[i]=inbuf.r[i]=0.0;

	int readsize=0;
	const int pause_max_write=65536;
	int pause_write=0;
	bool firstbuf=true;
	
    std::ofstream outfile(outaudio, std::ios::binary | std::ios::out);
	
	while(!ai->eof){
		float in_pos=(REALTYPE) ai->info.currentsample/(REALTYPE)ai->info.nsamples;
		if (firstbuf){
			readsize=stretchl->get_nsamples_for_fill();
			firstbuf=false;
		}else{
			readsize=stretchl->get_nsamples(in_pos*100.0);
		};
		int readed=0;
		if (readsize!=0) readed=ai->read(readsize,inbuf_i);
		
		for (int i=0;i<readed;i++) {
			inbuf.l[i]=inbuf_i[i*2]/32768.0;
			inbuf.r[i]=inbuf_i[i*2+1]/32768.0;
		};
		REALTYPE onset_l=stretchl->process(inbuf.l,readed);
		REALTYPE onset_r=stretchr->process(inbuf.r,readed);
		REALTYPE onset=(onset_l>onset_r)?onset_l:onset_r;
		stretchl->here_is_onset(onset);
		stretchr->here_is_onset(onset);
		bb.process(stretchl->out_buf,stretchr->out_buf,outbufsize,in_pos*100.0);
		for (int i=0;i<outbufsize;i++) {
			stretchl->out_buf[i]*=volume;
			stretchr->out_buf[i]*=volume;
		};
		int nskip=stretchl->get_skip_nsamples();
		if (nskip>0) ai->skip(nskip);

		if (outtype==FILE_WAV){	
			for (int i=0;i<outbufsize;i++) {
				REALTYPE l=stretchl->out_buf[i],r=stretchr->out_buf[i];
				if (l<-1.0) l=-1.0;
				else if (l>1.0) l=1.0;
				if (r<-1.0) r=-1.0;
				else if (r>1.0) r=1.0;
				outbuf[i*2]=(int)(l*32767.0*65536.0);
				outbuf[i*2+1]=(int)(r*32767.0*65536.0);
			};
			cout << "." << flush;
// 			ao.write(outbufsize,outbuf);
            outfile.write(reinterpret_cast<char*>(outbuf), outbufsize * sizeof(int) * 2);
		};
// 		if (outtype==FILE_VORBIS) vorbisout.write(outbufsize,stretchl->out_buf,stretchr->out_buf);

		REALTYPE totalf=ai->info.currentsample/(REALTYPE)ai->info.nsamples-pos1;
		if (totalf>(pos2-pos1)) break;

		info.render_percent=(totalf*100.0/(pos2-pos1+0.001));
// 		cout << "> " << (int)(info.render_percent*10000) << endl;
// 		pause_write+=outbufsize;
// 		if (pause_write>pause_max_write){
// 			float tmp=outbufsize/1000000.0;
// 			if (tmp>0.1) tmp=0.1;
// // 			Fl::wait(0.01+tmp);
// 			pause_write=0;
// 			if (info.cancel_render) break;
// 		};
	};
    outfile.close();

	delete stretchl;
	delete stretchr;    
	delete []outbuf;
	delete []inbuf_i;
	delete []inbuf.l;
	delete []inbuf.r;

	info.render_percent=-1.0;
	return "";
};

int main(int argc, char **argv) {
// 	wavinfo.samplerate=44100;
// 	wavinfo.nsamples=0;
// 	wavinfo.intype=FILE_WAV;
	wav32bit=false;

	process.bufsize=16384 / 32;
	process.stretch=8.0;
	process.onset_detection_sensitivity=0.0;

// 	seek_pos=0.0;
	window_type=W_HANN;
	info.render_percent=-1.0;
	info.cancel_render=false;
	volume=1.0;
	
	cout << "paulstretch" << endl;
    cout << Render("input.mp3", "output.raw", FILE_WAV, FILE_MP3, 0, 65536) << endl;
}
