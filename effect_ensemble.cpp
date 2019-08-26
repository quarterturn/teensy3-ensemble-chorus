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
// A stereo effect comes from offsetting the LFO on one channel by 90 degrees.

#include <Arduino.h>
#include "effect_ensemble.h"
#include "utility/dspinst.h"
#include "math.h"

AudioEffectEnsemble::AudioEffectEnsemble() : AudioStream(1, inputQueueArray)
{
	memset(delayBuffer1L, 0, sizeof(delayBuffer1L));
	memset(delayBuffer1R, 0, sizeof(delayBuffer1R));
    memset(delayBuffer2L, 0, sizeof(delayBuffer2L));
    memset(delayBuffer2R, 0, sizeof(delayBuffer2R));
    memset(delayBuffer3L, 0, sizeof(delayBuffer3L));
    memset(delayBuffer3R, 0, sizeof(delayBuffer3R));
    memset(lfoTable, 0, sizeof(lfoTable));
    
    // input index
    inIndex = 0;
    // output indexes
    // default to center of buffer
    outIndex1 = 1023;
    outIndex2 = 1023;
    outIndex3 = 1023;
    // lfo index
    // seprated by thirds to approximate 120 degree phase relationship
    lfoIndex1L = 0;
    lfoIndex2L = 245;
    lfoIndex3L = 490;
    lfoIndex1R = 0 + PHASE_90;
    lfoIndex2R = 245 + PHASE_90;
    lfoIndex3R = 490 + PHASE_90;
    // lfo rate counter
    lfoCount = 0;
    // output index offset
    offset1L = 0;
    offset2L = 0;
    offset3L = 0;
    offsetIndex1L = 0;
    offsetIndex2L = 0;
    offsetIndex3L = 0;
    offset1R = 0;
    offset2R = 0;
    offset3R = 0;
    offsetIndex1R = 0;
    offsetIndex2R = 0;
    offsetIndex3R = 0;
    
    // generate the LFO wavetable
    for (iC = 0; iC < LFO_SAMPLES; iC++)
    {
        lfoTable[iC] = round(((sin(((2.0 * M_PI)/LFO_SAMPLES) * iC) * LFO_RANGE) / 2.0) + (((sin(((20.0 * M_PI)/LFO_SAMPLES) * iC)) * LFO_RANGE) / 4.7));
    }
    
}

