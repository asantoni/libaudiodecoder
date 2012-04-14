libaudiodecoder
===============

The Native Audio Playback API
---------------

libaudiodecoder provides a common interface for low-level audio file playback on Windows, Mac OS X, and hopefully
more platforms in the future. Wrapping the audio playback APIs provided by Windows and Mac OS X offers
some important advantages:

*   **Portability**: One piece of code compiles and runs on both operating systems.
*   **Reliability**: The native audio APIs on each OS tend to be fairly bug-free.
*   **Cost**: Using the native platform APIs allows you to avoid shipping MP3 and AAC decoders (like ffmpeg) 
          with your application. Bundling such a decoder often requires royalty payments to the software 
          patent holders. Fortunately, Windows and Mac OS X already come with licensed decoders, and
          that's what libaudiodecoder is based on.

In more technical terms, we wrap the ExtAudioFile API (CoreAudio) on Mac OS X, and the Media Foundation API that's been part of Windows since Vista. Unfortunately, Media Foundation only works for decoding audio in Windows 7 and greater. 


API at a Glance
===============


    ```c++
    class AudioDecoder {

        /** Construct a decoder for a given audio file. */
        AudioDecoder(const std::string filename);
        virtual ~AudioDecoder();

        /** Opens the file for decoding */
        int open();

        /** Seek to a sample in the file */
        int seek(int sampleIdx);

        /** Read a maximum of 'size' samples of audio into buffer. 
            Returns the number of samples read. */
        int read(int size, const SAMPLE *buffer);

        /** Get the number of samples in the audio file */
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
        <td>Yes<sup>*</sup>*</td>
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
        <td>WAVE (16-bit int)/td>
        <td>Yes</td>
        <td>Yes</td>
    </tr>
    <tr>
        <td>WAVE (24-bit, 32-bit int)/td>
        <td>No</td>
        <td>Yes</td>
    </tr>
</table>

* Requires Windows 7+ or greater

If you require support for all the different types of WAVE files (different encodings, bit depths, etc.), check out libsndfile.  


API Stability Warning
=====================

libaudiodecoder was developed primarily with Windows and Mac OS X in mind. Because most application developers on these platforms build and ship their own 3rd party libraries with their products, API changes will be noticed at compile time or during testing. We make no guarantees about the stability of the API at this point, though it's not likely to change too much. 

Authors
=======

libaudiodecoder was originally created by RJ Ryan, Bill Good, and Albert Santoni from code we wrote for [Mixxx](http://www.mixxx.org/ "Mixxx"). Albert later refactored the code and rolled it into a separate library for use in [BeatCleaver](http://www.oscillicious.com/beatcleaver/ "BeatCleaver") by [Oscillicious](http://www.oscillicious.com/ "Oscillicious"). By sharing this code, we hope other developers will benefit from it in the same way open source has helped us. 


