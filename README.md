# jasmine-sa
Jasmine-SA is multichannel hi-res Spectrum Analyzer for X11 &amp; JACK (Linux 64bit)

HIGHLIGHTS
----------
* Closes the gap between <tt>fftw3</tt> library power and existing audio analyzers based on it, most of which are made for music, so looks a bit limited for measurements; one close to measurement precision is <tt>jaaa</tt>.
* No any UI deps, such as expiring each few years Qt or Gnome widgets. Not depends on (but feels well with any of) mouse, window managers, decorators, desktop things. It uses bare display and keyboard only, like one from golden era of instruments, HP 8563E. <br>
_Note_: Mouse still can be used for markers (added per request).
* No any tuning factors, just bare theory implemented as is.
* Highly customizable window behaviour, size, and colors.
* It can be run without desktop, via <tt>xinit</tt>.
* Suitable for both small and large screens.
* Highly optimized for CPU usage.
* Background transparency for compare two spectrogram windows, or compare live spectrogram with saved picture used as substrate.
* Uses only X11 internal font guaranteed to be available, ASCII only, gives uniform look and metrics everywhere. Pixel-precise and predictable, allows sharp look for any screens & discrete LED panels, papers and printed books.
* Optional OpenGL, include both MSAA and Alpha transparency (_Testing_).
* Brightness (luma) inversion option, HSL color model based.
* Per-channel FFT windows, runtime selected.
* One C file, with minimal and most stable deps.

DESCRIPTION
-----------
_Note that we have man page._

Please be familiar with DFT itself, and read at least [1], [2], [4].

Oscilloscopes are well known, and simplest possible one can be just LED on few ft. wire, rotating in your hand in dark room. Unlike good oscilloscope, good spectrum analyzer is a bit more complex (in terms of both usage and internals) and expensive unit, so not every desktop test setup include it. But spectrum analyzer is way more sensitive sometimes, and have other unique properties, so it's often worth to have it; while it requires that operator have some experience of thinking in frequency domain.

There are at least two essentially different types of spectrum analyzers are widely used today.

1. Classic one (let me call it "real SA") is narrowband, highly selective radio frequency receiver with amplitude detector in place of speaker, which can "scan" some frequency band (called **Span**) and plot amplitude vs. frequency graph. Acquiring of one frequency (let's call it "point") include internal tuning oscillators settle time, and amplitude acquire (power collecting) time, then we ready for next frequency (point) in our band. The most important thing is when we need more frequency resolution, we not just finer frequency step, but also make our detector bandwidth (final lowpass filter before "speaker" or detector) to get narrower, which proportionally increases time of acquire of this frequency point, to keep SNR. This detector bandwidth is most fundamental thing of every spectrum analysis. It obviously can't be zero, because we then need infinite time to acquire this frequency point or "bin". This bandwidth is called _reference bandwidth_, or **RBW**, and for real SA, it is ~ 100 Hz to 1 MHz with 10x or 3x steps. LPF should be near rectangle shape; when this is impossible (low RBW, 100...1000 Hz), we a) measure its width by 3 dB (re:amplitude) attenuation point, and b) get extra error measuring noise floor, see below why. <br>
It can be easily seen that with lower RBW and longer acquire time for each point, we can't measure amplitude of signal bursts shorter than our acquire time (while still often able to see if they are exist, at least, but if they are occurs when scanner is tuned to this frequency, of course). Operator of SA should select RBW to balance between burst sensitivity and frequency resolution. The fact that amplitude is not always measureable, or even louder, no any guarantee that collected points are represent some "real" value (because, at least, length of signal of this particular frequency is not known by SA), makes the name of instrument not Meter, but Analyzer: It gives (often very specific for human eye) picture, but still operator's deep knowledge required to interpret it. In particular, when measuring cursor ("Marker") displayed on graph, it should be accounted with care.
2. Digital one, or, most often, DFT(FFT)-based SA. We will omit combined type (radio frequency ones) of FFT SAs, and will talk only about baseband (down to 0 Hz) instruments, w/o frequency conversions, so it's basically an ADC as analog part, like PC sound card.
FFT SA keep all properties and limitations of real SA exactly, except that they add one more step of deep of understanding. There is fundamental difference in noise floor measurement between real SA and FFT SA. Real SA does not require special menu or operator knowledge beyond classic noise floor theory. FFT SA are uses special windowing function. It is essential thing across FFT SAs, and it defines many aspects like precision of amplitude and frequency, and dynamic range. Well known FFT windows are have < 90 dB dynamic range, limited by side lobes of pulse responce of windowing function; so we'll need for HFT144D [1] or so for more dynamic. Any window, except no-window aka rectangle/unity window, have important property: it collect not only exact signal power, if any, but also some adjacent point powers; they all will be summed, and if these points are mostly noise, we will have well defined _noise amplification_ of this particular windowing function. In other words: each point on FFT plot is really some combination (weighted sum) of few adjacent points. It is like if we use not rectangular but trapezoid-like filter in real SA; it is fundamentally impossible to have rectangular one in windowed FFT SA. <br>
_Stop_: it is important here to understand why only noise are amplified, but not tones.<br>
 The more dynamic range window have, the more noise amplification. This is unavoidable, so to keep the possibility to measure noise, we should have run time switch to de-embed noise factor from measured power in our bins, and operator is responsive to switch it between measure tones (graph "peaks") and noise floor. When no this switch, we still can de-embed by hand, but only if we know windowing function noise amplification (i call it window noise figure, NF). It is known for all windows [3] [1]. Note that it's true only for self-uncorrelated (pure or thermal) noise.

