#include <assert.h>
#ifdef KISSFFT
#include <kiss_fftr.h>
#else
#include <fftw3.h>
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "paulstretch.h"

#define PI 3.14159265358979323846

struct fft {
  size_t rand;
  size_t length;
  #ifdef KISSFFT
		float *smp;//size of samples/2
		float *freq;//size of samples
		kiss_fftr_cfg plankfft;
	    kiss_fftr_cfg plankifft;
		kiss_fft_scalar *datar;
		kiss_fft_cpx *datac;
  #else
        float *data;
        fftwf_plan plan;
        fftwf_plan plani;
  #endif
  float *window_data;
};

static void fft_init(struct fft *fft, size_t length) {
  static size_t rand_seed = 1;
  rand_seed += 161103;
  fft->rand = rand_seed;
  fft->length = length;
  #ifdef KISSFFT
 	fft->smp = (float*)malloc(length * sizeof(float));;for (int i=0;i<length;i++) fft->smp[i]=0.0;
 	fft->freq = (float*)malloc((length/2+1) * sizeof(float));;for (int i=0;i<length/2+1;i++) fft->freq[i]=0.0;
	fft->datar = (kiss_fft_scalar*)malloc((length + 2) * sizeof(kiss_fft_scalar));
	for (int i=0;i<length+2;i++) {
	    fft->datar[i]=0.0;
	}
	fft->datac = (kiss_fft_cpx*)malloc((length / 2 + 2) * sizeof(kiss_fft_cpx));
	for (int i=0;i<length/2+2;i++) {
	    fft->datac[i].r=0.0;
        fft->datac[i].i=0.0;
	}
	fft->plankfft = kiss_fftr_alloc(length,0,0,0);
	fft->plankifft = kiss_fftr_alloc(length,1,0,0);
  #else
    fft->data = fftwf_alloc_real(length);
    fft->plan = fftwf_plan_r2r_1d(length, fft->data, fft->data, FFTW_R2HC, FFTW_ESTIMATE);
    fft->plani = fftwf_plan_r2r_1d(length, fft->data, fft->data, FFTW_HC2R, FFTW_ESTIMATE);
  #endif
  fft->window_data = malloc(length * sizeof(float));
  for (size_t i = 0; i < length; ++i) {
    fft->window_data[i] = 0.53836 - 0.46164 * cos(2 * PI * i / (length + 1.0)); // W_HAMMING
  }
}

static void fft_apply_window(struct fft *fft) {
  for (size_t i = 0; i < fft->length; ++i) {
      #ifdef KISSFFT
        for (int i = 0; i<fft->length; i++) {
            fft->smp[i] *= fft->window_data[i];
        }
      #else
        fft->data[i] *= fft->window_data[i];
      #endif
  }
}

static void fft_s2f(struct fft *fft) {
#ifdef KISSFFT
	for (int i = 0; i < fft->length; i++) {
	   fft->datar[i] = fft->smp[i];
	}
	kiss_fftr(fft->plankfft, fft->datar, fft->datac);
#else
    fftwf_execute(fft->plan);
#endif
    #ifdef KISSFFT
        for (size_t i = 1; i < fft->length / 2; ++i) {
            float c = fft->datac[i].r;
            float s = fft->datac[i].i;
            fft->freq[i] = sqrt(c * c + s * s);
        }
        fft->freq[0] = 0.0f;
    #else
        for (size_t i = 1; i < fft->length / 2; ++i) {
            float c = fft->data[i];
            float s = fft->data[fft->length - i];
            fft->data[i] = sqrt(c * c + s * s);
        }
        fft->data[0] = 0.0f;
    #endif
}

static void fft_f2s(struct fft *fft) {
  float inv_2p15_2pi = 1.0 / 16384.0 * PI;
  for (size_t i = 1; i < fft->length / 2; ++i) {
    fft->rand = fft->rand * 1103515245 + 12345;
    size_t rd = fft->rand & 0x7fff;
    float phase = rd * inv_2p15_2pi;
    #ifdef KISSFFT
		fft->datac[i].r = fft->freq[i] * cos(phase);
		fft->datac[i].i = fft->freq[i] * sin(phase);
    #else
        float data = fft->data[i];
        fft->data[i] = data * cos(phase);
        fft->data[fft->length - i] = data * sin(phase);
    #endif
  }
  #ifdef KISSFFT
	fft->datac[0].r = fft->datac[0].i = 0.0;
	kiss_fftri(fft->plankifft, fft->datac, fft->datar);
	for (int i = 0; i < fft->length; i++) {
	   fft->smp[i]= fft->datar[i] / fft->length;
	}
  #else
    fft->data[0] = 0.0;
    fft->data[fft->length / 2] = 0.0;

    fftwf_execute(fft->plani);
    for (size_t i = 0; i < fft->length; ++i) {
        fft->data[i] = fft->data[i] / fft->length;
    }
  #endif
}

