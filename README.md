libaudiodecoder
===============

The Native Audio Playback API

libaudiodecoder provides a common interface for audio file playback on Windows, Mac OS X, and hopefully
more platforms in the future. Wrapping the audio playback APIs provided by Windows and Mac OS X offers
some important advantages:
    * Portability: One piece of code compiles and runs on both operating systems.
    * Reliability: The native audio APIs on each OS tend to be fairly bug-free.
    * Cost: Using the native platform APIs allows you to avoid shipping MP3 and AAC decoders (like ffmpeg) 
            with your application. Bundling such a decoder often requires royalty payments to the software 
            patent holders. Fortunately, Windows and Mac OS X already come with licensed decoders, and
            that's what libaudiodecoder is based on.

In more technical terms, we wrap the ExtAudioFile API (CoreAudio) on Mac OS X, and the Media Foundation
API that's been part of Windows since Vista. 


Open design questions:
- Should we always return stereo audio?
    - if so, then what is the point of channels()?
- Try typedefing SAMPLE as short, make sure everything works, and then make it a scons flag.



API FAQ
===============

Q. Why is this API not file-oriented? (Why is there no "AudioFile" class?)

A. 
While that may be simpler to use for small programs (and even appropriate), we found that that
design doesn't scale. In a larger audio program, you may have dozens of audio files that you
want to stream from disk, [ Does this even make sense? you could have a single reader thread that 
has a queue of AudioFiles and avoids disk contention ] 


