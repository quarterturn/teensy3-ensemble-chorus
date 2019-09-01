/* Audio Library for Teensy 3.X
 * Copyright (c) 2019, Alexander Davis info@matrixwide.com
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

// My take on a string machine ensemble chorus sound like the Roland RS-202
// or Lowrey Symphonic Strings.
// It takes the approach of 6.0 Hz and 0.6 Hz sinewaves making up an LFO,
// which then modulates three delay lines 120 degrees apart in the LFO waveform.

#include <Arduino.h>
#include "effect_ensemble.h"
#include "utility/dspinst.h"
#include "arm_math.h"

AudioEffectEnsemble::AudioEffectEnsemble() : AudioStream(1, inputQueueArray)
{
	memset(delayBuffer, 0, sizeof(delayBuffer));
    memset(lfoTable, 0, sizeof(lfoTable));
    
    // input index
    inIndex = 0;
    // output indexes
    // default to center of buffer
    outIndex1 = 256;
    outIndex2 = 256;
    outIndex3 = 256;
    // lfo index
    // seprated by thirds to approximate 120 degree phase relationship
    lfoIndex1 = 0;
    lfoIndex2 = 245;
    lfoIndex3 = 490;

    // lfo rate counter
    lfoCount = 0;
    // input index offset
    offset1 = 0;
    offset2 = 0;
    offset3 = 0;
    offsetIndex1 = 0;
    offsetIndex2 = 0;
    offsetIndex3 = 0;
    
    // generate the LFO wavetable
    for (iC = 0; iC < LFO_SAMPLES; iC++)
    {
        lfoTable[iC] = round(((sin(((2.0 * M_PI)/LFO_SAMPLES) * iC) * LFO_RANGE) / 2.0) + (((sin(((20.0 * M_PI)/LFO_SAMPLES) * iC)) * LFO_RANGE) / 4.7));
    }
    
    return;
    
}

// TODO: move this to one of the data files, use in output_adat.cpp, output_tdm.cpp, etc
static const audio_block_t zeroblock = {
    0, 0, 0, {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#if AUDIO_BLOCK_SAMPLES > 16
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
#if AUDIO_BLOCK_SAMPLES > 32
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
#if AUDIO_BLOCK_SAMPLES > 48
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
#if AUDIO_BLOCK_SAMPLES > 64
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
#if AUDIO_BLOCK_SAMPLES > 80
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
#if AUDIO_BLOCK_SAMPLES > 96
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
#if AUDIO_BLOCK_SAMPLES > 112
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
    } };

void AudioEffectEnsemble::update(void)
{
	const audio_block_t *block;
    audio_block_t *outblock;
	uint16_t i;

    outblock = allocate();
    if (!outblock) {
        audio_block_t *tmp = receiveReadOnly(0);
        if (tmp) release(tmp);
        return;
    }
	block = receiveReadOnly(0);
    if (!block)
        block = &zeroblock;

    // buffer the incoming block
    for (i=0; i < AUDIO_BLOCK_SAMPLES; i++)
    {

        // wrap the input index
        inIndex++;
        if (inIndex > (BUFFER_SIZE - 1))
            inIndex = 0;

        delayBuffer[inIndex] = block->data[i];

    }
    // re-load the block with the delayed data
    for (i=0; i < AUDIO_BLOCK_SAMPLES; i++)
    {
        // advance the wavetable indexes every COUNTS_PER_LFO
        // so the LFO modulates at the correct rate
        lfoCount++;
        if (lfoCount > COUNTS_PER_LFO)
        {
            // wrap the lfo index
            lfoIndex1++;
            if (lfoIndex1 > (LFO_SIZE - 1))
                lfoIndex1 = 0;
            lfoIndex2++;
            if (lfoIndex2 > (LFO_SIZE - 1))
                lfoIndex2 = 0;
            lfoIndex3++;
            if (lfoIndex3 > (LFO_SIZE - 1))
                lfoIndex3 = 0;

            // reset the counter
            lfoCount = 0;
        }

        // wrap the output index
        outIndex1++;
        if (outIndex1 > (BUFFER_SIZE - 1))
            outIndex1 = 0;

        outIndex2++;
        if (outIndex2 > (BUFFER_SIZE - 1))
            outIndex2 = 0;

        outIndex3++;
        if (outIndex3 > (BUFFER_SIZE - 1))
            outIndex3 = 0;

        // get the delay from the wavetable
        offset1 = lfoTable[lfoIndex1];
        offset2 = lfoTable[lfoIndex2];
        offset3 = lfoTable[lfoIndex3];

        // add the delay to the buffer index to get the delay index
        offsetIndex1 = outIndex1 + offset1;
        offsetIndex2 = outIndex2 + offset2;
        offsetIndex3 = outIndex3 + offset3;


        // wrap the index if it goes past the end of the buffer
        if (offsetIndex1 > (BUFFER_SIZE - 1))
            offsetIndex1 = offsetIndex1 - BUFFER_SIZE;
        if (offsetIndex2 > (BUFFER_SIZE - 1))
            offsetIndex2 = offsetIndex2 - BUFFER_SIZE;
        if (offsetIndex3 > (BUFFER_SIZE - 1))
            offsetIndex3 = offsetIndex3 - BUFFER_SIZE;

        // wrap the index if it goes past the buffer the other way
        if (offsetIndex1 < 0)
            offsetIndex1 = BUFFER_SIZE + offsetIndex1;
        if (offsetIndex2 < 0)
            offsetIndex2 = BUFFER_SIZE + offsetIndex2;
        if (offsetIndex3 < 0)
            offsetIndex3 = BUFFER_SIZE + offsetIndex3;

        // combine delayed samples into output
        // add the delayed and scaled samples
        outblock->data[i] = 16384 + (delayBuffer[offsetIndex1] >> 2) + (delayBuffer[offsetIndex2] >> 2) + (delayBuffer[offsetIndex3] >> 2);

    }

    transmit(outblock);
    release(outblock);
    if (block != &zeroblock) release((audio_block_t *)block);

    
    return;

}



