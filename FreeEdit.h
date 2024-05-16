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
#ifndef FREEEDIT_H
#define FREEEDIT_H

#define FREE_EDIT_MAX_POINTS 50
#include <math.h>
#include <stdio.h>

#include "globals.h"

#define LOG_2 0.693147181
#define LOG_10 2.302585093

#define dB2rap(dB) ((exp((dB)*LOG_10/20.0)))
#define rap2dB(rap) ((20*log(rap)/LOG_10))

struct FreeEditPos{
	float x,y;
	bool enabled;
};
enum FREE_EDIT_EXTREME_SCALE{
	FE_LINEAR=0,
	FE_LOG=1,
	FE_DB=2
};
class FreeEditExtremes{
	public:
		FreeEditExtremes(){
			init();
		};
		void init(float min_=0.0,float max_=1.0,FREE_EDIT_EXTREME_SCALE scale_=FE_LINEAR,bool lock_min_max_=false,bool lock_scale_=false){
			min=min_;
			max=max_;
			scale=scale_;
			lock_min_max=lock_min_max_;
			lock_scale=lock_scale_;
			correct_values();
		};

		//converting functions
		inline float coord_to_real_value(float coord){//coord = 0.0 .. 1.0
			float result;
			switch(scale){
				case FE_LOG://log
					result=exp(log(min)+coord*(log(max)-log(min)));
					return result;
				default://linear or dB
					result=min+coord*(max-min);
					return result;
			};
		};

		inline float real_value_to_coord(float val){//coord = 0.0 .. 1.0
			switch(scale){
				case FE_LOG://log
					{
						float coord=log(val/min)/log(max/min);
						clamp1(coord);
						return coord;
					};
				default://linear
					{
						float diff=max-min;
						float coord;
						if (fabs(diff)>0.0001) coord=(val-min)/diff;
						else coord=min;
						clamp1(coord);
						return coord;
					};
			};
		};

		//max and min functions
		void set_min(float val){
			if (lock_min_max) return;
			min=val;
			correct_values();
		};
		float get_min(){
			return min;
		};
		void set_max(float val){
			if (lock_min_max) return;
			max=val;
			correct_values();
		};
		float get_max(){
			return max;
		};
		//scale functions
		FREE_EDIT_EXTREME_SCALE get_scale(){
			return scale;
		};
		void set_scale(FREE_EDIT_EXTREME_SCALE val){
			if (lock_scale) return;
			scale=val;
		};

	private:
		inline float clamp1(float m){
			if (m<0.0) return 0.0;
			if (m>1.0) return 1.0;
			return m;
		};
		void correct_values(){
			if (scale!=FE_LOG) return;
			if (min<1e-8) min=1e-8;
			if (max<1e-8) max=1e-8;
		};
		bool lock_min_max,lock_scale;
		float min,max;
		FREE_EDIT_EXTREME_SCALE scale;
};
class FreeEdit{
	public:
		enum INTERP_MODE{
			LINEAR=0,
			COSINE=1
		};
		FreeEdit();
		FreeEdit (const FreeEdit &other);
		const FreeEdit &operator=(const FreeEdit &other);
		void deep_copy_from(const FreeEdit &other);

		//Enabled functions
		bool get_enabled(){
			return enabled;
		};
		void set_enabled(bool val){
			enabled=val;
		};

		inline int get_npoints(){
			return npos;
		};

		//manipulation functions
		inline bool is_enabled(int n){
			if ((n<0)||(n>=npos)) return false;
			return pos[n].enabled;
		};
		inline void set_enabled(int n,bool enabled){
			if ((n<0)||(n>=npos)) return;
			pos[n].enabled=enabled;
		};


		inline float get_posx(int n){
			if ((n<0)||(n>=npos)) return 0.0;
			return pos[n].x;
		};
		inline float get_posy(int n){
			if ((n<0)||(n>=npos)) return 0.0;
			return pos[n].y;
		};
		inline void set_posx(int n,float x){
			if ((n<2)||(n>=npos)) return;//don't allow to set the x position of the first two positions
			pos[n].x=clamp1(x);
		};
		inline void set_posy(int n,float y){
			if ((n<0)||(n>=npos)) return;
			pos[n].y=clamp1(y);
		};

		void set_all_values(float val){
			for (int i=0;i<npos;i++){
				if (pos[i].enabled) pos[i].y=extreme_y.real_value_to_coord(val);
			}
		};

		//interpolation mode
		INTERP_MODE get_interp_mode(){
			return interp_mode;
		};
		void set_interp_mode(INTERP_MODE interp_mode_){
			interp_mode=interp_mode_;
		};

		//smooth
		float get_smooth(){
			return smooth;
		};
		void set_smooth(float smooth_){
			smooth=clamp1(smooth_);;
		};

		//getting the curve
		void get_curve(int datasize,float *data,bool real_values);

		~FreeEdit(){
			delete []pos;
		};

		//making/updating the curve
		void update_curve(int size=16384);
		float get_value(float x);

		//extremes
		FreeEditExtremes extreme_x,extreme_y;

		struct{
			float *data;
			int size;
		}curve;
	private:
		inline float clamp1(float m){
			if (m<0.0) return 0.0;
			if (m>1.0) return 1.0;
			return m;
		};
		inline void swap(float &m1,float &m2){
			float tmp=m1;
			m1=m2;
			m2=tmp;
		};
		FreeEditPos *pos;
		int npos;
		float smooth;
		INTERP_MODE interp_mode;
		bool enabled;
};

#endif
