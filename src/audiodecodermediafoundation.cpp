/**
 * \file soundsourcemediafoundation.cpp
 * \author Bill Good <bkgood at gmail dot com>
 * \author Albert Santoni <alberts at mixxx dot org>
 * \date Jan 10, 2011
 * \note This file uses COM interfaces defined in Windows 7 and later added to
 * Vista and Server 2008 via the "Platform Update Supplement for Windows Vista
 * and for Windows Server 2008" (http://support.microsoft.com/kb/2117917).
 * Earlier versions of Vista (and possibly Server 2008) have some Media
 * Foundation interfaces but not the required IMFSourceReader, and are missing
 * the Microsoft-provided AAC decoder. XP does not include Media Foundation.
 */

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <iostream>
#include <string.h>
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>
#include <assert.h>

#include "audiodecodermediafoundation.h"

const int kBitsPerSample = 16;
const int kNumChannels = 2;
const int kSampleRate = 44100;
const int kLeftoverSize = 4096; // in int16's, this seems to be the size MF AAC
// decoder likes to give

const static bool sDebug = false;

/** Microsoft examples use this snippet often. */
template<class T> static void safeRelease(T **ppT)
{
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

std::wstring s2ws(const std::string& s)
{
 int len;
 int slength = (int)s.length() + 1;
 len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0); 
 wchar_t* buf = new wchar_t[len];
 MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
 std::wstring r(buf);
 delete[] buf;
 return r;
}


AudioDecoderMediaFoundation::AudioDecoderMediaFoundation(const std::string filename)
    : AudioDecoderBase(filename)
    , m_pReader(NULL)
    , m_pAudioType(NULL)
    , m_wcFilename(NULL)
    , m_nextFrame(0)
    , m_leftoverBuffer(NULL)
    , m_leftoverBufferSize(0)
    , m_leftoverBufferLength(0)
    , m_leftoverBufferPosition(0)
    , m_mfDuration(0)
    , m_iCurrentPosition(0)
    , m_dead(false)
    , m_seeking(false)
{
    //Defaults
    m_iChannels = kNumChannels;
    m_iSampleRate = kSampleRate;
	m_iBitsPerSample = kBitsPerSample;

    // http://social.msdn.microsoft.com/Forums/en/netfxbcl/thread/35c6a451-3507-40c8-9d1c-8d4edde7c0cc
    // gives maximum path + file length as 248 + 260, using that -bkgood
    m_wcFilename = new wchar_t[248 + 260];
}

AudioDecoderMediaFoundation::~AudioDecoderMediaFoundation()
{
    delete [] m_wcFilename;
    delete [] m_leftoverBuffer;

    safeRelease(&m_pReader);
    safeRelease(&m_pAudioType);
    MFShutdown();
    CoUninitialize();
}

int AudioDecoderMediaFoundation::open()
{
    if (sDebug) {
        std::cout << "open() " << m_filename << std::endl;
    }

    //Assumes m_filename is ASCII and converts it to UTF-16 (wide char).
    /*
	int wcFilenameLength = m_filename.size();
	for(std::wstring::size_type i=0; i < m_filename.size(); ++i)
	{
		m_wcFilename[i] = m_filename[i];
	}
	m_wcFilename[wcFilenameLength] = (wchar_t)'\0';

	std::string s;

	std::wstring stemp = s2ws(m_filename); // Temporary buffer is required
	LPCWSTR result = (LPCWSTR)stemp.c_str();
    */
   
    //LPCWSTR result; 
    const char* utf8Str = m_filename.c_str();
    MultiByteToWideChar(CP_UTF8, 
                        0, 
                        utf8Str, 
                        -1, //assume utf8Str is NULL terminated and give us back a NULL terminated string
                        (LPWSTR)m_wcFilename,
                        512);
    
    LPCWSTR result = m_wcFilename;

    HRESULT hr(S_OK);
    // Initialize the COM library.
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to initialize COM" << std::endl;
        return AUDIODECODER_ERROR;
    }

    // Initialize the Media Foundation platform.
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to initialize Media Foundation" << std::endl;
        return AUDIODECODER_ERROR;
    }

    // Create the source reader to read the input file.
    hr = MFCreateSourceReaderFromURL(/*m_wcFilename*/result, NULL, &m_pReader);
    if (FAILED(hr)) {
        std::cerr << "SSMF: Error opening input file:" << m_filename << ", with error: " << HRESULT_CODE(hr) << std::endl;
        return AUDIODECODER_ERROR;
    }

    if (!configureAudioStream()) {
        std::cerr << "SSMF: Error configuring audio stream." << std::endl;
        return AUDIODECODER_ERROR;
    }

    if (!readProperties()) {
        std::cerr << "SSMF::readProperties failed" << std::endl;
        return AUDIODECODER_ERROR;
    }

    //Seek to position 0, which forces us to skip over all the header frames.
    //This makes sure we're ready to just let the Analyser rip and it'll
    //get the number of samples it expects (ie. no header frames).
    seek(0);

    return AUDIODECODER_OK;
}