Other problem (for all type of SA, but mostly for FFT SA due to way higher amount of result data/points available) is what to do if we want to plot more points than our X axis allows. The obvious downscale interpolation will not work here, as no physical meaning of it: we will see good picture, but won't be able to measure anything on it (in fact, we can measure noise, but only in case we have integer relation between FFT points quantity for current span and X axis points, i.e. equal FFT points in each X bin).

The correct approach is to use MAX function if we measure signals, and AVG function if we measure noise (please also check [1]:p.17). As there is no "perfect" display approach for FFT SA, operator should be familiar with run time display mode selection. It often combined with Tone-Noise measurement mode switch, mentioned above; we also do that in our code.

Do not wonder if you can't find peaks you expect: it's probably Noise measurement (AVG) mode, when peaks will be reduced or lost. Opposite for that, Tone measurement (MAX) mode guarantees that peaks will be visible, and their amplitudes will be "correct" (correct if not short bursts/fast changes, see above). Btw, bursts are have way more chances to be catched with FFT SA, due to it "sees" all points in parallel, while real SA is sequental.

Important property of our FFT SA is X axis (horizontal) zoom, or span, is wide range. As it gives high frequency resolution, it requires way larger FFT transform sizes than usual; note that such input data collection takes some time. If we "zoom-in" (reduce span) for more than 1 resulting FFT point per X axis pixel, our measurement cursor (Marker) becomes exact frequency and amplitude (power), and there will be no MAX or AVG or other altering of results, except window noise floor de-embedding if we have Noise (AVG) measurement mode. Note that we have dynamic FFT size for such a wide range of spans, but max size still can be given using command line (depends of CPU).

Some other controls
-------------------

_Video bandwidth_, or **VBW**, can be limited for some cases like stable noise floor measure. First, we setup all things (frequency, etc) and see if we have expected picture; then, we limit VBW to filter it, and wait a lot more for stable picture. There are two types of VBW limiters. For real SA, it is often multi-point averaging (imagine an LPF at CRT ray Y deflection input). For FFT SA, there is also independent (per-point) filter often used; they also can be combined. We use only per-point one in our code yet. Note that VBW is CRT trace run time thing, it will not used when Stopped state. <br>
By either pressing key twice, or, going with `+`, `-` control to above VBW maximum, we got **Max hold** mode. Note that, to measure with markers on it, we'll need to have _Tone (MAX)_ but not _Noise (AVG)_ measurement mode.

