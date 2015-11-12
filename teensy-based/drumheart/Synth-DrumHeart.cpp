/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "Synth-DrumHeart.h"


// waveforms.c
extern "C" {
extern const int16_t AudioWaveformSine[257];
}


void AudioSynthDrumHeart::noteOn(void)
{
  __disable_irq();

  //if(env_lin_current < 0x0ffff)
  {
    wav_phasor = 0;
  }

  env_lin_current = 0x7fff0000;
  
  __enable_irq();
}

void AudioSynthDrumHeart::update(void)
{
  audio_block_t *block_wav, *block_env;
  int16_t *p_wave, *p_env, *end;
  int32_t sin_l, sin_r, interp, mod;
  uint32_t index, scale;

  block_env = allocate();
  if (!block_env) return;
  p_env = (block_env->data);
  end = p_env + AUDIO_BLOCK_SAMPLES;

  block_wav = allocate();
  if (!block_wav) return;
  p_wave = (block_wav->data);

  while(p_env < end)
  {
    // Do envelope first
    if(env_lin_current < 0x0000ffff)
    {
      *p_env = 0;
    }
    else
    {
      env_lin_current -= env_decrement;
      env_sqr_current = multiply_16tx16t(env_lin_current, env_lin_current) ;
      *p_env = env_sqr_current>>15;
    }  

    // do wave second;
    mod = multiply_16tx16b(env_sqr_current<<1, wav_pitch_mod);
    
    wav_phasor += wav_increment;
    wav_phasor += mod;
    wav_phasor &= 0x7fffffff;

    // then interp'd waveshape
    index = wav_phasor >> 23; // take top valid 8 bits
    sin_l = AudioWaveformSine[index];
    sin_r = AudioWaveformSine[index+1];

    scale = (wav_phasor >> 7) & 0xFFFF;
    sin_r *= scale;
    sin_l *= 0xFFFF - scale;
    interp = (sin_l + sin_r) >> 16;
    *p_wave = interp;

    p_env++;
    p_wave++;
 }

  transmit(block_env, 0);
  release(block_env);
  
  transmit(block_wav, 1);
  release(block_wav);

}

