/* 
 * generate a sum of sine waves and play the waveform.
 *
 * Each sine wave is defined by a tuple of:
 *   frequency: in hz [2 - 20000 are reasonable values]
 *   amplitude: as the upper half of a 16-bit range [0-32767]
 *       phase: as an angle in degrees (0-360)

 * A PCM representation is formed in memory and then
 * passed into gstreamer for playback.
 *
 * Troy D. Hanson, 14 July 2014
 * 
 * TODO generate image of waveform
 * 
 */
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "utarray.h"

void usage(char *exe) {
  fprintf(stderr,"usage: %s [-v] [-r <sample-rate>] [-s <seconds>] "
                                 "freq/amp/phase [...] \n", exe);
  fprintf(stderr,"sample rate is usually up to 44100 (CD quality)\n"
  fprintf(stderr,"freq/amp/phase is a sine wave specification (repeatable)\n");
  fprintf(stderr,"freq in hz (2-20000)\n");
  fprintf(stderr,"amp (0-32767) for upper half of 16-bit resolution\n");
  fprintf(stderr,"phase (0-360) degrees to offset vs relative sines\n");
  exit(-1);
}

struct {
  UT_array *sines;
  int verbose;
  int sample_rate;
  int duration;
} cfg = {
  .sample_rate=44100,
  .duration = 4, /* seconds of pcm audio to produce */
};

typedef struct {
  int freq;
  int amp;
  int phase;
} sine_t;

UT_icd sines_icd = {.sz = sizeof(sine_t)};

int main (int argc, char *argv[]) {
  char *exe = argv[0];
  int opt;
  utarray_new(cfg.sines, &sines_icd);

  while ( (opt = getopt(argc, argv, "v+h")) != -1) {
    switch (opt) {
      case 'v': cfg.verbose++; break;
      case 'h': default: usage(exe); break;
    }
  }
  if (optind >= argc) goto done;

  while (optind < argc) {
    char *spec = argv[optind++];
    char *f,*a,*p,*slash;
    f = spec;  // start of freq
    slash = f+1; 
    while(*slash != '/' && *slash != '\0') slash++; 
    if (*slash != '/') usage(exe);
    *slash='\0';

    a=slash+1; // start of amp
    slash=a+1;
    while(*slash != '/' && *slash != '\0') slash++; 
    if (*slash != '/') usage(exe);
    *slash='\0';

    p=slash+1; // start of phase

    int freq = atoi(f);
    int amp = atoi(a);
    int phase = atoi(p);
    if (freq < 2 || freq > 20000) 
      fprintf(stderr,"frequency %d may exceed audible range\n", freq);
    if (amp > 32767) {
      fprintf(stderr,"amplitude %d exceeds 32767, clipping\n", amp);
      amp = 32767;
    }
    /* TODO normalize phase */
    //fprintf(stderr,"freq=%d amp=%d phase=%d\n", freq, amp, phase);
    sine_t s;
    s.freq = freq;
    s.amp = amp;
    s.phase = phase;
    utarray_push_back(cfg.sines, &s);
  }

  /* set up a memory buffer of the appropriate duration */
  char *buf = malloc(cfg.sample_rate * cfg.duration);
  /* TODO should I fix the resolution at 16 bit? */


  /* now iterate over them and produce a PCM waveform in memory */

 done:
  utarray_free(cfg.sines);
  return 0;
}