int AudioDecoderMediaFoundation::seek(int sampleIdx)
{
    if (sDebug) { std::cout << "seek() " << sampleIdx << std::endl; }
    PROPVARIANT prop;
    HRESULT hr(S_OK);
    __int64 seekTarget(sampleIdx / m_iChannels);
    __int64 mfSeekTarget(mfFromFrame(seekTarget) - 1);
    // minus 1 here seems to make our seeking work properly, otherwise we will
    // (more often than not, maybe always) seek a bit too far (although not
    // enough for our calculatedFrameFromMF <= nextFrame assertion in ::read).
    // Has something to do with 100ns MF units being much smaller than most
    // frame offsets (in seconds) -bkgood
    long result = m_iCurrentPosition;
    if (m_dead) {
        return result;
    }

    // this doesn't fail, see MS's implementation
    hr = InitPropVariantFromInt64(mfSeekTarget < 0 ? 0 : mfSeekTarget, &prop);


    hr = m_pReader->Flush(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to flush before seek";
    }

    // http://msdn.microsoft.com/en-us/library/dd374668(v=VS.85).aspx
    hr = m_pReader->SetCurrentPosition(GUID_NULL, prop);
    if (FAILED(hr)) {
        // nothing we can do here as we can't fail (no facility to other than
        // crashing mixxx)
        std::cerr << "SSMF: failed to seek" << (
            hr == MF_E_INVALIDREQUEST ? "Sample requests still pending" : "");
    } else {
        result = sampleIdx;
    }
    PropVariantClear(&prop);

    // record the next frame so that we can make sure we're there the next
    // time we get a buffer from MFSourceReader
    m_nextFrame = seekTarget;
    m_seeking = true;
    m_iCurrentPosition = result;
    return result;
}

