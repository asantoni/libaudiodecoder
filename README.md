libaudiodecoder
===============

The Cross-Platform Audio Decoder API (C++)
---------------

libaudiodecoder provides a common interface for low-level compressed audio file decoding on Windows and Mac OS X, giving
you access to raw audio samples. Wrapping the audio APIs provided by Windows and Mac OS X has important benefits:

*   **Portability**: One piece of code compiles and runs on both operating systems.
*   **Reliability**: The native audio APIs on each OS tend to be fairly bug-free.
*   **Cost**: Using the native platform APIs allows you to avoid shipping MP3 and AAC decoders (like ffmpeg and libmad) 
          with your application. Bundling such a decoder often requires royalty payments to the software patent holders. 
          Fortunately, Windows and Mac OS X already come with licensed decoders for applications to use instead, and
          so that's what libaudiodecoder wraps.

In more technical terms, we wrap the ExtAudioFile API (CoreAudio) on Mac OS X, and the Media Foundation API that's been part of Windows since Vista. Unfortunately, it turns out that Media Foundation only works for decoding audio in Windows 7 and greater. 


API at a Glance
===============


```c++
    class AudioDecoder {

        /** Construct a decoder for a given audio file.
            @param filename A UTF-8 string containing the full path to the audio file to open.
		*/
        AudioDecoder(const std::string filename);
        virtual ~AudioDecoder();

        /** Opens the file for decoding */
        int open();

        /** Seek to a sample in the file */
        int seek(int sampleIdx);

        /** Read a maximum of 'size' samples of audio into buffer. 
            Samples are always returned as 32-bit floats, with stereo interlacing.
            Returns the number of samples read. */
        int read(int size, const SAMPLE *buffer);

        /** Get the number of audio samples in the file. This will be a good estimate of the 
            number of samples you can get out of read(), though you should not rely on it
            being perfectly accurate always. (eg. it might be slightly inaccurate with VBR MP3s)*/
        inline int    numSamples()        const;

        /** Get the number of channels in the audio file */
        inline int    channels()          const;

        /** Get the sample rate of the audio file (samples per second) */
        inline int    sampleRate()        const;

        /** Get the duration of the audio file (seconds) */
        inline float  duration()          const;

        /** Get the current playback position in samples */
        inline int    positionInSamples() const;

        /** Get a list of the filetypes supported by the decoder, by extension */
        static std::vector<std::string> supportedFileExtensions()
    };
```

For the complete API, please see audiodecoderbase.h.
Please note that at present, all API calls are blocking and none are considered real-time safe. For best performance, please do not call read() or any other libaudiodecoder function from inside your audio callback.


Compatibility
=============

<table>
    <tr>
        <td></td>
        <td><b>Windows (MediaFoundation)</b></td>
        <td><b>Mac OS X (CoreAudio)</b></td>
    </tr>
    <tr>
        <td>MP3</td>
        <td>Yes<sup>*</sup></td>
        <td>Yes</td>
    </tr>
    <tr>
        <td>AAC (M4A)</td>
        <td>Yes<sup>*</sup></td>
        <td>Yes</td>
    </tr>
    <tr>
        <td>WMA</td>
        <td>Yes<sup>*</sup></td>
        <td>Yes</td>
    </tr>
    <tr>
        <td>WAVE (16-bit int)</td>
        <td>Yes</td>
        <td>Yes</td>
    </tr>
    <tr>
        <td>WAVE (24-bit, 32-bit int)</td>
        <td>No</td>
        <td>Yes</td>
    </tr>
</table>

\* Requires Windows 7+ or greater

If you require support for all the different types of WAVE files (different encodings, bit depths, etc.), check out [libsndfile](http://www.mega-nerd.com/libsndfile/). It should also be noted that DRM encrypted files are not supported on any platform.


Example Code
============

The "examples" directory currently contains *playsong*, which demonstrates how to decode an MP3 file with libaudiodecoder
and play out the soundcard with PortAudio. The example requires PortAudio and libaudiodecoder installed, and can be built
with the project files provided for XCode 3 or greater on Mac OS X and Visual Studio 2008 on Windows. Please see the
README is the playsong directory for more compilation instructions.

Compiling
=========

libaudiodecoder requires [SCons](http://www.scons.org) to build. To compile libaudiodecoder in debugging configuration, run:

    scons debug=1 

or for release configuration:

    scons debug=0
   
To install system-wide on Mac OS X (recommended), run:

    scons debug=0 install



API Stability Warning
=====================

libaudiodecoder was developed primarily with Windows and Mac OS X in mind. Because most application developers on these platforms build and ship their own 3rd party libraries with their products, API changes will be noticed at compile time or during testing. We make no guarantees about the stability of the API at this point, though it's not likely to change too much. 


String Encoding
===============

All strings in the API are assumed to be UTF-8 encoded. 


Authors
=======

libaudiodecoder was originally created by [RJ Ryan](http://rustyryan.net/), [Bill Good](https://github.com/bkgood), and [Albert Santoni](http://www.santoni.ca/albert) from code we wrote for [Mixxx](http://www.mixxx.org/ "Mixxx"). We later refactored the code and rolled it into a separate library for use in [BeatCleaver](http://www.oscillicious.com/beatcleaver/ "BeatCleaver") by [Oscillicious](http://www.oscillicious.com/ "Oscillicious"). By sharing this code, we hope other developers will benefit from it in the same way open source has helped us. 


License (MIT)
=============

Copyright (c) 2010-2012 Albert Santoni, Bill Good, RJ Ryan  

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.



The text above constitutes the entire libaudiodecoder license; however, 
the Oscillicious community also makes the following non-binding requests:

Any person wishing to distribute modifications to the Software is
requested to send the modifications to the original developer so that
they can be incorporated into the canonical version. It is also 
requested that these non-binding requests be included along with the 
license above.

