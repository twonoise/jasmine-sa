# jasmine-sa
Jasmine-SA is multichannel hi-res Spectrum Analyzer for X11 &amp; JACK (Linux 64bit)

HIGHLIGHTS
----------
* Closes the gap between <tt>fftw3</tt> library power and existing audio analyzers based on it, most of which are made for music, so looks a bit limited for measurements; one close to measurement precision is <tt>jaaa</tt>.
* No any UI deps, such as Qt or Gnome widgets, mouse, window managers, decorators, desktop stuff. It uses bare display and keyboard only, like one from golden era of instruments, HP 8563E. <br>
Note: Mouse still can be used for markers (added per request).
* No any tuning / scaling coef's, just bare theory implemented.
* It can be run without desktop, via <tt>xinit</tt>.
* Suitable for both small and large screens.
* Highly optimized for CPU usage.
* Background transparency for compare two spectrogram windows, or compare live spectrogram with saved picture used as substrate.
* Uses only X11 internal font guaranteed to be available, ASCII only, gives uniform look and metrics everywhere. Pixel-precise and predictable, allows sharp look for any screens & LED panels.
* Optional OpenGL, include both MSAA and Alpha transparency (TODO).
* Brightness (luma) inversion option, HSL color model based.
* Per-channel FFT windows, runtime selected.
* One C file, with minimal and most stable deps.

DESCRIPTION
-----------
Note that we have man page.

Please be familiar with DFT itself, and read at least [1], [2], [4].

There are at least two essentially different types of spectrum analyzers are widely used today.

1. Classic one (let me call it "real SA") is narrowband, highly selective radio frequency receiver with amplitude detector in place of speaker, which can "scan" some frequency band (called **Span**) and plot amplitude vs. frequency graph. Acquiring of one frequency (let's call it "point") include internal tuning oscillators settle time, and amplitude acquire (power collecting) time, then we ready for next frequency (point) in our band. The most important thing is when we need more frequency resolution, we not just finer frequency step, but also make our detector bandwidth (final lowpass filter before "speaker" or detector) to get narrower, which proportionally increases time of acquire of this frequency point, to keep SNR. This detector bandwidth is most fundamental thing of every spectrum analysis. It obviously can't be zero, because we then need infinite time to acquire this frequency point or "bin". This bandwidth is called _reference bandwidth_, or **RBW**, and for real SA, it is ~ 100 Hz to 1 MHz with 10x or 3x steps. LPF should be near rectangle shape; when this is impossible (low RBW, 100...1000 Hz), we a) measure its width by 3 dB (re:amplitude) attenuation point, and b) get extra error measuring noise floor, see below why.
It can be easily seen that with lower RBW and longer acquire time for each point, we can't measure amplitude of signal bursts shorter than our acquire time (while still often able to see if they are exist, at least, but if they are occurs when scanner is tuned to this frequency, of course). Operator of SA should select RBW to balance between burst sensitivity and frequency resolution. The fact that amplitude is not always measureable, or even louder, no any guarantee that collected points are represent some "real" value (because, at least, length of signal of this particular frequency is not known by SA), makes the name of instrument not Meter, but Analyzer: It gives (often very specific for human eye) picture, but still operator's deep knowledge required to interpret it. In particular, when measuring cursor ("Marker") displayed on graph, it should be accounted with care.
2. Digital one, or, most often, DFT(FFT)-based SA. We will omit combined type (radio frequency ones) of FFT SAs, and will talk only about baseband (down to 0 Hz) instruments, w/o frequency conversions, so it's basically an ADC as analog part, like PC sound card.
FFT SA keep all properties and limitations of real SA exactly, except that they add one more step of deep of understanding. There is fundamental difference in noise floor measurement between real SA and FFT SA. Real SA does not require special menu or operator knowledge beyond classic noise floor theory. FFT SA are uses special windowing function. It is essential thing across FFT SAs, and it defines many aspects like precision of amplitude and frequency, and dynamic range. Well known FFT windows are have < 90 dB dynamic range, limited by side lobes of pulse responce of windowing function; so we'll need for HFT144D [1] or so for more dynamic. Any window, except no-window aka rectangle/unity window, have important property: it collect not only exact signal power, if any, but also some adjacent point powers; they all will be summed, and if these points are mostly noise, we will have well defined noise amplification of this particular windowing function. In other words: each point on FFT plot is really some combination (weighted sum) of few adjacent points. It is like if we use not rectangular but trapezoid-like filter in real SA; it is fundamentally impossible to have rectangular one in windowed FFT SA. Stop: it is important here to understand why only noise are amplificated, but not tones. The more dynamic range window have, the more noise amplification. This is unavoidable, so to keep the possibility to measure noise, we should have run time switch to de-embed noise factor from measured power in our bins, and operator is responsive to switch it between measure tones (graph "peaks") and noise floor. When no this switch, we still can de-embed by hand, but only if we know windowing function noise amplification (i call it window noise figure, NF). It is known for all windows [3] [1]. Note that it's true only for self-uncorrelated (pure or thermal) noise.

