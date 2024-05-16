/*
  Copyright (C) 2011 Nasca Octavian Paul
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


#ifndef PROCESSED_STRETCH_H
#define PROCESSED_STRETCH_H

#include "FreeEdit.h"
#include "Stretch.h"

struct ProcessParameters{
	ProcessParameters(){
		pitch_shift.enabled=false;
		pitch_shift.cents=0;

		octave.enabled=false;
		octave.om2=octave.om1=octave.o1=octave.o15=octave.o2=0;
		octave.o0=1.0;

		freq_shift.enabled=false;
		freq_shift.Hz=0;

		compressor.enabled=false;
		compressor.power=0.0;

		filter.enabled=false;
		filter.stop=false;
		filter.low=0.0;
		filter.high=22000.0;
		filter.hdamp=0.0;

		harmonics.enabled=false;
		harmonics.freq=440.0;
		harmonics.bandwidth=25.0;
		harmonics.nharmonics=10;
		harmonics.gauss=false;

		spread.enabled=false;
		spread.bandwidth=0.3;

		tonal_vs_noise.enabled=false;
		tonal_vs_noise.preserve=0.5;
		tonal_vs_noise.bandwidth=0.9;
	};
	~ProcessParameters(){
	};

	struct{
		bool enabled;
		int cents;
	}pitch_shift;

	struct{
		bool enabled;
		float om2,om1,o0,o1,o15,o2;
	}octave;

	struct{
		bool enabled;
		int Hz;
	}freq_shift;

	struct{
		bool enabled;
		float power;
	}compressor;

	struct{
		bool enabled;
		float low,high;
		float hdamp;
		bool stop;
	}filter;

	struct{
		bool enabled;
		float freq;
		float bandwidth;
		int nharmonics;
		bool gauss;
	}harmonics;

	struct{
		bool enabled;
		float bandwidth;
	}spread;

	struct{
		bool enabled;
		float preserve;
		float bandwidth;
	}tonal_vs_noise;

	FreeEdit free_filter;
	FreeEdit stretch_multiplier;

};

class ProcessedStretch:public Stretch{
	public:
		//stereo_mode: 0=mono,1=left,2=right
		ProcessedStretch(float rap_,int in_bufsize_,FFTWindow w=W_HAMMING,bool bypass_=false,float samplerate_=44100,int stereo_mode=0);
		~ProcessedStretch();
		void set_parameters(ProcessParameters *ppar);
	private:
		float get_stretch_multiplier(float pos_percents);
//		void process_output(float *smps,int nsmps);
		void process_spectrum(float *freq);
		void do_harmonics(float *freq1,float *freq2);
		void do_pitch_shift(float *freq1,float *freq2,float rap);
		void do_freq_shift(float *freq1,float *freq2);
		void do_octave(float *freq1,float *freq2);
		void do_filter(float *freq1,float *freq2);
		void do_free_filter(float *freq1,float *freq2);
		void do_compressor(float *freq1,float *freq2);
		void do_spread(float *freq1,float *freq2);
		void do_tonal_vs_noise(float *freq1,float *freq2);

		void copy(float *freq1,float *freq2);
		void add(float *freq2,float *freq1,float a=1.0);
		void mul(float *freq1,float a);
		void zero(float *freq1);
		void spread(float *freq1,float *freq2,float spread_bandwidth);

		void update_free_filter();
		int nfreq;

		float *free_filter_freqs;
		ProcessParameters pars;

		float *infreq,*sumfreq,*tmpfreq1,*tmpfreq2;
		//float *fbfreq;
};

#endif
