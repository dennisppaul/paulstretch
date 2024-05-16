/*
  Copyright (C) 2009 Nasca Octavian Paul
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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "ProcessedStretch.h"

ProcessedStretch::ProcessedStretch(float rap_,int in_bufsize_,FFTWindow w,bool bypass_,float samplerate_,int stereo_mode_):Stretch(rap_,in_bufsize_,w,bypass_,samplerate_,stereo_mode_){
	nfreq=bufsize;
	infreq=new float[nfreq];
	sumfreq=new float[nfreq];
	tmpfreq1=new float[nfreq];
	tmpfreq2=new float[nfreq];
	//fbfreq=new float[nfreq];
	free_filter_freqs=new float[nfreq];
	for (int i=0;i<nfreq;i++) {
		free_filter_freqs[i]=1.0;
	//	fbfreq[i]=0.0;
	};
};
ProcessedStretch::~ProcessedStretch(){
	delete [] infreq;
	delete [] sumfreq;
	delete [] tmpfreq1;
	delete [] tmpfreq2;
	delete [] free_filter_freqs;
//	delete [] fbfreq;
};

void ProcessedStretch::set_parameters(ProcessParameters *ppar){
	pars=*ppar;
	update_free_filter();
};


void ProcessedStretch::copy(float *freq1,float *freq2){
	for (int i=0;i<nfreq;i++) freq2[i]=freq1[i];
};

void ProcessedStretch::add(float *freq2,float *freq1,float a){
	for (int i=0;i<nfreq;i++) freq2[i]+=freq1[i]*a;
};

void ProcessedStretch::mul(float *freq1,float a){
	for (int i=0;i<nfreq;i++) freq1[i]*=a;
};

void ProcessedStretch::zero(float *freq1){
	for (int i=0;i<nfreq;i++) freq1[i]=0.0;
};

float ProcessedStretch::get_stretch_multiplier(float pos_percents){
	float result=1.0;
	if (pars.stretch_multiplier.get_enabled()){
		result*=pars.stretch_multiplier.get_value(pos_percents);
	};
	///float transient=pars.get_transient(pos_percents);
	///printf("\n%g\n",transient);
	///float threshold=0.05;
	///float power=1000.0;
	///transient-=threshold;
	///if (transient>0){
	///	transient*=power*(1.0+power);
	///	result/=(1.0+transient);
	///};
	///printf("tr=%g\n",result);
	return result;
};

void ProcessedStretch::process_spectrum(float *freq){
	if (pars.harmonics.enabled) {
		copy(freq,infreq);
		do_harmonics(infreq,freq);
	};

	if (pars.tonal_vs_noise.enabled){
		copy(freq,infreq);
		do_tonal_vs_noise(infreq,freq);
	};

	if (pars.freq_shift.enabled) {
		copy(freq,infreq);
		do_freq_shift(infreq,freq);
	};
	if (pars.pitch_shift.enabled) {
		copy(freq,infreq);
		do_pitch_shift(infreq,freq,pow(2.0,pars.pitch_shift.cents/1200.0));
	};
	if (pars.octave.enabled){
		copy(freq,infreq);
		do_octave(infreq,freq);
	};


	if (pars.spread.enabled){
		copy(freq,infreq);
		do_spread(infreq,freq);
	};


	if (pars.filter.enabled){
		copy(freq,infreq);
		do_filter(infreq,freq);
	};

	if (pars.free_filter.get_enabled()){
		copy(freq,infreq);
		do_free_filter(infreq,freq);
	};

	if (pars.compressor.enabled){
		copy(freq,infreq);
		do_compressor(infreq,freq);
	};

};

//void ProcessedStretch::process_output(float *smps,int nsmps){
//};


float profile(float fi, float bwi){
	float x=fi/bwi;
	x*=x;
	if (x>14.71280603) return 0.0;
	return exp(-x);///bwi;

};

void ProcessedStretch::do_harmonics(float *freq1,float *freq2){
	float freq=pars.harmonics.freq;
	float bandwidth=pars.harmonics.bandwidth;
	int nharmonics=pars.harmonics.nharmonics;

	if (freq<10.0) freq=10.0;

	float *amp=tmpfreq1;
	for (int i=0;i<nfreq;i++) amp[i]=0.0;

	for (int nh=1;nh<=nharmonics;nh++){//for each harmonic
		float bw_Hz;//bandwidth of the current harmonic measured in Hz
		float bwi;
		float fi;
		float f=nh*freq;

		if (f>=samplerate/2) break;

		bw_Hz=(pow(2.0,bandwidth/1200.0)-1.0)*f;
		bwi=bw_Hz/(2.0*samplerate);
		fi=f/samplerate;

		float sum=0.0;
		float max=0.0;
		for (int i=1;i<nfreq;i++){//todo: optimize here
			float hprofile;
			hprofile=profile((i/(float)nfreq*0.5)-fi,bwi);
			amp[i]+=hprofile;
			if (max<hprofile) max=hprofile;
			sum+=hprofile;
		};
	};

	float max=0.0;
	for (int i=1;i<nfreq;i++){
		if (amp[i]>max) max=amp[i];
	};
	if (max<1e-8) max=1e-8;

	for (int i=1;i<nfreq;i++){
		float c,s;
		float a=amp[i]/max;
		if (!pars.harmonics.gauss) a=(a<0.368?0.0:1.0);
		freq2[i]=freq1[i]*a;
	};

};


void ProcessedStretch::do_freq_shift(float *freq1,float *freq2){
	zero(freq2);
	int ifreq=(int)(pars.freq_shift.Hz/(samplerate*0.5)*nfreq);
	for (int i=0;i<nfreq;i++){
		int i2=ifreq+i;
		if ((i2>0)&&(i2<nfreq)) freq2[i2]=freq1[i];
	};
};
void ProcessedStretch::do_pitch_shift(float *freq1,float *freq2,float rap){
	zero(freq2);
	if (rap<1.0){//down
		for (int i=0;i<nfreq;i++){
			int i2=(int)(i*rap);
			if (i2>=nfreq) break;
			freq2[i2]+=freq1[i];
		};
	};
	if (rap>=1.0){//up
		rap=1.0/rap;
		for (int i=0;i<nfreq;i++){
			freq2[i]=freq1[(int)(i*rap)];
		};

	};
};
void ProcessedStretch::do_octave(float *freq1,float *freq2){
	zero(sumfreq);
	if (pars.octave.om2>1e-3){
		do_pitch_shift(freq1,tmpfreq1,0.25);
		add(sumfreq,tmpfreq1,pars.octave.om2);
	};
	if (pars.octave.om1>1e-3){
		do_pitch_shift(freq1,tmpfreq1,0.5);
		add(sumfreq,tmpfreq1,pars.octave.om1);
	};
	if (pars.octave.o0>1e-3){
		add(sumfreq,freq1,pars.octave.o0);
	};
	if (pars.octave.o1>1e-3){
		do_pitch_shift(freq1,tmpfreq1,2.0);
		add(sumfreq,tmpfreq1,pars.octave.o1);
	};
	if (pars.octave.o15>1e-3){
		do_pitch_shift(freq1,tmpfreq1,3.0);
		add(sumfreq,tmpfreq1,pars.octave.o15);
	};
	if (pars.octave.o2>1e-3){
		do_pitch_shift(freq1,tmpfreq1,4.0);
		add(sumfreq,tmpfreq1,pars.octave.o2);
	};

	float sum=0.01+pars.octave.om2+pars.octave.om1+pars.octave.o0+pars.octave.o1+pars.octave.o15+pars.octave.o2;
	if (sum<0.5) sum=0.5;
	for (int i=0;i<nfreq;i++) freq2[i]=sumfreq[i]/sum;
};

void ProcessedStretch::do_filter(float *freq1,float *freq2){
	float low=0,high=0;
	if (pars.filter.low<pars.filter.high){//sort the low/high freqs
		low=pars.filter.low;
		high=pars.filter.high;
	}else{
		high=pars.filter.low;
		low=pars.filter.high;
	};
	int ilow=(int) (low/samplerate*nfreq*2.0);
	int ihigh=(int) (high/samplerate*nfreq*2.0);
	float dmp=1.0;
	float dmprap=1.0-pow(pars.filter.hdamp*0.5,4.0);
	for (int i=0;i<nfreq;i++){
		float a=0.0;
		if ((i>=ilow)&&(i<ihigh)) a=1.0;
		if (pars.filter.stop) a=1.0-a;
		freq2[i]=freq1[i]*a*dmp;
		dmp*=dmprap+1e-8;
	};
};

void ProcessedStretch::update_free_filter(){
	pars.free_filter.update_curve();
	if (pars.free_filter.get_enabled()) {
		for (int i=0;i<nfreq;i++){
			float freq=(float)i/(float) nfreq*samplerate*0.5;
			free_filter_freqs[i]=pars.free_filter.get_value(freq);
		};
	}else{
		for (int i=0;i<nfreq;i++){
			free_filter_freqs[i]=1.0;
		};
	};
};
void ProcessedStretch::do_free_filter(float *freq1,float *freq2){
	for (int i=0;i<nfreq;i++){
		freq2[i]=freq1[i]*free_filter_freqs[i];
	};
};

void ProcessedStretch::do_spread(float *freq1,float *freq2){
	spread(freq1,freq2,pars.spread.bandwidth);
};

void ProcessedStretch::spread(float *freq1,float *freq2,float spread_bandwidth){
	//convert to log spectrum
	float minfreq=20.0;
	float maxfreq=0.5*samplerate;

	float log_minfreq=log(minfreq);
	float log_maxfreq=log(maxfreq);

	for (int i=0;i<nfreq;i++){
		float freqx=i/(float) nfreq;
		float x=exp(log_minfreq+freqx*(log_maxfreq-log_minfreq))/maxfreq*nfreq;
		float y=0.0;
		int x0=(int)floor(x); if (x0>=nfreq) x0=nfreq-1;
		int x1=x0+1; if (x1>=nfreq) x1=nfreq-1;
		float xp=x-x0;
		if (x<nfreq){
			y=freq1[x0]*(1.0-xp)+freq1[x1]*xp;
		};
		tmpfreq1[i]=y;
	};

	//increase the bandwidth of each harmonic (by smoothing the log spectrum)
	int n=2;
	float bandwidth=spread_bandwidth;
	float a=1.0-pow(2.0,-bandwidth*bandwidth*10.0);
	a=pow(a,8192.0/nfreq*n);

	for (int k=0;k<n;k++){
		tmpfreq1[0]=0.0;
		for (int i=1;i<nfreq;i++){
			tmpfreq1[i]=tmpfreq1[i-1]*a+tmpfreq1[i]*(1.0-a);
		};
		tmpfreq1[nfreq-1]=0.0;
		for (int i=nfreq-2;i>0;i--){
			tmpfreq1[i]=tmpfreq1[i+1]*a+tmpfreq1[i]*(1.0-a);
		};
	};

	freq2[0]=0;
	float log_maxfreq_d_minfreq=log(maxfreq/minfreq);
	for (int i=1;i<nfreq;i++){
		float freqx=i/(float) nfreq;
		float x=log((freqx*maxfreq)/minfreq)/log_maxfreq_d_minfreq*nfreq;
		float y=0.0;
		if ((x>0.0)&&(x<nfreq)){
			int x0=(int)floor(x); if (x0>=nfreq) x0=nfreq-1;
			int x1=x0+1; if (x1>=nfreq) x1=nfreq-1;
			float xp=x-x0;
			y=tmpfreq1[x0]*(1.0-xp)+tmpfreq1[x1]*xp;
		};
		freq2[i]=y;
	};


};


void ProcessedStretch::do_compressor(float *freq1,float *freq2){
	float rms=0.0;
	for (int i=0;i<nfreq;i++) rms+=freq1[i]*freq1[i];
	rms=sqrt(rms/nfreq)*0.1;
	if (rms<1e-3) rms=1e-3;

	float rap=pow(rms,-pars.compressor.power);
	for (int i=0;i<nfreq;i++) freq2[i]=freq1[i]*rap;
};

void ProcessedStretch::do_tonal_vs_noise(float *freq1,float *freq2){
	spread(freq1,tmpfreq1,pars.tonal_vs_noise.bandwidth);

	if (pars.tonal_vs_noise.preserve>=0.0){
		float mul=(pow(10.0,pars.tonal_vs_noise.preserve)-1.0);
		for (int i=0;i<nfreq;i++) {
			float x=freq1[i];
			float smooth_x=tmpfreq1[i]+1e-6;

			float result=0.0;
			result=x-smooth_x*mul;
			if (result<0.0) result=0.0;
			freq2[i]=result;
		};
	}else{
		float mul=(pow(5.0,1.0+pars.tonal_vs_noise.preserve)-1.0);
		for (int i=0;i<nfreq;i++) {
			float x=freq1[i];
			float smooth_x=tmpfreq1[i]+1e-6;

			float result=0.0;
			result=x-smooth_x*mul+0.1*mul;
			if (result<0.0) result=x;
			else result=0.0;

			freq2[i]=result;
		};
	};

};