Other problem (for all type of SA, but mostly for FFT SA due to way higher amount of result data/points available) is what to do if we want to plot more points than our X axis allows. The obvious downscale interpolation will not work here, as no physical meaning of it: we will see good picture, but won't be able to measure anything on it (in fact, we can measure noise, but only in case we have integer relation between FFT points quantity for current span and X axis points, i.e. equal FFT points in each X bin).

The correct approach is to use MAX function if we measure signals, and AVG function if we measure noise (please also check [1]:p.17). As there is no "perfect" display approach for FFT SA, operator should be familiar with run time display mode selection. It often comblied with Tone-Noise measurement mode switch, mentioned above; we also do that in our code.

Do not wonder if you can't find peaks you expect: it's probably Noise measurement (AVG) mode, when peaks will be reduced or lost. Opposite for that, Signal measurement (MAX) mode guarantees that peaks will be visible, and their amplitudes will be "correct" (correct if not short bursts/fast changes, see above). Btw, bursts are have way more chances to be catched with FFT SA, due to it "sees" all points in parallel, while real SA is sequental.

Important property of our FFT SA is X axis (horizontal) zoom, or span, is wide range. As it gives high frequency resolution, it requires way larger FFT transform sizes than usual; note that such input data collection takes some time. If we "zoom-in" (reduce span) for more than 1 resulting FFT point per X axis pixel, our measurement cursor (Marker) becomes exact frequency and amplitude (power), and there will be no MAX or AVG or other altering of results, except window noise floor de-embedding if we have Noise (AVG) measurement mode. Note that we have dynamic FFT size for such a wide range of spans, but max size still can be given using command line (depends of CPU).

Some other controls
-------------------

_Video bandwidth_, or **VBW**, can be limited for some cases like stable noise floor measure. First, we setup all things (frequency, etc) and see if we have expected picture; then, we limit VBW to filter it, and wait a lot more for stable picture. There are two types of VBW limiters. For real SA, it is often multi-point averaging (imagine an LPF at CRT ray Y deflection input). For FFT SA, there is also independent (per-point) filter often used; they also can be combined. We use only per-point one in our code yet. Note that VBW is CRT trace run time thing, it will not used when Stopped state.

_Y scale_ can be shifted up and down, and scale can be selected to be dB re:voltage, or dB re:power. Most baseband VU meters, like for speech or music, are use voltage, so we have it as default. dBPwr stretch Y scale two times, and also can be used to for more precise visual measurements.

We have run time per-channel (as well as for all channels at same time) _selection of windowing functions_ (there are four currently) and _MAX (Tone) vs AVG (Noise)_ measurements. Feeding one input signal to several channels, we can see all these results at one screen. Tab key used to jump marker between channels.

We have auto-selected _roll_ (_sliding window_) which allows to FFT and plot some (up to like 32) intermediate results before full FFT size of data will be gathered (which can be quite slow for low Fs and large FFT sizes); upper progress bar is one FFT size. Extra effect of roll is we can catch some bursts, which are falls on time line at edges of windowing function, so much attenuated. We can't follow per-window recommended values [1], due to we use different windows at same time.

