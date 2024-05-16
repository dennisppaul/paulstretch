/*
  Copyright (C) 2006-2011 Nasca Octavian Paul
  Author: Nasca Octavian Paul

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License
  as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License (version 2) for more details.

  You should have received a copy of the GNU General Public License (version 2)
  along with this program; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/
#ifndef STRETCH_H
#define STRETCH_H
#include "globals.h"

#ifdef KISSFFT
#include <kiss_fftr.h>
#else
#include <fftw3.h>
#endif


enum FFTWindow{W_RECTANGULAR,W_HAMMING,W_HANN,W_BLACKMAN,W_BLACKMAN_HARRIS};

class FFT{//FFT class that considers phases as random
	public:
		FFT(int nsamples_);//samples must be even
		~FFT();
		void smp2freq();//input is smp, output is freq (phases are discarded)
		void freq2smp();//input is freq,output is smp (phases are random)
		void applywindow(FFTWindow type);
		float *smp;//size of samples/2
		float *freq;//size of samples
		int nsamples;
	private:
#ifdef KISSFFT
		kiss_fftr_cfg  plankfft,plankifft;
		kiss_fft_scalar *datar;
		kiss_fft_cpx *datac;
#else
		fftwf_plan planfftw,planifftw;
		float *data;
#endif
		struct{
			float *data;
			FFTWindow type;
		}window;

		unsigned int rand_seed;
		static unsigned int start_rand_seed;
};

class Stretch{
	public:
		Stretch(float rap_,int in_bufsize_,FFTWindow w=W_HAMMING,bool bypass_=false,float samplerate_=44100,int stereo_mode_=0);
		//in_bufsize is also a half of a FFT buffer (in samples)
		virtual ~Stretch();

		int get_max_bufsize(){
			return bufsize*3;
		};
		int get_bufsize(){
			return bufsize;
		};

		float get_onset_detection_sensitivity(){
			return onset_detection_sensitivity;
		};

		float process(float *smps,int nsmps);//returns the onset value
		void set_freezing(bool new_freezing){
			freezing=new_freezing;
		};


		float *out_buf;//pot sa pun o variabila "max_out_bufsize" si asta sa fie marimea lui out_buf si pe out_bufsize sa il folosesc ca marime adaptiva

		int get_nsamples(float current_pos_percents);//how many samples are required
		int get_nsamples_for_fill();//how many samples are required to be added for a complete buffer refill (at start of the song or after seek)
		int get_skip_nsamples();//used for shorten

		void set_rap(float newrap);//set the current stretch value

		void set_onset_detection_sensitivity(float detection_sensitivity){
			onset_detection_sensitivity=detection_sensitivity;
			if (detection_sensitivity<1e-3) extra_onset_time_credit=0.0;
		};
		void here_is_onset(float onset);

		FFTWindow window_type;
	protected:
		int bufsize;

		virtual void process_spectrum(float *freq){};
		virtual float get_stretch_multiplier(float pos_percents);
		float samplerate;
		int stereo_mode;//0=mono,1=left,2=right
	private:

		void do_analyse_inbuf(float *smps);
		void do_next_inbuf_smps(float *smps);
		float do_detect_onset();

//		float *in_pool;//de marimea in_bufsize
		float rap,onset_detection_sensitivity;
		float *old_out_smps;
		float *old_freq;
		float *new_smps,*old_smps,*very_old_smps;

		FFT *infft,*outfft;
		FFT *fft;
		long double remained_samples;//0..1
		long double extra_onset_time_credit;
		float c_pos_percents;
		int skip_samples;
		bool require_new_buffer;
		bool bypass,freezing;
};


#endif