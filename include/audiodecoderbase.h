/*
 * libaudiodecoder - Native Portable Audio Decoder Library
 * libaudiodecoder API Header File
 * Latest version available at: http://www.oscillicious.com/libaudiodecoder
 *
 * Copyright (c) 2010-2012 Albert Santoni, Bill Good, RJ Ryan  
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire libaudiodecoder license; however, 
 * the Oscillicious community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also 
 * requested that these non-binding requests be included along with the 
 * license above.
 */


#ifndef __AUDIODECODERBASE_H__
#define __AUDIODECODERBASE_H__

#include <string>
#include <vector>

#ifdef _WIN32
#define DllExport   __declspec( dllexport )
#else
#define DllExport
#endif

//Types
typedef float SAMPLE;

//Error codes
#define AUDIODECODER_ERROR -1
#define AUDIODECODER_OK     0


class DllExport AudioDecoderBase
{
    public:
        AudioDecoderBase(const std::string& filename);
        virtual ~AudioDecoderBase();

        int open() { return 0; };
        long seek(unsigned long filepos) { return 0l; };
        unsigned read(unsigned long size, const SAMPLE *buffer) { return 0u; };
        inline unsigned long    numSamples()        const { return m_iNumSamples; };
        inline unsigned long    channels()          const { return m_iChannels; };
        inline unsigned long    sampleRate()        const { return m_iSampleRate; };
        inline float            duration()          const { return m_fDuration; };
        inline unsigned long    positionInSamples() const { return m_iPositionInSamples; };
        static std::vector<std::string> supportedFileExtensions()
        { 
            return std::vector<std::string>();
        };
    protected:
        std::string     m_filename;
        unsigned long   m_iNumSamples;
        unsigned long   m_iChannels;
        unsigned long   m_iSampleRate;
        float           m_fDuration; // in seconds
        unsigned long   m_iPositionInSamples; // in samples;
};

#endif //__AUDIODECODERBASE_H__
