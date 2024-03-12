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

/**
 * \file audiodecodermediafoundation.h
 * \class AudioDecoderMediaFoundation
 * \brief Decodes MPEG4/AAC audio using the SourceReader interface of the
 * Media Foundation framework included in Windows 7.
 * \author Bill Good <bkgood at gmail dot com>
 * \author Albert Santoni <alberts at mixxx dot org>
 * \date Jan 10, 2011
 */


#ifndef AUDIODECODERMEDIAFOUNDATION_H
#define AUDIODECODERMEDIAFOUNDATION_H

#include "audiodecoderbase.h"

class IMFSourceReader;
class IMFMediaType;
class IMFMediaSource;

#define SHORT_SAMPLE short

class DllExport AudioDecoderMediaFoundation : public AudioDecoderBase {
  public:
    AudioDecoderMediaFoundation(const std::string filename);
    ~AudioDecoderMediaFoundation();
    int open();
    int seek(int sampleIdx);
    int read(int size, const SAMPLE *buffer);
    inline int numSamples();
    std::vector<std::string> supportedFileExtensions();

  private:
    bool configureAudioStream();
    bool readProperties();
    void copyFrames(short *dest, size_t *destFrames, const short *src,
        size_t srcFrames);
    inline double secondsFromMF(__int64 mf);
    inline __int64 mfFromSeconds(double sec);
    inline __int64 frameFromMF(__int64 mf);
    inline __int64 mfFromFrame(__int64 frame);
    IMFSourceReader *m_pReader;
    IMFMediaType *m_pAudioType;
    wchar_t *m_wcFilename;
    int m_nextFrame;
    short *m_leftoverBuffer;
    size_t m_leftoverBufferSize;
    size_t m_leftoverBufferLength;
    int m_leftoverBufferPosition;
    __int64 m_mfDuration;
    long m_iCurrentPosition;
    bool m_dead;
    bool m_seeking;
	unsigned int m_iBitsPerSample;
	SHORT_SAMPLE m_destBufferShort[8192];
};

#endif // ifndef AUDIODECODERMEDIAFOUNDATION_H