_Y scale_ can be shifted up and down, and scale can be selected to be **dB re:voltage**, or **dB re:power**. Most baseband VU meters, like for speech or music, are use voltage, so we have it as default. dBPwr stretch Y scale two times, and also can be used to for more precise visual measurements.

We have run time per-channel (as well as for all channels at same time) _selection of windowing functions_ (there are four currently) and _MAX (Tone) vs AVG (Noise)_ measurements. Feeding one input signal to several channels, we can see all these results at one screen. Tab key used to jump marker between channels.

We have auto-selected _roll_ (_sliding window_) which allows to FFT and plot some (up to like 32) intermediate results before full FFT size of data will be gathered (which can be quite slow for low Fs and large FFT sizes); upper progress bar is one FFT size. Extra effect of roll is we can catch some bursts, which are falls on time line at edges of windowing function, so much attenuated. We can't follow per-window recommended values [1], due to we use different windows at same time.

We have per-channel _statistics_ of input signal(s). It show min and max values with auto reset on each screen. It also have **z** value, which i define as <tt>log2(abs(minNZ)) - 1</tt>, where _minNZ_ is minimal (most close to zero) but non-zero value so far, and _1_ is extra sign bit. I define its dimension unit as bits, and it, like ENOB, can be non-integer. **z** statistics are resets only manually (Esc hit, or Instrument reset). **z** value gives an idea how fine-grained incoming samples are.
 * _Example 1_: correct tuning of alsamixer (input signal, i.e. line input) is max value of slider, when **z** = -24.
 * _Example 2_: ENOB signal source, which is unity sine with artificially coarsed (LSB bits cut) samples, here **z** will show this remaining (not cutted) bits quantity. <br>
Note that time is required for **z** to settle. 

Btw, z is not ENOB, while they can match exactly (like for ENOB calibration sources). Z can be less (deeper, or more in its abs value) than ENOB, like for input white noise of sound card, when z = -24 and ENOB ~13; but ENOB can't be better (more) than z in its abs value.

Hi-DPI aka Window (up)scaling
-----------------------------
* For _pure X11_, we can set lines, points, and plot (and so, window) sizes, but not font size.
* For _openGL_, in addition to it, we also have scaling. There is better if scale factor is integer (_N_ * 100%).<br>
We have two options:
  * One is well known, and acts like `QT_SCALE_FACTOR=N` (_we do not have Qt, so just an example_). It works as picture upscaling, except that vector-based (scalable) elements of window are redrawn with full resolution. <br>
_Note_ that in this case, we got nice picture, but do not increase "real" resolution (like X axis frequency points quantity).
  * Font only scaling. Other elements are remaining under regular size options control. When operator scales both font and plot dimensions via their own controls, we get result looks similar to above, yet have benefit of increased real video resolution. <br>
_Note_: Spectrogram lines and points width are also scaled with font scale (along with they have its own control).

Control
-------
* _Keyboard_: <br>
 `Esc`, `F1`...`F10`, alphanumeric, `+`, `-` (`PgUp`, `PgDn`) and Cursor keys; <br>
 `h`, `k`, `m` for frequency units entry;<br>
 `x` for exit.
* _Mouse_: <br>
 Absolute (`left`) and Relative (`right` button) markers; `wheel` as keyboard `+`, `-`.

Code
----
Our C code is intended to be modified by operator and also asts as part of documentation, so i've made it as simple and clean as i can. Please add your own windowing functions, etc.

ENOB MEASUREMENT
----------------
We have special `-e` key to show _effecive number of bits_ (**ENOB**) of input signal. It show some labels on right Y scale. It also switch default measure mode to AVG (noise) for all channels. ENOB should be measured with all care of noise measurement. Extra calibrated noise sources can be feed to extra channels to check if ENOB scale is correct.