void AudioEffectEnsemble::update()
{
#if defined(__ARM_ARCH_7EM__)
	const audio_block_t *blockL;
    const audio_block_t *blockR;
	audio_block_t *outblockL;
	audio_block_t *outblockR;
	uint16_t i;
	int16_t inputL, inputR, outputL, outputR;
    int isChR = 1;
    int isChL = 1;

	blockL = receiveReadOnly(0);
//    if (!blockL)
//    {
//        release(blockL);
//        isChL = 0;
//    }
    blockR = receiveReadOnly(1);
//    if (!blockR)
//    {
//        release(blockR);
//        isChR = 0;
//    }
    
    // allocate memory as needed
    if (isChL)
        outblockL = allocate();
    if (isChR)
        outblockR = allocate();
	if (!outblockL || !outblockR) {
		if (outblockL) release(outblockL);
		if (outblockR) release(outblockR);
		if (blockL) release((audio_block_t *)blockL);
        if (blockR) release((audio_block_t *)blockR);
		return;
	}

    // iterate through the block for each channel if present
    for (i=0; i < AUDIO_BLOCK_SAMPLES; i++)
    {
        // get a sample
        if (isChL)
            inputL = blockL->data[i];
        if (isChR)
            inputR = blockR->data[i];
        
        // store samples in buffer
        if (isChL)
        {
            delayBuffer1L[inIndex] = inputL;
            delayBuffer2L[inIndex] = inputL;
            delayBuffer3L[inIndex] = inputL;
        }
        if (isChR)
        {
            delayBuffer1R[inIndex] = inputR;
            delayBuffer2R[inIndex] = inputR;
            delayBuffer3R[inIndex] = inputR;
        }
        
        // advance the wavetable indexes every COUNTS_PER_LFO
        // so the LFO modulates at the correct rate
        lfoCount++;
        if (lfoCount > COUNTS_PER_LFO)
        {
            // wrap the left index
            lfoIndex1L++;
            if (lfoIndex1L > (LFO_SIZE - 1))
                lfoIndex1L = 0;
            lfoIndex2L++;
            if (lfoIndex2L > (LFO_SIZE - 1))
                lfoIndex2L = 0;
            lfoIndex3L++;
            if (lfoIndex3L > (LFO_SIZE - 1))
                lfoIndex3L = 0;
            // wrap the right index
            lfoIndex1R++;
            if (lfoIndex1R > (LFO_SIZE - 1))
                lfoIndex1R = 0;
            lfoIndex2R++;
            if (lfoIndex2R > (LFO_SIZE - 1))
                lfoIndex2R = 0;
            lfoIndex3R++;
            if (lfoIndex3R > (LFO_SIZE - 1))
                lfoIndex3R = 0;
            
            // reset the counter
            lfoCount = 0;
        }
        
        // wrap the input index
        inIndex++;
        if (inIndex > (BUFFER_SIZE - 1))
            inIndex = 0;
        
        // wrap the output indexes
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
        offset1L = lfoTable[lfoIndex1L];
        offset2L = lfoTable[lfoIndex2L];
        offset3L = lfoTable[lfoIndex3L];
        offset1R = lfoTable[lfoIndex1R];
        offset2R = lfoTable[lfoIndex2R];
        offset3R = lfoTable[lfoIndex3R];
        
        // add the delay to the buffer index to get the delay index
        offsetIndex1L = outIndex1 + offset1L;
        offsetIndex2L = outIndex2 + offset2L;
        offsetIndex3L = outIndex3 + offset3L;
        offsetIndex1R = outIndex1 + offset1R;
        offsetIndex2R = outIndex2 + offset2R;
        offsetIndex3R = outIndex3 + offset3R;
        
        // wrap the index if it goes past the end of the buffer
        if (offsetIndex1L > (BUFFER_SIZE - 1))
            offsetIndex1L = offsetIndex1L - BUFFER_SIZE;
        if (offsetIndex2L > (BUFFER_SIZE - 1))
            offsetIndex2L = offsetIndex2L - BUFFER_SIZE;
        if (offsetIndex3L > (BUFFER_SIZE - 1))
            offsetIndex3L = offsetIndex3L - BUFFER_SIZE;
        if (offsetIndex1R > (BUFFER_SIZE - 1))
            offsetIndex1R = offsetIndex1R - BUFFER_SIZE;
        if (offsetIndex2R > (BUFFER_SIZE - 1))
            offsetIndex2R = offsetIndex2R - BUFFER_SIZE;
        if (offsetIndex3R > (BUFFER_SIZE - 1))
            offsetIndex3R = offsetIndex3R - BUFFER_SIZE;
        
        // wrap the index if it goes past the buffer the other way
        if (offsetIndex1L < 0)
            offsetIndex1L = BUFFER_SIZE + offsetIndex1L;
        if (offsetIndex2L < 0)
            offsetIndex2L = BUFFER_SIZE + offsetIndex2L;
        if (offsetIndex3L < 0)
            offsetIndex3L = BUFFER_SIZE + offsetIndex3L;
        if (offsetIndex1R < 0)
            offsetIndex1R = BUFFER_SIZE + offsetIndex1R;
        if (offsetIndex2R < 0)
            offsetIndex2R = BUFFER_SIZE + offsetIndex2R;
        if (offsetIndex3R < 0)
            offsetIndex3R = BUFFER_SIZE + offsetIndex3R;
        
        // combine delayed samples into output
        if (isChL)
        {
            outputL = 16384 + (delayBuffer1L[offsetIndex1L] >> 2) + (delayBuffer2L[offsetIndex2L] >> 2) + (delayBuffer3L[offsetIndex3L] >> 2);
            outblockL->data[i] = outputL;
        }
        if (isChR)
        {
            outputR = 16384 + (delayBuffer1R[offsetIndex1R] >> 2) + (delayBuffer2R[offsetIndex2R] >> 2) + (delayBuffer3R[offsetIndex3R] >> 2);
            outblockR->data[i] = outputR;
        }
    }
    if (isChL)
        //transmit(outblockL, 0);
        transmit(blockL, 0);
    if (isChR)
        //transmit(outblockR, 1);
        transmit(blockR, 0);
	release(outblockL);
	release(outblockR);

#elif defined(KINETisChL)
	audio_block_t *block;
	block = receiveReadOnly(0);
	if (block) release(block);
#endif
}