int AudioDecoderMediaFoundation::read(int size, const SAMPLE *destination)
{
	assert(size < sizeof(m_destBufferShort));
    if (sDebug) { std::cout << "read() " << size << std::endl; }
	//TODO: Change this up if we want to support just short samples again -- Albert
    SHORT_SAMPLE *destBuffer = m_destBufferShort;
	size_t framesRequested(size / m_iChannels);
    size_t framesNeeded(framesRequested);

    // first, copy frames from leftover buffer IF the leftover buffer is at
    // the correct frame
    if (m_leftoverBufferLength > 0 && m_leftoverBufferPosition == m_nextFrame) {
        copyFrames(destBuffer, &framesNeeded, m_leftoverBuffer,
            m_leftoverBufferLength);
        if (m_leftoverBufferLength > 0) {
            if (framesNeeded != 0) {
                std::cerr << __FILE__ << __LINE__
                           << "WARNING: Expected frames needed to be 0. Abandoning this file.";
                m_dead = true;
            }
            m_leftoverBufferPosition += framesRequested;
        }
    } else {
        // leftoverBuffer already empty or in the wrong position, clear it
        m_leftoverBufferLength = 0;
    }

    while (!m_dead && framesNeeded > 0) {
        HRESULT hr(S_OK);
        DWORD dwFlags(0);
        __int64 timestamp(0);
        IMFSample *pSample(NULL);
        bool error(false); // set to true to break after releasing

        hr = m_pReader->ReadSample(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM, // [in] DWORD dwStreamIndex,
            0,                                   // [in] DWORD dwControlFlags,
            NULL,                                // [out] DWORD *pdwActualStreamIndex,
            &dwFlags,                            // [out] DWORD *pdwStreamFlags,
            &timestamp,                          // [out] LONGLONG *pllTimestamp,
            &pSample);                           // [out] IMFSample **ppSample
        if (FAILED(hr)) {
            if (sDebug) { std::cout << "ReadSample failed." << std::endl; }
            break;
        }

        if (sDebug) {
            std::cout << "ReadSample timestamp: " << timestamp
                     << "frame: " << frameFromMF(timestamp)
                     << "dwflags: " << dwFlags
					 << std::endl;
        }

        if (dwFlags & MF_SOURCE_READERF_ERROR) {
            // our source reader is now dead, according to the docs
            std::cerr << "SSMF: ReadSample set ERROR, SourceReader is now dead";
            m_dead = true;
            break;
        } else if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
            std::cout << "SSMF: End of input file." << std::endl;
            break;
        } else if (dwFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
            std::cerr << "SSMF: Type change";
            break;
        } else if (pSample == NULL) {
            // generally this will happen when dwFlags contains ENDOFSTREAM,
            // so it'll be caught before now -bkgood
            std::cerr << "SSMF: No sample";
            continue;
        } // we now own a ref to the instance at pSample

        IMFMediaBuffer *pMBuffer(NULL);
        // I know this does at least a memcopy and maybe a malloc, if we have
        // xrun issues with this we might want to look into using
        // IMFSample::GetBufferByIndex (although MS doesn't recommend this)
        if (FAILED(hr = pSample->ConvertToContiguousBuffer(&pMBuffer))) {
            error = true;
            goto releaseSample;
        }
        short *buffer(NULL);
        size_t bufferLength(0);
        hr = pMBuffer->Lock(reinterpret_cast<unsigned __int8**>(&buffer), NULL,
            reinterpret_cast<DWORD*>(&bufferLength));
        if (FAILED(hr)) {
            error = true;
            goto releaseMBuffer;
        }
        bufferLength /= (m_iBitsPerSample / 8 * m_iChannels); // now in frames

        if (m_seeking) {
            __int64 bufferPosition(frameFromMF(timestamp));
            if (sDebug) {
                std::cout << "While seeking to "
                         << m_nextFrame << "WMF put us at " << bufferPosition
						 << std::endl;

            }
            if (m_nextFrame < bufferPosition) {
                // Uh oh. We are farther forward than our seek target. Emit
                // silence? We can't seek backwards here.
                SHORT_SAMPLE* pBufferCurpos = destBuffer +
                        (size - framesNeeded * m_iChannels);
                __int64 offshootFrames = bufferPosition - m_nextFrame;

                // If we can correct this immediately, write zeros and adjust
                // m_nextFrame to pretend it never happened.

                if (offshootFrames <= framesNeeded) {
                    std::cerr << __FILE__ << __LINE__
                               << "Working around inaccurate seeking. Writing silence for"
                               << offshootFrames << "frames";
                    // Set offshootFrames * m_iChannels samples to zero.
                    memset(pBufferCurpos, 0,
                           sizeof(*pBufferCurpos) * offshootFrames *
                           m_iChannels);
                    // Now m_nextFrame == bufferPosition
                    m_nextFrame += offshootFrames;
                    framesNeeded -= offshootFrames;
                } else {
                    // It's more complicated. The buffer we have just decoded is
                    // more than framesNeeded frames away from us. It's too hard
                    // for us to handle this correctly currently, so let's just
                    // try to get on with our lives.
                    m_seeking = false;
                    m_nextFrame = bufferPosition;
                    std::cerr << __FILE__ << __LINE__
                               << "Seek offshoot is too drastic. Cutting losses and pretending the current decoded audio buffer is the right seek point.";
                }
            }

            if (m_nextFrame >= bufferPosition &&
                m_nextFrame < bufferPosition + bufferLength) {
                // m_nextFrame is in this buffer.
                buffer += (m_nextFrame - bufferPosition) * m_iChannels;
                bufferLength -= m_nextFrame - bufferPosition;
                m_seeking = false;
            } else {
                // we need to keep going forward
                goto releaseRawBuffer;
            }
        }

        // If the bufferLength is larger than the leftover buffer, re-allocate
        // it with 2x the space.
        if (bufferLength * m_iChannels > m_leftoverBufferSize) {
            int newSize = m_leftoverBufferSize;

            while (newSize < bufferLength * m_iChannels) {
                newSize *= 2;
            }
            SHORT_SAMPLE* newBuffer = new SHORT_SAMPLE[newSize];
            memcpy(newBuffer, m_leftoverBuffer,
                   sizeof(m_leftoverBuffer[0]) * m_leftoverBufferSize);
            delete [] m_leftoverBuffer;
            m_leftoverBuffer = newBuffer;
            m_leftoverBufferSize = newSize;
        }
        copyFrames(destBuffer + (size - framesNeeded * m_iChannels),
            &framesNeeded, buffer, bufferLength);

releaseRawBuffer:
        hr = pMBuffer->Unlock();
        // I'm ignoring this, MSDN for IMFMediaBuffer::Unlock stipulates
        // nothing about the state of the instance if this fails so might as
        // well just let it be released.
        //if (FAILED(hr)) break;
releaseMBuffer:
        safeRelease(&pMBuffer);
releaseSample:
        safeRelease(&pSample);
        if (error) break;
    }

    m_nextFrame += framesRequested - framesNeeded;
    if (m_leftoverBufferLength > 0) {
        if (framesNeeded != 0) {
            std::cerr << __FILE__ << __LINE__
				<< "WARNING: Expected frames needed to be 0. Abandoning this file." << std::endl;
            m_dead = true;
        }
        m_leftoverBufferPosition = m_nextFrame;
    }
    long samples_read = size - framesNeeded * m_iChannels;
    m_iCurrentPosition += samples_read;
    if (sDebug) { std::cout << "read() " << size << " returning " << samples_read << std::endl; }
	
	const int sampleMax = 1 << (m_iBitsPerSample-1);
	//Convert to float samples
	if (m_iChannels == 2)
	{
		SAMPLE *destBufferFloat(const_cast<SAMPLE*>(destination));
		for (unsigned long i = 0; i < samples_read; i++)
		{
			destBufferFloat[i] = destBuffer[i] / (float)sampleMax;
		}
	}
	else //Assuming mono, duplicate into stereo frames...
	{
		SAMPLE *destBufferFloat(const_cast<SAMPLE*>(destination));
		for (unsigned long i = 0; i < samples_read; i++)
		{
			destBufferFloat[i] = destBuffer[i] / (float)sampleMax;
		}
	}
    return samples_read;
}