We have per-channel _statistics_ of input signal(s). It show min and max values with auto reset on each screen. It also have **z** value, which i define as <tt>log2(abs(minNZ)) - 1</tt>, where _minNZ_ is minimal (most close to zero) but non-zero value so far, and _1_ is extra sign bit. I define its dimension unit as bits, and it, like ENOB, can be non-integer. **z** statistics are resets only manually (Esc hit, or Instrument reset). **z** value gives an idea how fine-grained incoming samples are.
 * _Example 1_: correct tuning of alsamixer (input signal, i.e. line input) is max value of slider, when **z** = -24.
 * _Example 2_: ENOB signal source, which is unity sine with artificially coarsed (LSB bits cut) samples, here **z** will show this remaining (not cutted) bits quantity. <br>
Note that time is required for **z** to settle. 

Btw, z is not ENOB, while they can match exactly (like for ENOB calibration sources). Z can be less (deeper, or more in its abs value) than ENOB, like for input white noise of sound card, when z = -24 and ENOB ~13; but ENOB can't be better (more) than z in its abs value.

Control
-------
* Keyboard: F1...F10, alphanumeric, +, - (PgUp, PgDn) and Cursor keys. 
* Mouse: Absolute (left) and Relative (right button) markers; wheel as keyboard +, -.

Code
----
Our C code is intended to be modified by operator and also asts as part of documentation, so it made as simple and clean as i can. Please add your own windowing functions, etc.

ENOB MEASUREMENT
----------------
We have special -e key to show _effecive number of bits_ (**ENOB**) of input signal. It show some labels on right Y scale. It also switch default measure mode to AVG (noise) for all channels. ENOB should be measured with all care of noise measurement. Extra calibrated noise sources can be feed to extra channels to check if ENOB scale is correct.

Note that ENOB defined like SINAD with one tone with (near-)unity (max) amplitude. ENOB is not just silence input noise of ADC: it will be worse than that, and it shows why just input noise floor measurement is way less useful than ENOB (which is "real" noise). Tip: Use stats in our SA to see if our ADC samples are near unity, but never overloaded.

Requirement of unity tone for ENOB measurement is directly requires spectrum analysis, because, unlike of just SNR measurement, it can't be measured using oscilloscope or VU meter as normal S+N to N radio.

Important for noise measurement. Please read throughly [1]:p.22. Quantizing noise we generate with ENOB test source, _should_ be running on screen, not stable (like when all freqs are perfect, orthogonal-to-FFT-size) due to this noise behaves as signal in at least two terms:

1.  It is stop to reduce when FFT size increase (RBW decrease) and MAX measure method.
2.  It is reduces by 6 dB per FFT size 2x increase step, while should by 3 dB, when AVG measure method.

_Perfect noise_ sine source with ~25 ENOB. The problem with it is exact ENOB of it is not known: it is quantized by float samples (JACK is float), but not by exact bit limiter:

    gst-launch-1.0 audiotestsrc freq=749.999 volume=1.0 ! audio/x-raw,channels=2 ! jackaudiosink
    
_Bad noise_ sine source is same but with <tt>freq=750</tt> (when JACK Fs is 48 kHz or multiple of it; check it with <tt>jack_samplerate</tt>).

Quantizing noise sine source can be not only in form of C code as per [1], but also in form of Faust plugin (i use this for real calibrations):

    //  See my `enobsrc.dsp`, here is math.
    //  Compile using: `faust2lv2 -double ./enobsrc.dsp`
    import("stdfaust.lib");
    signal = os.osci(100.071);
    bits = nentry("Bits", 16, 8, 31, 1) : int;
    signBit = 1;
    quant = 1.0 / (1 << (bits - signBit));
    micro = 1.0 / (1000*1000);
    process =
      ( signal <:
        (int(_ / quant + 0.5) * quant <: _ * (1 - micro), _ * micro),
        (_ <: _ * (1 - micro), _ * micro)
      ),
      ( bits : vbargraph("Bits Check [CV:0]", 8, 32) ),
      ( quant : ba.linear2db : vbargraph("Quant dB [CV:1]", -240, 0) )
    ;
    
Note that, unlike direct-type ADC, most PC sound cards, as well as other not-so-expensive (sigma-delta) ADC, are have _noise shaping_ which push out noise from audible band to upper band, so it's worth to measure ENOB not only below 20 kHz (which is default band for our SA), but also for full band. Noise shaping can depend of sample rate.