static void fft_finish(struct fft *fft) {
#ifdef KISSFFT
    free(fft->smp);
    free(fft->freq);
    free(fft->datar);
	free(fft->datac);
	free(fft->plankfft);
	free(fft->plankifft);
#else
  fftwf_free(fft->data);
  fftwf_destroy_plan(fft->plan);
  fftwf_destroy_plan(fft->plani);
#endif
  free(fft->window_data);
}

struct paulstretch {
  double amount;
  size_t window_size;

  float *in_buf;
  float *out_buf;
  float *old_out_samples;

  struct fft fft;

  int first_remaining;
  bool processed;
  double remaining_samples;
  bool require_new_buffer;
};

static void process(paulstretch ps, float *samples, size_t length) {
  if(ps->first_remaining > 0) {
    memcpy(&ps->in_buf[ps->window_size * (3 - ps->first_remaining)], samples, ps->window_size * sizeof(float));
    if(--ps->first_remaining > 0) {
      return;
    }
  } else if(length > 0) {
    memmove(ps->in_buf, &ps->in_buf[ps->window_size], ps->window_size * 2 * sizeof(float));
    memcpy(&ps->in_buf[ps->window_size * 2], samples, ps->window_size * sizeof(float));
  }
  ps->processed = true;

  size_t start_pos = (size_t)(floor(ps->remaining_samples * ps->window_size));
  if(start_pos >= ps->window_size) {
    start_pos = ps->window_size - 1;
  }
  #ifdef KISSFFT
    memcpy(ps->fft.smp, &ps->in_buf[start_pos], ps->window_size * 2 * sizeof(float));
  #else
    memcpy(ps->fft.data, &ps->in_buf[start_pos], ps->window_size * 2 * sizeof(float));
  #endif

  fft_apply_window(&ps->fft);
  fft_s2f(&ps->fft);
  fft_f2s(&ps->fft);

  float frequency = 1.0f / (float)(ps->window_size) * PI;
  float hinv_sqrt2 = (1.0 + 1.0 / sqrt(2)) * 0.5;
  float amplification = 2.0;

  for (size_t i = 0; i < ps->window_size; ++i) {
    float lerp_factor = 0.5 + 0.5 * cos(i * frequency);
    #ifdef KISSFFT
    float lerped = ps->fft.smp[i + ps->window_size] * (1.0 - lerp_factor) + ps->old_out_samples[i] * lerp_factor;
    #else
    float lerped = ps->fft.data[i + ps->window_size] * (1.0 - lerp_factor) + ps->old_out_samples[i] * lerp_factor;
    #endif
    float val = lerped * (hinv_sqrt2 - (1.0 - hinv_sqrt2) * cos(i * 2.0 * frequency)) * amplification;
    if(val >= 1) {
      val = 1;
    } else if(val <= -1) {
      val = -1;
    }
    ps->out_buf[i] = val;
  }

  #ifdef KISSFFT
  memcpy(ps->old_out_samples, ps->fft.smp, ps->window_size * sizeof(float));
  #else
  memcpy(ps->old_out_samples, ps->fft.data, ps->window_size * sizeof(float));
  #endif

  ps->remaining_samples += 1.0 / ps->amount;
  if (ps->remaining_samples >= 1.0) {
    ps->remaining_samples = ps->remaining_samples - floor(ps->remaining_samples);
    ps->require_new_buffer = true;
  } else {
    ps->require_new_buffer = false;
  }
}

paulstretch paulstretch_create(double amount, size_t window_size) {
  paulstretch ps = malloc(sizeof(*(paulstretch)(NULL)));

  ps->amount = amount;
  ps->window_size = window_size;

  ps->in_buf = malloc(window_size * 3 * sizeof(float));
  ps->out_buf = malloc(window_size * sizeof(float));
  ps->old_out_samples = calloc(window_size, sizeof(float));

  fft_init(&ps->fft, window_size * 2);

  ps->first_remaining = 3;
  ps->processed = false;
  ps->remaining_samples = 0.0;
  ps->require_new_buffer = true;

  return ps;
}

void paulstretch_write(paulstretch ps, float *samples) {
  assert(!ps->processed && ps->require_new_buffer);

  process(ps, samples, ps->window_size);
}

bool paulstretch_read(paulstretch ps, float **samples) {
  if(ps->processed) {
    ps->processed = false;
    *samples = ps->out_buf;
    return true;
  }
  if(!ps->require_new_buffer) {
    process(ps, NULL, 0);
    ps->processed = false;
    *samples = ps->out_buf;
    return true;
  }
  return false;
}

void paulstretch_destroy(paulstretch ps) {
  free(ps->in_buf);
  free(ps->out_buf);
  free(ps->old_out_samples);

  fft_finish(&ps->fft);
}