inline int AudioDecoderMediaFoundation::numSamples()
{
    int len(secondsFromMF(m_mfDuration) * m_iSampleRate * m_iChannels);
    return len % m_iChannels == 0 ? len : len + 1;
}

std::vector<std::string> AudioDecoderMediaFoundation::supportedFileExtensions()
{
    std::vector<std::string> list;
    list.push_back("m4a");
    list.push_back("mp4");
	list.push_back("wma");
	list.push_back("mp3");
	list.push_back("wav");
	list.push_back("aif");
	list.push_back("aiff");
    return list;
}


//-------------------------------------------------------------------
// configureAudioStream
//
// Selects an audio stream from the source file, and configures the
// stream to deliver decoded PCM audio.
//-------------------------------------------------------------------

/** Cobbled together from:
    http://msdn.microsoft.com/en-us/library/dd757929(v=vs.85).aspx
    and http://msdn.microsoft.com/en-us/library/dd317928(VS.85).aspx
    -- Albert
    If anything in here fails, just bail. I'm not going to decode HRESULTS.
    -- Bill
    */
bool AudioDecoderMediaFoundation::configureAudioStream()
{
    HRESULT hr(S_OK);

    // deselect all streams, we only want the first
    hr = m_pReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, false);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to deselect all streams";
        return false;
    }

    hr = m_pReader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, true);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to select first audio stream";
        return false;
    }

