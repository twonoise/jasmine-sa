// faust2lv2 quadraturetestsrc.dsp  &&  sed -i -e 's/out0/OutI/g' -e 's/out1/OutQ/g' -e 's/out2/LevelI/g' -e 's/out3/LevelQ/g' quadraturetestsrc.lv2/quadraturetestsrc.ttl  &&  cp -R ./quadraturetestsrc.lv2/ /usr/local/lib/lv2/

declare name "QuadratureTestSrc"; // No spaces for better JACK port names.
declare version "2026";
declare author "jpka";
declare license "MIT";
declare description "In-phase and Quadrature paired (complex numbers) signal source, can output `negative' frequencies.";

import("stdfaust.lib");

BTNPOS   =  button("[0] ON Pos");
BTNNEG   =  button("[1] ON Neg");
BTNNOISE =  button("[2] Noise ON");
BTNLO    =  button("[3] Freq x 0.1");
BTNHI    =  button("[4] Freq x 10");
LEVEL    = hslider("[5] Output Level", 1.0, 0.0, 2.0, 0.1);

freq = (440, 44 :> select2(BTNLO)), 4400 :> select2(BTNHI);

att = 1; //, 0.5 :> select2(BTNPOS * BTNNEG);

testsrc(p) = ( os.oscp(freq * 1.00, ma.PI/2 * p) * 0.22 +
               os.oscp(freq * 1.05, ma.PI/2 * p) * 0.20 +
               os.oscp(freq * 1.10, ma.PI/2 * p) * 0.18 +
               os.oscp(freq * 1.15, ma.PI/2 * p) * 0.16 +
               os.oscp(freq * 1.20, ma.PI/2 * p) * 0.14 ) * LEVEL * att;

envelope = abs : max ~ -(1.0/ma.SR);

outputI = testsrc( 0) * (BTNPOS | BTNNEG) +
          no.noises(2, 0) * 0.01 * BTNNOISE; // -40 dB;
// Note that when both Pos & Neg buttons, these are cancel each other, so our Q-out VU meter will show zero. This is expected, and does not mean that both buttons should not be used at same time.
outputQ = testsrc( 1) * BTNNEG +
          testsrc(-1) * BTNPOS +
          no.noises(2, 1) * 0.01 * BTNNOISE; // -40 dB;

process =
  outputI,
  outputQ,
  (outputI : envelope : vbargraph("[6] Out I", 0, 1.0)),
  (outputQ : envelope : vbargraph("[7] Out Q", 0, 1.0))
;