ABOVE 192 kS/s?
---------------
JACK engine not only allows to go above 192 kS/s, but also does it at realtime, that means even if our ADC or DAC can't go so high, we still have complete and solid solution, with 1:N resampler at i/o. Inside the i/o borders to outer world (ADC & DAC), our lv2 plugins, as well as all other DSP, will work at this "radio" frequency, which allows to use "radio" math like IF-LO-RF mixers, etc, and samplerate will be auto decimated on i/o only. Note that JACK never altering our samples on road. This is best possible, defined, all-RT setup for precision science i see so far.

Start JACK with **plug:hw:PCH** but not normal **hw:PCH**, like:

    jackd -dalsa -dplug:hw:PCH -r384000 -p4096 -n3 -Xraw
    
then test it:

    jack_samplerate

We then can start our SA and see noise floor of real i/o and other things above 192 kS/s, which is typically impossible, and while upper half can't show us new tones, it can show how resampler and codec filters things; it also prepare us to have more than 192k i/o, we know it will work and displayed as expected, and we can estimate if we have enough CPU horsepower for that. But most good news that our "radio" DSPs (w/radio mixers... etc), inside of i/o boundary, will be plotted exactly as should.

Q & A
-----

_Where is log scale?_
---------------------
We have log (dB) Y scale. As for X scale, it is linear only. While most audio(music)-centric analyzers are often have log X scale, this SA is not (only) musical, but more like scientific, these instrumensts often try to avoid log X scale, except phase noise measurements:

* There is no zero/negative frequencies allowed;
* And, there is no measurements are possible, only estimations, as no Exact, Max, or Avg, display method for entire scale is useful.
    
But feel free to implement log scale yourself: our code is _intended_ for user enhancements.

_How about inter-bin interpolation?_
------------------------------------
I can't find or estimate noise specs of these Time-Frequency Reassigned Spectrogram transforms [7] [8]. Without that, we can't make useful measurements. Btw, there are other SA where it is implemented, like x42 [9].

_I hate your pixels._
---------------------
There are **texels**.<br>
And we have full openGL'ed GPU-driven antialiasing (MSAA). <br>
But, yes, there is a bug for Intel GPU (or driver?) which blocks both **MSAA** and **Alpha** (**transparency**) at same time [5]. 


TESTING
-------
<small>TODO Move it to man page.</small>

Test for memory leaks. Note that either _all_ openGL apps have some leaks in order of 200...300 kb (_i talk about Linux only_); or, there are `valgrind` false starts. [6] (It is so-so everywhere, still no exact answer).<br>
So i prepare some filters.

    gcc -ggdb3 -Wall -lm -ljack -lX11 -lXrender -lXss -lGL -lfftw3_threads -lfftw3 -o jasmine-sa ./jasmine-sa.c && echo -e '{\n1\nMemcheck:Leak\n...\nsrc:dl-open.c:874\n}\n{\n2\nMemcheck:Leak\n...\nsrc:dl-init.c:121\n}\n' > /tmp/s && valgrind --leak-check=full --show-leak-kinds=all --suppressions=/tmp/s ./jasmine-sa -k 16 system:capture_1 -e -O -M 0 -A 1 -o 0
    

CREDITS
-------
* [1] https://holometer.fnal.gov/GH_FFT.pdf
* [2] https://kluedo.ub.rptu.de/frontdoor/deliver/index/docId/4293/file/exact_fft_measurements.pdf
* [3] https://www.gaussianwaves.com/2020/09/equivalent-noise-bandwidth-enbw-of-window-functions/
* [4] Analog Devices MT-001.pdf
* [5] https://stackoverflow.com/questions/79065474/transparent-background-and-msaa-at-same-time-for-opengl-window-on-x11
* [6] https://gitlab.freedesktop.org/mesa/mesa/-/issues/11275
* [7] https://hal.science/hal-00414583/document
* [8] https://people.ece.cornell.edu/land/PROJECTS/ReassignFFT/index.html
* [9] https://github.com/x42/spectra.lv2
* Credits for C code are shown at tail of C file.


LICENSE
-------
This manual for Jasmine-SA, as well as man page for Jasmine-SA, are licensed under Creative Commons Attribution 4.0. You are welcome to contribute to the manual in order to improve it so long as your contributions are made available under this same license.

Jasmine-SA is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