//Debugging:
//Let's get some info
	// Get the complete uncompressed format.
    //hr = m_pReader->GetCurrentMediaType(
    //    MF_SOURCE_READER_FIRST_AUDIO_STREAM,
    //   &m_pAudioType);
	hr = m_pReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
									   0, //Index of the media type to retreive... (what does that even mean?)
									   &m_pAudioType);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to retrieve completed media type";
        return false;
    }
	UINT32 allSamplesIndependent	= 0;
	UINT32 fixedSizeSamples		= 0;
	UINT32 sampleSize				= 0;
	UINT32 bitsPerSample			= 0;
	UINT32 blockAlignment			= 0;
	UINT32 numChannels				= 0;
	UINT32 samplesPerSecond		= 0;
	hr = m_pAudioType->GetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, &allSamplesIndependent);
	hr = m_pAudioType->GetUINT32(MF_MT_FIXED_SIZE_SAMPLES, &fixedSizeSamples);
	hr = m_pAudioType->GetUINT32(MF_MT_SAMPLE_SIZE, &sampleSize);
	hr = m_pAudioType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
	hr = m_pAudioType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlignment);
	hr = m_pAudioType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &numChannels);
	hr = m_pAudioType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samplesPerSecond);

	std::cout << "bitsPerSample: " << bitsPerSample << std::endl;
	std::cout << "allSamplesIndependent: " << allSamplesIndependent << std::endl;
	std::cout << "fixedSizeSamples: " << fixedSizeSamples << std::endl;
	std::cout << "sampleSize: " << sampleSize << std::endl;
	std::cout << "bitsPerSample: " << bitsPerSample << std::endl;
	std::cout << "blockAlignment: " << blockAlignment << std::endl;
	std::cout << "numChannels: " << numChannels << std::endl;
	std::cout << "samplesPerSecond: " << samplesPerSecond << std::endl;

	m_iChannels = numChannels;
	m_iSampleRate = samplesPerSecond;
	m_iBitsPerSample = bitsPerSample;
	//For compressed files, the bits per sample is undefined, so by convention we're
	//going to get 16-bit integers out.
	if (m_iBitsPerSample == 0)
	{
		m_iBitsPerSample = kBitsPerSample;
	}

    hr = MFCreateMediaType(&m_pAudioType);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to create media type";
        return false;
    }

    hr = m_pAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to set major type";
        return false;
    }

    hr = m_pAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to set subtype";
        return false;
    }
/*
    hr = m_pAudioType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, true);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to set samples independent";
        return false;
    }

    hr = m_pAudioType->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, true);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to set fixed size samples";
        return false;
    }

    hr = m_pAudioType->SetUINT32(MF_MT_SAMPLE_SIZE, kLeftoverSize);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to set sample size";
        return false;
    }

    // MSDN for this attribute says that if bps is 8, samples are unsigned.
    // Otherwise, they're signed (so they're signed for us as 16 bps). Why
    // chose to hide this rather useful tidbit here is beyond me -bkgood
    hr = m_pAudioType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, kBitsPerSample);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to set bits per sample";
        return false;
    }


    hr = m_pAudioType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,
        numChannels * (kBitsPerSample / 8));
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to set block alignment";
        return false;
    }
	*/

	/*
	//MediaFoundation will not convert between mono and stereo without a transform!
    hr = m_pAudioType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, kNumChannels);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to set number of channels";
        return false;
    }

	
	//MediaFoundation will not do samplerate conversion without a transform in the pipeline.
    hr = m_pAudioType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, kSampleRate);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to set sample rate";
        return false;
    }
	*/

    // Set this type on the source reader. The source reader will
    // load the necessary decoder.
    hr = m_pReader->SetCurrentMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        NULL, m_pAudioType);

    // the reader has the media type now, free our reference so we can use our
    // pointer for other purposes. Do this before checking for failure so we
    // don't dangle.
    safeRelease(&m_pAudioType);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to set media type";
        return false;
    }

    // Get the complete uncompressed format.
    hr = m_pReader->GetCurrentMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        &m_pAudioType);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to retrieve completed media type";
        return false;
    }

    // Ensure the stream is selected.
    hr = m_pReader->SetStreamSelection(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        true);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to select first audio stream (again)";
        return false;
    }

    // this may not be safe on all platforms as m_leftoverBufferSize is a
    // size_t and this function is writing a uint32. However, on 32-bit
    // Windows 7, size_t is defined as uint which is 32-bits, so we're safe
    // for all supported platforms -bkgood
    UINT32 leftoverBufferSize = 0;
    hr = m_pAudioType->GetUINT32(MF_MT_SAMPLE_SIZE, &leftoverBufferSize);
    if (FAILED(hr)) {
        std::cerr << "SSMF: failed to get buffer size";
		leftoverBufferSize = 32;
       // return false;
    }
    m_leftoverBufferSize = static_cast<size_t>(leftoverBufferSize);
    m_leftoverBufferSize /= 2; // convert size in bytes to size in int16s
    m_leftoverBuffer = new short[m_leftoverBufferSize];

    return true;
}

