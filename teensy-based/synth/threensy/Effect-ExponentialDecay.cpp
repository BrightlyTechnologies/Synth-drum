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

#include "Effect-ExponentialDecay.h"


void AudioEffectExponentialDecay::noteOn(void)
{
  __disable_irq();

  current = 0x7fff0000;
  
  __enable_irq();
}

void AudioEffectExponentialDecay::update(void)
{
  audio_block_t *block;
  uint32_t *p, *end;
  uint32_t remaining, mul, smple12, tmp1, tmp2;

  block = receiveWritable();
  if (!block) return;

  p = (uint32_t *)(block->data);
  end = p + AUDIO_BLOCK_SAMPLES/2;

  if(current < 0x0000ffff)
  {
    current = 0;
    while (p < end) 
    {
      *p = pack_16t_16t(current, current);
      p++;
    }
  }
  else
  {
    remaining = current / increment;

    if(remaining > 128)
    {
      while(p < end)
      {
        smple12 = *p;
        current -= increment;
        mul = multiply_16tx16t(current, current) ;
        tmp1 = multiply_16tx16b(mul<<1, smple12);
        current -= increment;
        mul = multiply_16tx16t(current, current) ;
        tmp2 = multiply_16tx16t(mul<<1, smple12);
        smple12 = pack_16t_16t(tmp2, tmp1);
        *p = smple12;
        p++;
      }
    }
    else // less than 128 remaining
    {
      while(remaining >= 2)
      {
        remaining -= 2;
        current -= increment;
        mul = multiply_16tx16t(current, current) ;
        tmp1 = multiply_16tx16b(mul<<1, smple12);
        current -= increment;
        mul = multiply_16tx16t(current, current) ;
        tmp2 = multiply_16tx16t(mul<<1, smple12);
        smple12 = pack_16t_16t(tmp2, tmp1);
        *p = smple12;
        p++;
          
      }
       
      if (remaining == 1)
      {
        smple12 = *p;
        current -= increment;
        mul = multiply_16tx16t(current, current);
        tmp1 = multiply_16tx16b(mul<<1, smple12);
        current = 0;
        tmp2 = 0;
        smple12 = pack_16t_16t(tmp2, tmp1);
        *p = smple12;
        p++;
      }

      smple12 = 0;
      while(p < end)
      {
        *p = smple12;
        p++;
      }
    }
  }

  transmit(block);
  release(block);

}