Note that ENOB defined like SINAD with one tone with (near-)unity (max) amplitude. ENOB is not just silence input noise of ADC: it will be worse than that, and it shows why just input noise floor measurement is way less useful than ENOB (which is "real" noise). Tip: Use stats in our SA to see if our ADC samples are near unity, but never overloaded.

Requirement of unity tone for ENOB measurement is directly requires spectrum analysis, because, unlike of just SNR measurement, it can't be measured using oscilloscope or VU meter as normal S+N to N radio.

Important for noise measurement. Please read throughly [1]:p.22. Quantizing noise we generate with ENOB test source, _should_ be running on screen, not stable (like when all freqs are perfect, orthogonal-to-FFT-size) due to this noise behaves as signal in at least two terms:

1.  It is stop to reduce when FFT size increase (RBW decrease) and MAX measure method.
2.  It is reduces by 6 dB per FFT size 2x increase step, while should by 3 dB, when AVG measure method.

Calibration sources
-------------------

_Perfect noise_ sine source with ~25 ENOB. The problem with it is exact ENOB of it is not known: it is quantized by float samples (JACK is float), but not by exact bit limiter:

> This command will work correctly even [above 192 kS/s](https://github.com/twonoise/jasmine-sa/?tab=readme-ov-file#above-192-kss).

    gst-launch-1.0 audiotestsrc freq=749.999 volume=1.0 ! audio/x-raw,channels=2 ! jackaudiosink
    
The ENOB of 32-bit float obviously defined and should be well known, but no any paper about it. Trying to estimate it myself, gives value of 24.6535, which is differ from ~25 i get during calibrated measurements:

<details> 
 <summary>[C code]</summary>
 
    // Prints effective number of bits of ideal sine, possible using float. 
    // Ideal means unity amplitude and orthogonal to wavetable buffer used (for Faust it's 65536 samples).
	   void print_enob_for_sine_float (void)
    {
		   double result = 0;
		   for (int i = 0; i < 23; i++) // Float have 23 bits of mantissa
			   result = result + (asin(1.0/(1 << i)) - asin(1.0/(1 << (i+1)))) * (24 + i);
		   result = result / (M_PI / 2.0);
		   printf("%f\n", result); // prints 24.6535
	   }
    
</details>

_Bad noise_ sine source is same but with <tt>freq=750</tt> (when JACK Fs is 48 kHz or multiple of it; check it with <tt>jack_samplerate</tt>).

Quantizing noise sine source can be not only in form of C code as per [1], but also in form of Faust plugin (i use this for real calibrations):

> This may not work correctly [above 192 kS/s](https://github.com/twonoise/jasmine-sa/?tab=readme-ov-file#above-192-kss). Some tune-up of **Faust** may be need:
> 
>     sed -i 's/192000/384000/' /usr/share/faust/platform.lib
>     sed -i 's/192000/384000/' /usr/share/faust/math.lib 

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

BUILD
-----
Pre-requisites for **Debian**:

    sudo apt install jackd libjack-dev libbsd-dev libfftw3-dev libx11-dev libxrender-dev libxss-dev libgl-dev libglx-dev

Pre-requisites for **Archlinux**:

Have regular system with some JACK, x11 and openGL apps.

Pre-requisites for **Slackware**:

Add `-pthread` key.

**Compile**:
(_add_ `-fopt-info-vec-optimized` _key for some SIMD info_)

    gcc ./jasmine-sa.c -Wshadow -Wall -g -O3 -ffast-math -march=native -lm -lbsd -ljack -lX11 -lXrender -lXss -lGL -lfftw3_threads -lfftw3 -o jasmine-sa


EXAMPLES
--------
> _New users_: We assume that JACK sound engine (`jackd`) started already, and you know how to operate with it. If not, it will be started automatically for you, but sample rate can be low. _Tip_: try `qjackctl`.

<br>

From as simple as (add `-e` for ENOB scale)...

    jasmine-sa system:capture_1 system:capture_2
    
![jasmine-sa-1](https://github.com/user-attachments/assets/4216214d-b353-47db-ae77-129d0bc2e982)

...To smooth transparent overlay bar on given screen place on your DAW, with tiny CPU usage (will also work as _JACK application_ with **[Carla](https://github.com/twonoise/carla-patches)**)<br>
_Tip_: See man page, or help using `-?`, to find these numbers meaning.

    jasmine-sa system:mic -O -A 1 -M 4 -o 0 -d -60,0,2,32 -h 0,2500,10,32 -p 4 -z -q CCEE -u 4 -x 100 -y 200 -b 31

![jasmine-sa-2](https://github.com/user-attachments/assets/6c49736b-11a5-49a0-9da0-f0903bbf3b32)

![jasmine-sa-3](https://github.com/user-attachments/assets/f42e1438-ba00-4472-bd11-84dfb9a6e99b)
  

ABOVE 192 kS/s?
---------------
JACK engine not only allows to go above 192 kS/s, but also does it at realtime, that means even if our ADC or DAC can't go so high, we still have complete and solid solution, with 1:N resampler at i/o. Inside the i/o borders to outer world (ADC & DAC), our lv2 plugins, as well as all other DSP, will work at this "radio" frequency, which allows to use "radio" math like [IF-LO-RF mixers](https://github.com/twonoise/rfclipper), etc, and samplerate will be auto decimated on i/o only. Note that JACK never altering our samples on road. This is best possible, defined, all-RT setup for precision science i see so far.

Start JACK with **plug:hw:PCH** but not normal **hw:PCH**, like:

    jackd -dalsa -dplug:hw:PCH -r384000 -p4096 -n3 -Xraw
    
then test it:

    jack_samplerate

We then can start our SA and see noise floor of real i/o and other things above 192 kS/s, which is typically impossible, and while upper half can't show us new tones, it can show how resampler and codec filters things; it also prepare us to have more than 192k i/o, we know it will work and displayed as expected, and we can estimate if we have enough CPU horsepower for that. But most good news that our ["radio" DSPs](https://github.com/twonoise/rfclipper/blob/main/rfclipper.dsp) (w/radio mixers... etc), inside of i/o boundary, will be plotted [exactly as should](https://github.com/user-attachments/assets/b6a5c5b3-bd96-4283-9687-64532751188f).

Q & A
-----

_Where is log scale?_
---------------------
We have log (dB) Y scale. As for X (frequency) scale, it is linear only. <br>
Most audio(music)-centric analyzers are often have log X scale, which is good choice when we try to fit entire audible band onto one screen (i.e., there are no shift nor zoom-in for X scale) yet need it to be reasonably informative (19000 to 20000 Hz difference is not of same importance that 20 to 1020 Hz jump). <br>
This SA is not (only) musical, but more like scientific, these instrumensts often try to avoid log X scale, except phase noise measurements:

* There is no zero/negative frequencies allowed;
* And, there is no measurements are possible, only estimations, as no Exact, Max, or Avg, display method for entire scale is useful.
    
But feel free to implement log scale yourself: our code is _intended_ for user enhancements.

_How do i move non-decorated window?_
-------------------------------------
When no caption nor border mode selected, _but_ no locked position mode, window is moveable as usual: 
* either `Alt+Left mouse button drag` using any place inside of window;
* or, Window menu (`Alt+Space`, or `Right click` on it on taskbar), then **Move**. I.e., this way it is possible without mouse.

_How about inter-bin interpolation?_
------------------------------------
I can't find or estimate noise specs of these Time-Frequency Reassigned Spectrogram transforms [7] [8]. Without that, we can't make useful measurements. Btw, there are other SA where it is implemented, like x42 [9].

If you need more frequency resolution than ~ 0.2 Hz/point (which is max for default max FFT size and 96 kHz Fs), you may increase max FFT size (depends on CPU).

_State save?_
-------------
We do not have it, and not planned. Rather, we have rich command line options set, with most if not all aspects highly configurable.

_Offline or file support?_
------------------------
There is no, currently.
* For _incoming_ data array, it can be "played" as `.wav` file, then feed to us as usual. Incoming samples often are 32-bit `float`s, and better if in range **[-1.0 ... 1.0]**. But can be any other "playable" data, if it can be "played" with some JACK audio player without distortion. Multi-channel audio files are almost always contains _interleaved_ data.<br>
Note that **JACK Transport** allow us to sync several players, so we get multiple incoming channels with zero skew guaranteed. Good if we have several _non-interleaved_ data sets.
* As for _resulting_ FFT data, there are last 16 screens are holded in screen memory; it should be easy to dump it to binary file, please feel free to implement this command. Data format is signed shorts (Q1.15) with **-327.67** to **327.67 dB** range, and **-32768** value is NODATA (out of bounds / non existent, etc).

_I hate your pixels :-[_
---------------------
There are **texels**.<br>
And we have full openGL'ed GPU-driven antialiasing (MSAA). <br>
But, yes, there is a "bug" for Intel GPU (or driver?) which blocks both **MSAA** and **Alpha** (**transparency**) at same time [5] [5a]. <br>
P.S. _Added_: AMD integrated GPU was tested. It (or driver?) also have this "bug". So, currently, both mainstream _integrated_ GPUs are affected.


TESTING
-------
_See also man page._

Test for memory leaks. Note that either _all_ openGL apps have some leaks in order of 200...300 kb (_i talk about Linux only_); or, there are `valgrind` false starts. [6] (It is so-so everywhere, still no exact answer).<br>
So i prepare some filters.

    gcc -ggdb3 -Wall -lm -ljack -lX11 -lXrender -lXss -lGL -lfftw3_threads -lfftw3 -o jasmine-sa ./jasmine-sa.c && echo -e '{\n1\nMemcheck:Leak\n...\nsrc:dl-open.c:874\n}\n{\n2\nMemcheck:Leak\n...\nsrc:dl-init.c:121\n}\n' > /tmp/s && valgrind --leak-check=full --show-leak-kinds=all --suppressions=/tmp/s ./jasmine-sa -k 16 system:capture_1 -e -O -M 0 -A 1 -o 0
    

CREDITS
-------
* [1] https://holometer.fnal.gov/GH_FFT.pdf
* [2] https://kluedo.ub.rptu.de/frontdoor/deliver/index/docId/4293/file/exact_fft_measurements.pdf
* [3] https://www.gaussianwaves.com/2020/09/equivalent-noise-bandwidth-enbw-of-window-functions/
* [4] Analog Devices MT-001.pdf
* [5] https://stackoverflow.com/questions/79065474/transparent-background-and-msaa-at-same-time-for-opengl-window-on-x11 **Not work**, please see below a copy.
* [5a] https://www.reddit.com/r/opengl/comments/1dhwgn1/alpha_to_coverage_and_intel_gpus/
* [6] https://gitlab.freedesktop.org/mesa/mesa/-/issues/11275
* [7] https://hal.science/hal-00414583/document
* [8] https://people.ece.cornell.edu/land/PROJECTS/ReassignFFT/index.html
* [9] https://github.com/x42/spectra.lv2
* Thanks to all brave people from #lad, #opengl Libera chat, and others, who helps me all this time.
* Credits for C code are shown at tail of C file.

About [5]. This my question on SO was downvoted (aka deleted) very fast. Here is carbon copy.

> https://stackoverflow.com/questions/79065474/transparent-background-and-msaa-at-same-time-for-opengl-window-on-x11
> 
> Transparent background and MSAA at same time for OpenGL window on X11
> 
> Asked 8 months ago
> 
> Modified 7 months ago
> 
> Viewed 119 times
> 
> -3
> 
> This post is hidden. It was automatically deleted 7 months ago by CommunityBot.
> 
> I need (A) transparent or semi-transparent background (i.e., not just transparent things in opaque window), like per [1], [2], [3]. At the same time, i need (B) MSAA (or GPU-accelerated smoothing of other type). There are varoius working examples for (B). The only working example for (A) for up-to-day OpenGL i found, is [4]; the reason why is quite hard to obtain transparent background with OpenGL & X11, author of [4] commented here [5] (2nd answer from top, expand comments and read important thing about "You definitely must use XRenderPictFormat...").
> 
> But now i need to join (A) & (B), with GPU only. No any example of this combo, and even no any info if it can/should be possible in theory. After a lot of coffees and segfaults, i am have partial understanding with it. Long story short, if we look at glxinfo | grep "32 tc" or so, it can be found that on Intel GPUs, regardless of if it is battery operated Celeron or hundered Watts CPU, there are no one Visual with MSAA: two right columns are zeroes. Or, when with MSAA (up to 16x), there is no one Visual with Alpha. And my massive trial-and-errors with C code, all are failed, confirms this (so far). My Frankenstein made of (A) and (B) does not want to alive.
> 
> Contrary to that, with (external) Nvidia GPU, even old one, there is no problem to have (A) & (B) at same time: you 1) see this via glxinfo, 2) just take code [4] and add/enable all the OpenGL eyecandies.
> 
> So it shows that (a) Intel GPU (or, (b) current Linux Intel driver/OpenGL) makes it impossible.
> 
> Q1: How can i confirm that with theoretical specs of hardware?
> 
> Q2: Is glxinfo output mostly driver-dependent?
> 
> So far, so good. Know that it is impossible (with Intel, which is my target, i am limited in physical size of setup) is better than not know it at all.
> 
> BUT.
> 
> My old Compiz easily overcomes this. On Intel GPU, using Window Opacity, i can get window transparency (or transparent window over another transparent window, or even over a video) with exact pixel-precision math, and at no any noticeable bit of extra CPU load. Compiz uses the OpenGL library as the interface to the graphics hardware, as per wiki.
> 
> Q3. How can it be possible?
> 
> Q4. Again, where are the specs for this patricular case?
> 
> This (Compiz's) type of transparency (C) can be fundamentally different from (A), because theoretically can be implemented without alpha channel, because all the pixels of one window are same "alpha" (semi-transparency) value (unlike my (A) case, where i need fully opaque elements). How i can in my OpenGL C code use this (C) per-window semi-transparency?
> 
> UPDATE.
> 
> For semi-transparency of type (C), i found, test for CPU efficiency, and use now in my release code, the X11 method described by author of [3] in comment #2 at [3]:
> 
>     uint32_t cardinal_alpha = (uint32_t) (0.5/transpareny/ * (uint32_t)-1) ;
>     XChangeProperty(display, win,
>     XInternAtom(display, "_NET_WM_WINDOW_OPACITY", 0),
>     XA_CARDINAL, 32, PropModeReplace, (uint8_t*) &cardinal_alpha,1) ;
> 
> Thanks.
> 
> Arch Linux, 6.11.1-arch1-1, OpenGL ES 3.2 Mesa 24.2.3-arch1.1, OpenGL ES GLSL ES 3.20
> 
> OpenGL renderer string: Mesa Intel(R) UHD Graphics 630 (CFL GT2) (along other Intels)
> 
> All test setups are use fresh software, except Compiz, it is stable 0.8.18.
> 
> [1] [How to make an OpenGL rendering context with transparent background?](https://stackoverflow.com/questions/4052940)
> 
> [2] [Setting transparent background color in OpenGL doesn't work](https://stackoverflow.com/questions/48675484)
> 
> [3] https://gist.github.com/je-so/903479
> 
> [4]
> https://github.com/datenwolf/codesamples/blob/master/samples/OpenGL/x11argb_opengl/x11argb_opengl.c
> 
> [5] https://stackoverflow.com/a/9215724


LICENSE
-------
This manual for Jasmine-SA, as well as man page for Jasmine-SA, are licensed under Creative Commons Attribution 4.0. You are welcome to contribute to the manual in order to improve it so long as your contributions are made available under this same license.

Jasmine-SA is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