bool AudioDecoderMediaFoundation::readProperties()
{
    PROPVARIANT prop;
    HRESULT hr = S_OK;

    //Get the duration, provided as a 64-bit integer of 100-nanosecond units
    hr = m_pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,
        MF_PD_DURATION, &prop);
    if (FAILED(hr)) {
        std::cerr << "SSMF: error getting duration";
        return false;
    }
    // QuadPart isn't available on compilers that don't support _int64. Visual
    // Studio 6.0 introduced the type in 1998, so I think we're safe here
    // -bkgood
    m_fDuration = secondsFromMF(prop.hVal.QuadPart);
    m_mfDuration = prop.hVal.QuadPart;
    std::cout << "SSMF: Duration: " << m_fDuration << std::endl;
    PropVariantClear(&prop);

    // presentation attribute MF_PD_AUDIO_ENCODING_BITRATE only exists for
    // presentation descriptors, one of which MFSourceReader is not.
    // Therefore, we calculate it ourselves.
    //m_iBitrate = m_iBitsPerSample * m_iSampleRate * m_iChannels;
	//XXX: Should we implement bitrate in libaudiodecoder? Just enable that line...

    return true;
}

/**
 * Copies min(destFrames, srcFrames) frames to dest from src. Anything leftover
 * is moved to the beginning of m_leftoverBuffer, so empty it first (possibly
 * with this method). If src and dest overlap, I'll hurt you.
 */
void AudioDecoderMediaFoundation::copyFrames(
    short *dest, size_t *destFrames, const short *src, size_t srcFrames)
{
    if (srcFrames > *destFrames) {
        int samplesToCopy(*destFrames * m_iChannels);
        memcpy(dest, src, samplesToCopy * sizeof(*src));
        srcFrames -= *destFrames;
        memmove(m_leftoverBuffer,
            src + samplesToCopy,
            srcFrames * m_iChannels * sizeof(*src));
        *destFrames = 0;
        m_leftoverBufferLength = srcFrames;
    } else {
        int samplesToCopy(srcFrames * m_iChannels);
        memcpy(dest, src, samplesToCopy * sizeof(*src));
        *destFrames -= srcFrames;
        if (src == m_leftoverBuffer) {
            m_leftoverBufferLength = 0;
        }
    }
}

/**
 * Convert a 100ns Media Foundation value to a number of seconds.
 */
inline double AudioDecoderMediaFoundation::secondsFromMF(__int64 mf)
{
    return static_cast<double>(mf) / 1e7;
}

/**
 * Convert a number of seconds to a 100ns Media Foundation value.
 */
inline __int64 AudioDecoderMediaFoundation::mfFromSeconds(double sec)
{
    return sec * 1e7;
}

/**
 * Convert a 100ns Media Foundation value to a frame offset.
 */
inline __int64 AudioDecoderMediaFoundation::frameFromMF(__int64 mf)
{
    return static_cast<double>(mf) * m_iSampleRate / 1e7;
}

/**
 * Convert a frame offset to a 100ns Media Foundation value.
 */
inline __int64 AudioDecoderMediaFoundation::mfFromFrame(__int64 frame)
{
    return static_cast<double>(frame) / m_iSampleRate * 1e7;
}
