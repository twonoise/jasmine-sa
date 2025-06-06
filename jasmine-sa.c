/*
 * jasmine-sa is multichannel hi-res Spectrum Analyzer for X11 & JACK (Linux 64bit).
 * License: GPL version 2 or later.
 * See man page for compile; README.md for usage; tail for credits.
 */

#define GL_GLEXT_PROTOTYPES // For glWindowPos2i()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>
#include <errno.h>

#include <math.h>
#include <bsd/bsd.h> // strlcat()
#include "sys/param.h" // MIN(), MAX()

#include <fftw3.h>

#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/scrnsaver.h>

#include <GL/glx.h>
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

#define C  "Ctrl"
#define F  "FFT"
#define J  "JACK"
#define O  "OpenGL"
#define P  "Parse"
#define S  "System"
#define X  "X11"

// __label__ cleanup;

#define cFmt(color,s,S) "\e[0;3"color"m"s":\e[0m "S"\n"
#define DBV(s,S,...) if (verbose>3) printf(cFmt("5",s,S), ##__VA_ARGS__);
#define DBG(s,S,...) if (verbose>2) printf(cFmt("4",s,S), ##__VA_ARGS__);
#define MSG(s,S,...) if (verbose>1) printf(cFmt("2",s,S), ##__VA_ARGS__);
#define WRN(s,S,...)       fprintf(stderr, cFmt("3",s,S), ##__VA_ARGS__);
// #define ERR(s,S,...)     { fprintf(stderr, cFmt("1",s,S), ##__VA_ARGS__); goto cleanup;
#define ERR(s,S,...)     { fprintf(stderr, cFmt("1",s,S), ##__VA_ARGS__); exit (1); }

// HSL colors: individual Hue values...
uint64_t fontColors = 0x0000501930805056;
uint64_t rayColors = 0xa83300c06df08d20; // 20=orange, 8d=blue, f0=pink, 6d=aqua...
// ...and grouped Saturation and Luma values
uint64_t satLuma = 0xff80ffb2ff80ff80; // SSLL: Reserved, Lines, Grid, Font.

#define MAXMEM 16

static void usage(const char *name)
{
  printf("%s is multichannel hi-res Spectrum Analyzer for JACK\n"
  "Usage: %s [options] port1 [ port2 ... ]\n"
  "options:\n"
  " -k, --fftk-max=N         max FFT size to 2^k. Default: 20\n"
  " -r, --roll=N             max roll factor, 1..256. Default: 16\n"
  " -j, --jobs=N             use 1 (default) to 4 fftw3's threads (aka jobs)\n"
  " -h, --hz=N[,N[,N[,N]]]   X axis: min (Hz), max (Hz), grids,\n"
  "                            grid cell size (px). Default: 0,20000,10,50\n"
  " -d, --db=N[,N[,N[,N]]]   Y axis: min (dBV), max (dBV), grids,\n"
  "                            grid cell size (px). Default: -100,0,10,50\n"
  " -D, --db-pwr=...         same as -d, but with dB Power units\n"
  " -p, --phosphor=N         auto memory levels, 1..16. Default: 0\n"
  " -u, --subgrid-size=N     sub-grid pitch, px. Default: 5\n"
  " -e, --enob               show ENOB scale on right Y axis,\n"
  "                            and set Noise (Avg) mode for all\n"
  "                            channels as default.\n"
  " -z, --show-zero          show zero input as minimal value\n"
  "                            (-327.67 dB) plot, rather than hide it.\n"
  " -c, --colors=UINT64      up to 8 hex colors as hue values,\n"
  "                            seven Font colors and one Grid color;\n"
  "                            0 is special value: Gray;\n"
  "                            1 or ff is Red, 80 is Aqua.\n"
  "                            Default: %lx\n"
  " -q, --ray-colors=UINT64  same as -c, 8 channels ray colors.\n"
  "                            Default: %lx\n"
  " -l, --sat-luma=UINT64    up to 4 hex SSLL saturation+luma pairs:\n"
  "                            Reserved, Lines, Grid, Font.\n"
  "                            Default: %lx\n"
  " -s, --ray-style=N        CRT ray style, 0..7. Default: 0 is Auto.\n"
  " -f, --ray-fade           CRT ray brightness dim on long jog,\n"
  "                            like real CRT\n"
  " -m, --memory-slots=N     memory slots to use, 0..%d (default)\n"
  " -g, --gpu-comp=N         openGL point & line sizes compensation %%\n"
  "                            for particular GPU, 0..100,\n"
  "                            like 0 for Intel, 50 for Nvidia\n"
  " -o, --opacity=N          background opacity %%, 0..100\n"
  " -b, --bits=N             window behavior bits, default: 0, are sum of:\n"
  "                             1: Lock window position\n"
  "                             2: Hide window caption and border\n"
  "                             4: Always on top\n"
  "                             8: Disable screen saver\n"
  "                            16: Hide everything except plot area\n"
  "                            32: Hide right info pane aka legend\n"
  "                            64: No progressbar. It shows buffering & FFT\n"
  "                                when low FPS, but can be hidden always\n"
  "                           128: Invert luma (brightness)\n"
  " -O, --opengl             use openGL\n"
  " -M, --msaa=N             use MSAA, 0..4. Default: 0\n"
  " -A, --alpha=N            use Alpha transparency, 0 or 1 (default),\n"
  "                            see also -o\n"
  " -x, --x-pos=N            position on screen, px\n"
  " -y, --y-pos=N            position on screen, px\n"
  " -w, --rev-wheel          reverse mouse wheel\n"
  " -v, --verbose=N          message filter, 0..4. Default: 2\n"
  "port1 [ port2 ... ]       use 'jack_lsp' to see all \n", name, name, fontColors, rayColors, satLuma, MAXMEM - 1);
}

static const char *shortopts =
  "k:r:j:h:d:D:p:u:ezc:q:l:s:fm:g:o:b:OM:A:x:y:wv:";

static const struct option longopts[] = {
  {"fftk-max",     1, 0, 'k'},
  {"roll",         1, 0, 'r'},
  {"jobs",         1, 0, 'j'},
  {"hz",           1, 0, 'h'},
  {"db",           1, 0, 'd'},
  {"db-pwr",       1, 0, 'D'},
  {"phosphor",     1, 0, 'p'},
  {"subgrid-size", 1, 0, 'u'},
  {"enob",         0, 0, 'e'},
  {"show-zero",    0, 0, 'z'},
  {"colors",       1, 0, 'c'},
  {"ray-colors",   1, 0, 'q'},
  {"sat-luma",     1, 0, 'l'},
  {"ray-style",    1, 0, 's'},
  {"ray-fade",     0, 0, 'f'},
  {"memory-slots", 1, 0, 'm'},
  {"gpu-comp",     1, 0, 'g'},
  {"opacity",      1, 0, 'o'},
  {"bits",         1, 0, 'b'},
  {"opengl",       0, 0, 'O'},
  {"msaa",         1, 0, 'M'},
  {"alpha",        1, 0, 'A'},
  {"x-pos",        1, 0, 'x'},
  {"y-pos",        1, 0, 'y'},
  {"rev-wheel",    0, 0, 'w'},
  {"verbose",      1, 0, 'v'},
  {0, 0, 0, 0}
};

// User options and variables, and their default values.
int xHzMin = 0;
int xHzMax = 20000;
int xGrids = 10;
int xGridSize = 50;
int xSize;
int xShift;
float deltaHz;
int sampleNum;

int yDbMin = -100;
int yDbMax = 0;
int yDbStep;
int yGrids = 10;
int yGridSize = 50;
int ySize;
int isDbPwr = 0;  // 0 or 1
int subGridSize = 5;

int crtRayStyle = 0;  // 0 = Auto
int optRayFade = 0;
int gpuComp = 0;
int memorySlots = MAXMEM - 1;
int defPhospor = 0;

int windowBits = 0;

int maxFFTK = 20;
int maxRoll = 16;
int optShowEnob = 0;
int optShowZero = 0;
int opacity = 100;  // background, 0...100 %
int verbose = 2;    // 0...4
int optRevWheel = 0;
int jobs = 1;

int optOpengl = 0;
int optAlpha = 1;
int optMsaa = 0;

// Geometry
#define MAXXSIZE 2048
#define MAXYSIZE 2048
#define LEGENDWIDTH 168
// Half-size of markers
#define mkrSize 6

float scalingYcoe0, scalingYcoe1;

// Storage and processing
#define MAXCH 8
#define MAXPHOSPHOR (16 + 1)

// More than 1.0 is useless.
#define MAXSTEP 1.0
// Too small increases 'data' array, read its note.
#define MINSTEP 0.125
// for MINSTEP 0.125=1/8
#define MAXDATA (MAXXSIZE * 8 + 1)
// Limits precision to 0.01 dB while ~ -327 to 327 dB dynamic.
#define intDbScale 100

// It is essentially important to keep arrays as small as possible, due to memory page switch latency have bad effects. This is a reason for int16s.
int16_t data[MAXMEM][MAXDATA][MAXCH];
#define NODATA (int16_t) -32768

int memCurr = 0, memPrev;
int memAddScheduled = 0;
int memQty;

#define Hz  1L
#define kHz 1000L
#define MHz 1000000L

int64_t startHz, spanHz, minHz, maxHz, minSpanHz, maxSpanHz;

int menuPage;
int menu;
int units;
int discardCurrentFft;
int stats;
int stopped;
int rePlot;
int phosphor;
int firstUsedBin, lastUsedBin;

int amptZero, amptMax, amptOverload;
int lowCpu;

int mkrIsDelta;
int marker[2];
int mkrCh[2];
int valueAbsMkr = 0;
int fnumAbsMkr = 0;
double freqHz = 0;

int vbwLog;
int vbw;
int vbwContinue = 0;

int rbwLog;
int rbwLogMin;
int rbwLogMax;
float rbw;

int programExit = 0;

// X11 and optional openGL
Display *dpy;
Window win;
XFontStruct *xfont;
Atom wm_delete_window;
XSetWindowAttributes attr;
GLXContext glcontext;
XVisualInfo visual;
Pixmap pm, pmMkr;
#define mkrW 192
#define mkrH 64
int depth;

// Corner shifts
int DX = 32;
int DY = 24;

int winXPos = 100, winYPos = 100;
int winW, winH;
int nPoints;
XPoint points[MAXDATA];

// X11 AARRGGBB Icon
#define W 0xffffffff
#define R 0xffff0000
#define G 0xff888888
#define B 0xff000000

uint64_t icon[] = {  // 32 bit will not work with x86_64.
    32, 21,
    0,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,0,
    G,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,G,
    G,W,W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,W,W,W,W,W,W,W,W,W,W,W,W,G,
    G,W,W,G,B,B,B,B,B,B,B,B,B,B,B,B,B,G,W,W,G,G,W,G,G,W,G,G,W,W,W,G,
    G,W,W,G,B,B,B,B,B,B,B,B,B,B,B,B,B,G,W,W,W,W,W,W,W,W,W,W,W,W,W,G,
    G,W,W,G,B,B,B,B,B,B,R,B,B,B,B,B,B,G,W,W,G,G,W,G,G,W,G,G,W,W,W,G,
    G,W,W,G,B,B,B,B,B,R,B,R,B,B,B,B,B,G,W,W,W,W,W,W,W,W,W,W,W,W,W,G,
    G,W,W,G,B,B,B,B,B,R,B,R,B,B,B,B,B,G,W,W,G,G,W,G,G,W,G,G,W,W,W,G,
    G,W,W,G,B,B,B,B,B,R,B,R,B,B,B,B,B,G,W,W,W,W,W,W,W,W,W,W,W,W,W,G,
    G,W,W,G,B,B,B,B,R,B,B,B,R,B,B,B,B,G,W,W,G,G,W,W,W,W,G,G,G,W,W,G,
    G,W,W,G,B,B,B,B,R,B,B,B,R,B,B,B,B,G,W,W,G,G,W,W,W,G,W,W,W,G,W,G,
    G,W,W,G,R,R,R,R,B,B,B,B,B,R,R,R,R,G,W,W,W,W,W,W,W,G,W,G,W,G,W,G,
    G,W,W,G,B,B,B,B,B,B,B,B,B,B,B,B,B,G,W,W,G,G,W,W,W,G,W,W,W,G,W,G,
    G,W,W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,W,G,G,W,W,W,W,G,G,G,W,W,G,
    G,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,G,
    G,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,G,
    G,W,W,W,G,G,W,W,G,G,G,G,W,W,G,G,G,W,W,W,W,W,G,W,G,W,G,W,G,W,W,G,
    G,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,G,
    0,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,0,
    0,0,0,G,W,W,W,G,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,G,W,W,W,G,0,0,0,
    0,0,0,G,G,G,G,G,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,G,G,G,G,G,0,0,0,
};


// FFT
int plans;
uint64_t fftSize;
uint64_t chunkSize;
uint32_t plotSamplesNum;
float fftPlotTime, fftsPerSecond, framesPerSecond;

uint64_t fftSizeK, fftOldSizeK;
double *fftin[MAXCH], inmin[MAXCH], inminAbsNonzero[MAXCH], inmax[MAXCH];

#define MINFFTK 13
#define MAXFFTK 25  // 31 is max.
#define MAXPLANS (MAXFFTK - MINFFTK + 1)
fftw_complex *fftout[MAXCH];
fftw_plan plan[MAXPLANS][MAXCH];

// 0: No/Custom, 1: Hanning, 2: FlatTop, 3: HFT144D.  [1] [2]
// Long Chebyshev windows are takes too long time (months) to calc. (120, 150 att)
#define MAXWIN 4
#define DEFWIN 3 // HFT144D
int fftWindow[MAXCH];
char *fftWindowStr[MAXWIN] = {"Blackmn", "Hanning", "FlatTop", "HFT144D"};
double *windowfunc[MAXPLANS][MAXWIN];

// 0: Tone (Max), 1: Noise minus window NF (Avg)
#define MAXMEASMODE 2
int measMode[MAXCH];
char *measModeStr[MAXMEASMODE] = {"Tone (MAX)", "Noise minus NF (AVG)"};
#define AVERAGE (measMode[ch])  // Avg or Max

// Window noise gain (i call it Noise figure, NF).  [1] [3]
// HFT144D           4.5386 bins
// Hanning (Hann)    1.500 bins 1.761 dB
// Sine (Cosine)     1.234 bins 0.912 dB
// Blackman          1.727 bins 2.373 dB
// Flattop           3.770 bins 5.764 dB
float fftWindowNFbins[MAXWIN] = {1.727, 1.5000, 3.7702, 4.5386};

// Display
char inputStr[256], inputName[256], resultStr[256];
int prevParamStrLen = 0;
// We'll reuse overlapping audio data for:
// 1. Faster plot updates,
// 2. Lower energy loss. From [1], 'overlap' is our '1 - roll', but we can't follow per-window recommended values, due to we use different windows at same time.
uint64_t roll; // min=1 max=2^n
int rollPhase = 0;
float stepAbs, stepRel;
int squeeze;

// JACK
uint64_t channels;
int64_t sampleRate; // Not uint64_t
const size_t sample_size_4bytes = sizeof(jack_default_audio_sample_t);
char portName[MAXCH][64];

// Common useful things
#define FIT(x, min, max) (x < min ? min : x > max ? max : x)
#define BYTE(x, n) (((uint8_t *)&x)[n])


// Colors, and HSL->RGB [11] [12]
// I try to rewrite [12] to integer-only: bytes in and bytes out.
// To be even simpler, we need to limit hue span to 102 (0...0x66) or 204 (0...0xcc).
uint32_t ahsl2argb(uint8_t a, uint8_t h, uint8_t s, uint8_t l) // Full range 0..255 each
{
// NOTE h=0 is special one: forces not red, but gray color. Use 1 for red.
  if (h == 0)
    s = 0;

  if (windowBits & 128)
    l = 255 - l;

  if (! optAlpha)
    a = 255;

  // 24 bit
  uint16_t A = (s * MIN(l, 255 - l) * 259) >> 8;

  uint8_t f (uint8_t N)
  {
    uint16_t k = (N * 255 + (h-1) * 12) % (12*255); // N = 0 (R), 8 (G), 4 (B)

#ifdef no_pre_multiplied_alpha
    // 24 bit
    uint32_t r = (l * (257*255) - A * MAX(MIN(MIN(k - 3*255, 9*255 - k), 255), -255)) / (255*255);
#else
    // 32 bit, Pre-multiplied Alpha [9] [10]
    uint32_t r = (l * (257*255) - A * MAX(MIN(MIN(k - 3*255, 9*255 - k), 255), -255)) * a / (255*255*255);
#endif
    return (uint8_t) r;
  }

  uint32_t result = (((((a << 8) + f(0)) << 8) + f(8)) << 8) + f(4);
  DBV(C, "ahsl2argb(): optInvert %d, optAlpha %d, %x %x %x %x -> %x", !!(windowBits & 128), optAlpha, a, h, s, l, result);
  return result;
}

#define GRADIENTS (MAXMEM * 2)
GC bgColor, transparentColor;
GC fontColor[10];
GC lineColor[8 * GRADIENTS][2]; // CRT ray: thin, thick
uint32_t lineColorAbgr[8 * GRADIENTS];

void setFontAndColors(void)
{
  xfont = XLoadQueryFont(dpy, "fixed");
  if (! xfont)
    ERR(X, "XLoadQueryFont() failed.");

  transparentColor = XCreateGC(dpy, win, 0, 0);
  XSetForeground(dpy, transparentColor, 0);

  uint32_t bg = ahsl2argb(opacity * 255 / 100, 0, 0, 0);
  bgColor = XCreateGC(dpy, win, 0, 0);
  XSetForeground(dpy, bgColor, bg);

  for (int i = 0; i < 10; i++)
  {
    uint32_t fg = (i == 9) ? 0xff000000 : (i == 8) ? 0xffffffff : ahsl2argb(255, BYTE(fontColors, i), BYTE(satLuma, !i * 2 + 1), BYTE(satLuma, !i * 2));

    fontColor[i] = XCreateGC(dpy, win, 0, 0);
    XSetState(dpy, fontColor[i], fg, bg, GXcopy, 0xffffffff);
    XSetFont(dpy, fontColor[i], xfont->fid);
  }

  // Per-channel CRT ray colors, fading from bright to dim
  for (int ch = 0; ch < 8; ch++)
    for (int grad = 0; grad < GRADIENTS; grad++)
    {
      // Square root fade, or replace 2.0 to 1.0 to make it linear.
      float fade = pow(((GRADIENTS - grad) / (float)GRADIENTS), 2.0);

      // We dim both saturation and luma, it gives better colors.
      uint32_t fg = ahsl2argb(255, BYTE(rayColors, ch), (int)(BYTE(satLuma, 5) * fade), (int)(BYTE(satLuma, 4) * fade));
      int c = ch * GRADIENTS + grad;
      lineColorAbgr[c] = (__builtin_bswap32(fg) >> 8) | 0xff000000;

      // Thickness.
      for (int t = 0; t < 2; t++)
      {
        lineColor[c][t] = XCreateGC(dpy, win, 0, 0);
        // GXand can't work when inverted AND transparent at same time.
        // XSetState(..., optInvert ? GXand : GXor, 0xffffffff);
        XSetState(dpy, lineColor[c][t], fg, bg, GXor, 0xffffffff);
        XSetLineAttributes(dpy, lineColor[c][t], t + 1, LineSolid, 2, 2);
      }
    }
}

void XFlushArea(int a, int b, int c, int d)
{
  XCopyArea(dpy, pm, win, bgColor, a, b, c, d, a, b);
  XFlush(dpy);
}

// Font engine, should be used with bitmap 'fixed' 6x13 guaranteed to be available and fastest possible.
int xx = 0, yy = 0;
int fgcolor = 1, bgcolor = 0;
int vtab = 14;

void plotGotoXY(int x, int y)
{
  xx = x;
  yy = y;
}

void plotSetColors(int fg, int bg)
{
  fgcolor = fg % 16; // 0..7 font colors, 8..15 line colors
  bgcolor = bg; // Can be negative.
}

// bgcolor < 0 is transparent background.
//  bgcolor = -2 is centered text.
//  bgcolor = -3 is left shifted text.
// fgcolor = {5, 6} is immediate update.
// fgcolor = 9 and 8 are opaque black and white.
void plotStr(const char *fmt, ...)
{
  char buffer[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  int textWidth = 0;
  int glyphHeight = 13; // 'fixed' is 6x13
  int centeringOffset = 0;

  if ((yy < 0) || (!strlen(buffer)))
    return;

  textWidth = strlen(buffer) * 6; // 'fixed' is 6x13

  if (bgcolor == -2)
    centeringOffset = - textWidth / 2;
  else if (bgcolor == -3)
    centeringOffset = - textWidth;

  GC color = (fgcolor < 10) ? fontColor[fgcolor % 10] : lineColor[((fgcolor - 10) % 8) * GRADIENTS][0];

  if (bgcolor >= 0)
    // Opaque background
    XDrawImageString(dpy, pm, color, xx + centeringOffset, yy + glyphHeight - 2, buffer, strlen(buffer));
  else
  {
    // No background
    XDrawString(dpy, pm, color, xx + centeringOffset, yy + glyphHeight - 2, buffer, strlen(buffer));
  }

  // These special colors causes immediate update.
  if (((fgcolor == 5) || (fgcolor == 6)) && (! optOpengl))
    XFlushArea(xx + centeringOffset, yy, textWidth, glyphHeight);

  yy = yy + vtab;
  if (yy > DY + ySize)
    yy = -1; // Stop plotting this column.
}

void plotBottomHelp(void)
{
  if (windowBits & 16)
    return;

  // 1. Print with opaque BG
  plotGotoXY(18, DY + ySize + 18);
  plotSetColors(2, 0);
  if (menuPage == 0)
    plotStr("Center    Span      RBV       %s   %s   Memory    Y Shift   %s   Stop       Menu: 0", (vbw)?"VBW    ":"MaxHold", (mkrIsDelta)?"MkDelta":"Marker ", (isDbPwr)?"dBVolts":"dBPower");
  else if (menuPage == 1)
    plotStr("Stats                                   MkrCntr   stepRel   Phospho   LnStyle   Reset      Menu: 1");
  else
    plotStr("Ch.1      Ch.2      Ch.3      Ch.4      Ch.5      Ch.6      Ch.7      Ch.8      All        Menu: 2");

  // 2. Print with transparent BG on top of previous string. Special color causes immediate update.
  plotGotoXY(4, DY + ySize + 18);
  plotSetColors(5, -1);
  plotStr("F1        F2        F3        F4        F5        F6        F7        F8        F9        F10        ");
}

void printParam(void)
{
  char tempStr[512];

  plotGotoXY(DX + 6, DY + 5);
  plotSetColors(5, 0); // Color 5 is like 1 (green) but causes immediate update.
  if (strlen(resultStr))
    sprintf(tempStr, resultStr);
  else
    sprintf(tempStr, "%s%s", inputName, inputStr);

  // 1. Add tail whitespace to cover longer previous text;
  // 2. Also for 'backspace' correctly deletes last char.
  int newLen = strlen(tempStr);
  int diff = prevParamStrLen - newLen;
  if (diff)
    for (int i = 0; i <= MIN((diff + 2), 128); i++)
      strlcat(tempStr, (char[2]){' ', '\0'}, 254);

  plotStr(tempStr);
  prevParamStrLen = newLen;
}

#define ADDPOINT(X, Y) { points[nPoints].x = DX + X; points[nPoints++].y = DY + Y; }
#define RAYFADE(fade, y0, y1) (MIN(fade + MIN((abs(y1 - y0)) / 4, GRADIENTS / 1), GRADIENTS - 1))
#define PLOTAREA DX - mkrSize, DY - mkrSize, xSize + mkrSize * 2 + 1, ySize + mkrSize * 2 + 1
#define COLOR(ch, fade) ((ch % 8) * GRADIENTS + (fade % 16))

void newPlot()
{
  if (phosphor < MAXPHOSPHOR)
    XFillRectangle(dpy, pm, bgColor, PLOTAREA);

  nPoints = 0;

  for (int j = 0; j <= yGrids; j++)
  {
    int step = (j == (yDbMax / yDbStep)) ? 2 : subGridSize;
    for (int i = 0; i <= xSize / step; i++)
      ADDPOINT(i * step, j * yGridSize);
  }

  for (int j = 0; j <= xGrids; (spanHz == 0) ? j = j + xGrids : j++)
    for (int i = 0; i < ySize / subGridSize; i++)
      ADDPOINT(j * xGridSize, i * subGridSize);

  XDrawPoints(dpy, pm, fontColor[0], points, nPoints, CoordModeOrigin);

  plotGotoXY(DX + 6, DY + 20);
  plotSetColors(3, 0);

  if (amptOverload >= 0)
    plotStr("Level Overload ch.%d", amptOverload);
  else if (amptMax >= 0)
    plotStr("Level Max ch.%d", amptMax);
  else if (amptZero >= 0)
    plotStr("Zero Input ch.%d", amptZero);

  amptZero = amptMax = amptOverload = -1;

  if (lowCpu)
    plotStr("Low CPU.");

  if ((! stopped) && (stats)) {
    plotGotoXY(DX + xSize - 6, DY + 8);
    for (int i = 0; i < channels; i++)
    {
      plotSetColors(i + 10, -3);
      plotStr("%.3f..%.3f, z %.3f", inmin[i], inmax[i], log2(inminAbsNonzero[i]) - 1); // Sign bit also counts.
    }
  }

  printParam();
}


#define UNITS2STR    (units == Hz) ? "Hz" : (units == kHz) ? "kHz" : "MHz"
#define DUNITS2STR   / (float)units, UNITS2STR
#define DIGIT2STR(d) (char[2]){(char)(d + '0'), '\0'}
#define PHOSPHOR2STR (phosphor < MAXPHOSPHOR) ? "Phosphor: %d" : "Phosphor: Unlimited", phosphor
#define RBW2STR rbw > 1 ? "RBW: %.4g Hz/S (1/%gx)" : "RBW: %.4g Hz/S (%gx)", 2.0 / fftPlotTime, rbw > 1 ? rbw : 1 / rbw
#define VBW2STR vbw == 0 ? "VBW: Max Hold" : vbw == 1 ? "VBW: Full" : "VBW: 1/%dx", vbw

void legend(void)
{
  if (windowBits & 32)
    return;

  XFillRectangle(dpy, pm, bgColor, winW - LEGENDWIDTH, 0, LEGENDWIDTH, winH);

  plotGotoXY(DX + xSize + DX - 2, 5);
  plotSetColors(2, -1);
  plotStr("Fs: %g kHz", sampleRate / 1000.0);
  plotStr("FFTW3: Double, %d thr", jobs);
  plotStr("FFT: %ld", (1 << fftSizeK));
  plotStr("Start:  %.6g %s", startHz DUNITS2STR);
  plotStr("Stop:   %.6g %s", (startHz + spanHz) DUNITS2STR);
  plotStr("Center: %.6g %s", (startHz + spanHz / 2.0) DUNITS2STR);
  plotStr("Span:   %.6g %s", spanHz DUNITS2STR);
  plotStr(RBW2STR);
  plotStr(VBW2STR);
  plotStr("FPS: %.4g x %ld", fftsPerSecond, roll);
  plotStr("Step: %.4g %s", stepAbs * stepRel, squeeze ? "" : "(Exact mkr)");
  plotStr(PHOSPHOR2STR);

  for (int i = 0; i < channels; i++)
  {
    yy += 4;
    plotSetColors(i + 10, -1);
    plotStr("Ch. %d: %s", i, portName[i]);
    plotStr("%s %s", measModeStr[measMode[i]], fftWindowStr[fftWindow[i]]);
  }

  XFlushArea(winW - LEGENDWIDTH, 0, LEGENDWIDTH, winH);
}

void newScreen(int clear)
{
  if (clear) {
    memset(data, NODATA, sizeof(data));
    marker[0] = marker[1] = -1;
    vbwContinue = 0;
  }

  scalingYcoe0 = yGridSize / (float)yDbStep * (float)yDbMax;
  scalingYcoe1 = yGridSize / (float)yDbStep / (float)intDbScale;

  XFillRectangle(dpy, pm, bgColor, 0, 0, winW, winH);

  if (! (windowBits & 16))
  {
    plotGotoXY(DX + xSize / 2, 1);
    plotSetColors(4, -2);
    plotStr("* Jasmine Hi-Res Spectrum Analyzer *");

  // X axis labels.
    plotSetColors(1, -2);
    for (int i = 0; i <= xGrids; i++) {
      plotGotoXY(DX + i * xGridSize + 4, DY + ySize + 6);
      if ((spanHz > 0) || (i == 0) || (i == xGrids))
        plotStr("%.5g %s", (startHz + i * spanHz / (double)xGrids) / (double)units, (i == xGrids) ? UNITS2STR : "");
    }

  // Left Y axis labels: dB Pwr or dBV.
    plotSetColors(1, -2); // Centered texts
    plotGotoXY(DX - 9, DY - 22);
    plotStr((isDbPwr) ? "dB Pwr" : "dBV");

    plotSetColors(1, -1);
    for (int i = 0; i <= yGrids; i++)
    {
      int dB = yDbMax - i * yDbStep;
      plotGotoXY(DX - 30, DY + i * yGridSize - 7);
      plotStr("%4d", dB);
    }

  // Right Y axis labels: ENOB. Only sine waves; Only uncorrelated noise; Related to full scale (unity amplitude) signal. [4]
    if (optShowEnob)
    {
      plotSetColors(1, -2); // Centered texts
      plotGotoXY(DX + xSize + 9, DY - 22);
      plotStr("ENOB");

  // Testcase: 12-bit 8192p FFT, [4]:fig. 2 (NOTE: Units there, are dB pwr)
  // ENOB = (SINADpwr − 1.76 dBpwr) / 6.02
      plotSetColors(1, -1);
      for (int bits = 8; bits < 48; bits++)
      {
        float dBsnrPwr = 6.02 * bits + 1.76;
        float dBfftGain = 10.0 * log10(fftSize);
        plotGotoXY(DX + xSize + 7, DY + roundf(((dBsnrPwr + dBfftGain) / (float)(2 - isDbPwr) + yDbMax) * yGridSize / (float)yDbStep) - 7);
        if ((yy > DY) && (yy < ySize))
          plotStr("%2d", bits);
      }
    }

    plotBottomHelp();

    legend();
  }

  newPlot();

  XFlushArea(0, 0, winW - LEGENDWIDTH, winH);
}


void plotOneChannel(int ch)
{
  int pointThick = crtRayStyle % 3;
  int lineThick  = crtRayStyle / 3;
  if (! crtRayStyle) { // Auto
    pointThick = stepAbs > 4 ? 2 : 0;
    lineThick = 1;
  }

  // Draw from last to 1st to make fresh data on top. 0 = actual, 1-... = memory
  for (int m = memQty - 1; m >= 0; m--)
  {
    int mem = (memCurr - m + MAXMEM) % MAXMEM;

    int fade = m;
    if (phosphor)
      fade = MIN((MAXPHOSPHOR - 1) / phosphor * m, MAXMEM - 1);

    nPoints = 0;

    for (int i = firstUsedBin; i <= lastUsedBin; i++)
    {
      int x = (int)(i * (squeeze ? stepRel : stepAbs) + 0.0) + xShift;

      int y = data[mem][i][ch];

      // Currently, only one whole non-interrupted set of points. TODO
      if ((y != NODATA) && (x >= 0) && (x <= xSize))
      {
        y = scalingYcoe0 - y * scalingYcoe1;
        y = FIT(y, 0, ySize);
        ADDPOINT(x, y);
      }
    }

    if (! nPoints)
      return;

    // Now draw using same array with either X11 or openGL plot.
    if (! optOpengl)
    {
      if (lineThick)
      {
        if (optRayFade)
          // Effect of different brightness of long and short lines, as on real CRT.
          for (int i = 0; i < (nPoints - 1); i++)
          {
            int y0 = points[i].y;
            int y1 = points[i + 1].y;
            XDrawLine(dpy, pm, lineColor[COLOR(ch, RAYFADE(fade, y1, y0))][lineThick-1],  points[i].x, y0, points[i + 1].x, y1);
          }
        else
          XDrawLines(dpy, pm, lineColor[COLOR(ch, fade)][lineThick-1], points, nPoints, CoordModeOrigin);
      }

      if (pointThick)
      {
        XDrawPoints(dpy, pm, lineColor[COLOR(ch, fade)][pointThick-1], points, nPoints, CoordModeOrigin);
        // Workaround for X11 points don't use GC 'width' (bold are impossible).
        if (pointThick > 1)
        {
          // Absolute to relative coords
          for (int i = nPoints - 1; i >= 1; i--)
          {
            points[i].x = points[i].x - points[i - 1].x;
            points[i].y = points[i].y - points[i - 1].y;
          }

          // Place extra 4 points around
          for (int j = 0; j < 4; j++)
          {
            points[0].x = points[0].x + (((((j+1) % 4) / 2) * 2) - 1);
            points[0].y = points[0].y + (((j / 2) * 2) - 1) + !(j);
            XDrawPoints(dpy, pm, lineColor[COLOR(ch, fade)][pointThick-1], points, nPoints, CoordModePrevious);
          }
        }
      }
    }
    else
    {
      glColor4ubv((void *) lineColorAbgr + COLOR(ch, fade) * 4);
      glEnableClientState(GL_VERTEX_ARRAY);

      if (lineThick)
      {
        glLineWidth(MAX(lineThick, 1) - (float)gpuComp / 100.0);
        if (optRayFade)
        {
          // Effect of different brightness of long and short lines, as on real CRT.
          XPoint lines[MAXDATA * 2];
          uint32_t colors[MAXDATA * 2];

          // Separate multiline to atomic lines 0 1 2 3 4 -> (0) 0 1, 1 2, 2 3, 3 4 (4)
          for (int i = 0; i < nPoints; i++)
          {
            colors[i*2] = colors[i*2 + 1] = lineColorAbgr[COLOR(ch, RAYFADE(fade, points[i + 1].y, points[i].y))];
            lines[i*2].x = lines[i*2 + 1].x = points[i].x;
            lines[i*2].y = lines[i*2 + 1].y = points[i].y;
          }

          glEnableClientState(GL_COLOR_ARRAY);
          glVertexPointer(2, GL_SHORT, 0, &lines[1]); // Shifted a bit.
          glColorPointer(4, GL_UNSIGNED_BYTE, 0, &colors); // Not shifted.
          glDrawArrays(GL_LINES, 0, (nPoints - 1) * 2);
          glDisableClientState(GL_COLOR_ARRAY);
        }
        else
        {
          glVertexPointer(2, GL_SHORT, 0, &points); // XPoint's are int16's.
          glDrawArrays(GL_LINE_STRIP, 0, nPoints);
        }
      }

      if (pointThick)
      {
        glPointSize(MAX(pointThick + 1.5, 1) - (float)gpuComp / 100.0);
        glVertexPointer(2, GL_SHORT, 0, &points);
        glDrawArrays(GL_POINTS, 0, nPoints);
      }

      glDisableClientState(GL_VERTEX_ARRAY);
    }
  }
}


// Two shape of markers: diamonds or star, depend if marker is delta.
void plotMkr(int x, int y, int ch, int isDelta, int value, float freq)
{
  XImage *xim;
  int8_t q = mkrSize;
  int8_t p[12][4] = {{-q, 0, 0, q}, {0, q, q, 0}, {q, 0, 0, -q}, {0, -q, -q, 0},
        {-q/2, 0, 0, q/2}, {0, q/2, q/2, 0}, {q/2, 0, 0, -q/2}, {0, -q/2, -q/2, 0},
        {-q+1, -q+1, q-1, q-1}, {-q+1, q-1, q-1, -q+1}, {0, q, 0, -q}, {-q, 0, q, 0}};

  if (optOpengl)
    // Black opaque, or, Transparent.
    XFillRectangle(dpy, pmMkr, (depth < 32) ? fontColor[9] : transparentColor, 0, 0, mkrW, mkrH);

  for (int i = 0 + isDelta * 8; i < (8 + isDelta * 4); i++)
  {
    if (optOpengl)
      // White opaque, will be key.
      XDrawLine(dpy, pmMkr, fontColor[8], mkrW / 2 + p[i][0], mkrH / 2 + p[i][1], mkrW / 2 + p[i][2], mkrH / 2 + p[i][3]);
    else
      XDrawLine(dpy, pm, lineColor[ch * GRADIENTS][0], DX + x + p[i][0], DY + y + p[i][1], DX + x + p[i][2], DY + y + p[i][3]);
  }

  char tempStrDb[256];
  sprintf(tempStrDb, isDelta ? "%+.3g dB" : "%.5g dB", value / (float)intDbScale);

  char tempStrHz[256];
  sprintf(tempStrHz, isDelta ? "%+g %s" : "%g %s", freq DUNITS2STR);

  // Labels position change if close to top or right.
  int mirrorX = ((xSize - x) < 80) ? -3 : -1;
  int flipY = (y < 40) ? q - 12 : -32 - q;

  if (optOpengl)
  {
    int labelW = MAX(strlen(tempStrDb), strlen(tempStrHz)) * 6;
    int leftShift = (mirrorX < -2) ? labelW : 0;
    int mx = mkrW / 2 + (mirrorX + 2) * q - leftShift;
    int my = mkrH / 2 + flipY + 20;

    // White opaque.
    XDrawString (dpy, pmMkr, fontColor[8], mx, my, tempStrDb, strlen(tempStrDb));
    XDrawString (dpy, pmMkr, fontColor[8], mx, my + vtab, tempStrHz, strlen(tempStrHz));

    // For 24 bit: here is alpha also exist, but prefilled with 255, the problem is how to auto add alpha now (color keying).
    xim = XGetImage(dpy, pmMkr, 0, 0, mkrW, mkrH, AllPlanes, ZPixmap);
    if (! xim)
      ERR(X, "XGetImage() failed.");

    glLoadIdentity();
    glOrtho(0, winW, 0, winH, -1.0, 1.0);

    // OpenGL does not have any color keying (auto Alpha based on some color threshold).     We emulate Alpha keying using white "key" color, then do math based on it twice: for black letters outline, then for colored letters. It works good here, and we even do not need to know if we have Alpha (24 or 32 bit), except correct rectangle pre-fill above.
    // Opaque black for text outline.
    glBlendColor(0, 0, 0, 1.0);
    // Replace white as key color, to blend color.
    // glBlendFuncSeparate(), glBlendEquationSeparate() not helps much here.
    glBlendFunc(GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_COLOR);
  }

  // Colored text over black outline.
  for (int i = 0; i < 9; i++)
  {
    int dx = (i + 2) % 3;
    int dy = (i + 6) % 9 / 3;

    if (optOpengl)
    {
      if (i == 8)
        // Will replace white as key color, now not to black, but to text color.
        // The pity is that glBlendColor(), glClearColor() are not have 4ubv form.
        glBlendColor(BYTE(lineColorAbgr[ch * GRADIENTS], 0) / 255.0, BYTE(lineColorAbgr[ch * GRADIENTS], 1) / 255.0, BYTE(lineColorAbgr[ch * GRADIENTS], 2) / 255.0, 1.0);

      // [20] Unlike glRasterPos2i(), this one is faster and allows negative x, y.
      glWindowPos2i(DX + x + dx - mkrW / 2 - 1, winH - (DY + y + dy - mkrH / 2 - 1));
      glDrawPixels(mkrW, mkrH, GL_BGRA, GL_UNSIGNED_BYTE, (void*)(&(xim->data[0])));
    }
    else // Normal X11 transparent draw.
    {
      plotSetColors((i < 8) ? 9 : ch + 10, mirrorX);
      plotGotoXY(DX + x + dx + (mirrorX + 2) * q , DX + y + dy + flipY);
      plotStr(tempStrDb);
      plotStr(tempStrHz);
    }
  }

  if (optOpengl)
    XDestroyImage(xim);
}

void plotOneChannelMkr(int ch, int isDelta)
{
  if (! isDelta)
    valueAbsMkr = fnumAbsMkr = 0;

  int fnum = marker[isDelta];
  if (fnum != -1)
  {
    // For label, we need raw value...
    int value = data[memCurr][fnum][ch];
    if (value == NODATA)
      return;

    if (squeeze)
      // This result is approx., screen pixel aligned. To get exact one, we'll need to un-squeeze ("zoom in", i.e. narrower span).
      freqHz = (isDelta ? 0 : startHz) + ((fnum - fnumAbsMkr) + 1 * (startHz < 0)) * stepRel * spanHz / xSize;
    else
      // Here result is exact, and float (single) precision isn't enough for debug or report; while marker label limit digits, then result can be downgraded to float later.

      // Both are same:
      // freqHz = ((fnum - fnumAbsMkr) + (isDelta ? 0 : sampleNum)) * sampleRate / (double)fftSize;
      freqHz = (fnum - fnumAbsMkr) * (double)sampleRate / (double)fftSize +
          (isDelta ? 0 : (startHz + deltaHz));

    int x = (int)(fnum * (squeeze ? stepRel : stepAbs) + 0.0) + xShift;
    int y = scalingYcoe0 - value * scalingYcoe1;
    // ...But for position, we use limited one.
    y = FIT(y, 0, ySize);

    plotMkr(x, y, ch, isDelta, value - valueAbsMkr, freqHz);

    valueAbsMkr = value;
    fnumAbsMkr = fnum;
  }
}


void newFft(int forceClear)
{
  if (spanHz == 0)
    ERR(F, "spanHz == 0");

// These are experimental values, they should be adjusted due to variation in CPU && GPU power for particular system for best picture && eliminate low CPU warning.
#define MAXFPS 50
#define MAXRBW -4
#define MINRBW 8

  rbwLogMin = MAX((int) log2(spanHz / (float)(xSize * 2 * MAXFPS)), MAXRBW);
  rbwLogMax = MIN(rbwLogMin + 10, MINRBW);

  rbwLog = FIT(rbwLog, rbwLogMin, rbwLogMax);
  rbw = pow(2.0, rbwLog);

  DBG(F, "(1) spanHz %ld, rbw %f, rbwLog %d, rbwLogMin %d, rbwLogMax %d", spanHz, rbw, rbwLog, rbwLogMin, rbwLogMax);

  fftOldSizeK = fftSizeK;

  if (! stopped)
    fftSizeK = ceil(log2(sampleRate * xSize * 2.0 * rbw / (float)spanHz - 1.0));

  fftSizeK = FIT(fftSizeK, MINFFTK, maxFFTK);

  fftSize = 1 << fftSizeK;
  fftPlotTime = fftSize / (float)sampleRate;

  stepAbs = sampleRate * xSize / (float)spanHz / (float)fftSize;

  fftsPerSecond = framesPerSecond = 1.0 / fftPlotTime;
  roll = 1;
  while ((framesPerSecond < (MAXFPS / 5)) && (roll < maxRoll))
  {
    framesPerSecond = framesPerSecond * 2.0;
    roll = roll * 2;
  }

  chunkSize = fftSize * channels * sample_size_4bytes / roll;

  while ((stepRel < fmax(MINSTEP, stepAbs)) && (stepRel < MAXSTEP))
    stepRel = stepRel * 2.0;

  marker[0] = marker[1] = -1;
  mkrCh[0] = mkrCh[1] = 0;
  mkrIsDelta = 0;
  menuPage = 0;

  plotSamplesNum = (int)((float)fftSize * (float)spanHz / (float)sampleRate);
  squeeze = (plotSamplesNum < xSize) ? 0 : 1;

  if ((fftOldSizeK != fftSizeK) || (forceClear))
  {
    memQty = 1;
    rollPhase = 0;
    discardCurrentFft = 1;
    newScreen(! stopped);
  }

  DBG(F, "(2) sampleRate %ld, spanHz %ld, transform_size %ld", sampleRate, spanHz, fftSize);
  DBG(F, "(3) stepAbs %f, stepRel %f, roll %ld, fftSize %ld, chunkSize %ld", stepAbs, stepRel, roll, fftSize, chunkSize);

  float sampleNumF = startHz * (float)fftSize / (float)sampleRate;
  sampleNum = (int)(sampleNumF);
  deltaHz = (sampleNum - sampleNumF) / (float)fftSize * (float)sampleRate;
  xShift = ceil(deltaHz * (float)xSize / (float)spanHz + 0.0);
  DBV(F, "(4) %f %d %f %d", sampleNumF, sampleNum, deltaHz, xShift);
}


void setCenter (int new_value)
{
  int centerHz = FIT(new_value, minHz, maxHz);
  startHz = centerHz - spanHz / 2;
  sprintf(resultStr, "Center: %g %s", centerHz DUNITS2STR);
  newFft(0); // For sampleNum, at least.
  newScreen(1);
}

void setSpan (int new_value)
{
  int centerHz = startHz + spanHz / 2;
  spanHz = FIT(new_value, minSpanHz, maxSpanHz);
  startHz = centerHz - spanHz / 2;
  newFft(1);
  sprintf(resultStr, "Span: %g %s", spanHz DUNITS2STR);
}

// 'direction' here & after can be -1, 1, or 0 if just fit/limit
void setRbw (int direction)
{
  rbwLog = FIT(rbwLog + direction, rbwLogMin, rbwLogMax);
  rbw = pow(2, rbwLog);
  newFft(0);
  sprintf(inputStr, RBW2STR);
}

void setVbw (int direction)
{
  vbwLog = FIT(vbwLog + direction, -1, 8);
  vbw = (vbwLog < 0) ? 0 : 1 << vbwLog;
  sprintf(inputStr, VBW2STR);
  newScreen(0);
}

void setYShift (int direction)
{
  yDbMax = FIT(yDbMax + yDbStep * direction, -300, 100); // TODO fix limits.
  newScreen(0);
}

void instrumentReset (int cold)
{
  if (cold)
  {
    startHz = xHzMin;
    spanHz = xHzMax - xHzMin;
    stepAbs = stepRel = 1.0;
    crtRayStyle = 0;
    for (int ch = 0; ch < MAXCH; ch++)
    {
      fftWindow[ch] = DEFWIN;
      // Noise (Avg) menu is default for ENOB measurement.
      measMode[ch] = optShowEnob;
    }
    stats = 0;
    vbwLog = 0;
    vbw = 1;
    phosphor = defPhospor;
  }

  for (int ch = 0; ch < MAXCH; ch++)
    inminAbsNonzero[ch] = 2.0;

  setSpan(spanHz); // -> newFft(1) -> memory clear

  memAddScheduled = 0;
  inputStr[0] = inputName[0] = resultStr[0] = '\0';
  menu = 0;
  rePlot = 0;
}

void processMouse(int x, int y, int which)
{
  if ((x >= 0) && (x <= (xSize + 1)) && (y >= 0) && (y <= (ySize + 1)))
  {
    int fnum = ((x - xShift) / (squeeze ? stepRel : stepAbs) + 0.5);
    marker[which] = FIT(fnum, firstUsedBin, lastUsedBin);
  }
}

void processKeyboard (int rk)
{
  void inputMode (int m, char* str)
  {
    menu = m;
    strcpy(inputName, str);
    inputStr[0] = resultStr[0] = '\0';
  }

  rk = rk + 256 * menuPage;

  DBV(X, "Menu %d Key %d (%d)", menu, rk % 256, rk);

  // Per-channel things: menuPage == 2. F1...F9
  if ((rk >= (67 + 512)) && (rk <= (75 + 512)))
  {
    int ch = (rk - (67 + 512)); // Channel 0...7; or 8 for all channels
    int c = ch % MAXCH; // For all (8) we take 0 as reference
    if (c < channels)
    {
      // Cycle 1st param:
      // 0: Tone (Max), 1: Noise minus window NF (Avg)
      measMode[c] = (measMode[c] + 1) % MAXMEASMODE;
      if (measMode[c] == 0)
        // Cycle 2nd param:
        fftWindow[c] = (fftWindow[c] + 1) % MAXWIN;

      if (ch == MAXCH)
        for (int i = 1; i < channels; i++) // Copy reference to all
        {
          measMode[i] = measMode[0];
          fftWindow[i] = fftWindow[0];
        }

      sprintf(resultStr, "Ch. %s: %s, %s", (ch < MAXCH) ? DIGIT2STR(ch) : "All", measModeStr[measMode[c]], fftWindowStr[fftWindow[c]]);
    }
  }

  // This numeric and some more input handle is for menuPage 0.
  if ((menu == 1) || (menu == 2))
  {
    // 0..9 digits input, include keypad.
    for (uint8_t i = 0; i < 20; i++)
      if (rk == (uint16_t[20]){19, 90, 10, 87, 11, 88, 12, 89, 13, 83, 14, 84, 15, 85, 16, 79, 17, 80, 18, 81}[i])
      {
        strlcat(inputStr, DIGIT2STR(i / 2), 254);
        break;
      }

    // Add decimal point, if not already added.
    if ((rk == 59) || (rk == 60) || (rk == 91)) // Both ',' or '.'
      if (strchr(inputStr, '.') == NULL)
        strlcat(inputStr, ".", 254);

    // Backspace.
    if (rk == 22)
      inputStr[MAX(strlen(inputStr) - 1, 0)] = '\0';

    // 'h', 'k', 'm' are acts as Enter, yet define entry as Hz, kHz or MHz.
    if (((rk == 43) || (rk == 45) || (rk == 58)) && strlen(inputStr))
    {
      units = Hz;
      if (rk == 45) // 'k'
        units = kHz;
      else if (rk == 58) // 'm'
        units = MHz;

      if (menu == 1)
        setCenter(atof(inputStr) * units);
      else if (menu == 2)
        setSpan(atof(inputStr) * units);

      menu = 0;
    }
  }

  // Menu Page 0.
  if (rk == 67) // F1
    inputMode (1, "Center: ");

  if (rk == 68) // F2
    inputMode (2, "Span: ");

  if (rk == 69) // F3: RBW
  {
    inputMode (3, "");
    setRbw(0);
  }

  if (rk == 70) // F4: VBW
  {
    if (menu == 4)
      vbwLog = - !vbwLog;

    inputMode (4, "");
    setVbw(0);
  }

  if (rk == 71) // F5: Marker
  {
    if (menu != 5)
      mkrIsDelta = 0;
    else
      mkrIsDelta = (!(mkrIsDelta));

    sprintf(resultStr, mkrIsDelta ? "Marker delta" : "Marker");
    if (marker[mkrIsDelta] == -1)
    {
      if (mkrIsDelta)
        mkrCh[1] = mkrCh[0];
      // Set new marker at center using mouse click emulation.
      processMouse(xSize / 2, 0, mkrIsDelta);
    }
    menu = 5;
  }

  if ((rk == 72) && (! memAddScheduled) && (memorySlots)) // F6: Memory add (Esc: clear)
  {
    if (phosphor)
      memQty = 1;

    phosphor = 0;
    sprintf(resultStr, "Memory add: %d", memQty);
    memAddScheduled = 1;
  }

  if (rk == 73) // F7
  {
    inputMode (7, "Y Shift: ");
    setYShift(0);
  }

  if (rk == 74) // F8: dBV / dB Pwr
  {
    isDbPwr = (!(isDbPwr));
    strcpy(resultStr, "Y Scale changed.");
    newScreen(1);
  }

  // NOTE VBW filter can't work when stopped.
  if (rk == 75) // F9: Stop / Run
  {
    stopped = (!(stopped));
    sprintf(resultStr, stopped ? "Stopped" : "Sweep Normal");
    if (stopped)
    {
      phosphor = 0;
      memQty = 1;
    }
  }

  // Menu Page 1: Momentary actions, like on-off.
  if (rk == 67 + 256) // F1: Stats
    stats = !stats;

  if (rk == 71 + 256) // F5: Mkr to Center
    if (marker[0] != -1)
    {
      // Note: freqHz is valid after marker was drawn once.
      setCenter((int) freqHz);
      processMouse(xSize / 2, 0, 0);
    }

  if (rk == 72 + 256) // F6: StepRel
  {
    menu = 0;
    strcpy(inputStr, "");
    double stepRelOld = stepRel;
    stepRel = stepRel / 2.0;
    if (stepRel < MAX(MINSTEP, stepAbs))
      stepRel = MAXSTEP;

    if (stepRelOld != stepRel)
      newScreen(1);

    sprintf(resultStr, "stepRel: %g points", stepRel);
  }

  if (rk == 73 + 256) // F7: Phosphor
  {
    if (! phosphor)
      phosphor = 1;
    else
    {
      phosphor = phosphor * 2;
      if (phosphor > MAXPHOSPHOR)
        phosphor = 0;
    }
    sprintf(resultStr, PHOSPHOR2STR);
    legend();

    if (! phosphor)
      memQty = 1;
  }

  if (rk == 74 + 256) // F8: Line Style
  {
    crtRayStyle = (crtRayStyle + 1) % 9; // 0 = auto, or 1...8
    sprintf(resultStr, "Line Style: %s", crtRayStyle ? DIGIT2STR(crtRayStyle) : "Auto");
  }

  if (rk == 75 + 256)  // F9: Reset
    instrumentReset(1);


  // The following will work for all menu pages.
  rk = rk % 256;

  if ((rk == 23) || (rk == 2)) // Tab, Mouse middle click
    mkrCh[mkrIsDelta] = (mkrCh[mkrIsDelta] + 1) % channels;

  int direction = 0;
  int multiplier = 1;

  // minus, <-, '-', 'up arrow', PgUp, mouse wheel
  if ((rk==20) || (rk==82) || (rk==113) || (rk==111) || (rk==112) || (rk == (5 - optRevWheel)))
    direction = -1;

  // plus, ->, '+', 'down arrow', PgDown, mouse wheel
  if ((rk==21) || (rk==86) || (rk==114) || (rk==116) || (rk==117) || (rk == (4 + optRevWheel)))
    direction = 1;

  // Marker jog: 'up arrow', PgUp, 'down arrow', PgDown
  if ((rk==111) || (rk==112) || (rk==116) || (rk==117))
    multiplier = squeeze ? (stepRel * xSize / xGrids) : 10;

  if (direction != 0)
    switch (menu)
    {
      case 1:
        if (strlen(inputStr))
          break;

        units = kHz;

        setCenter(startHz + spanHz / 2 + (spanHz / 10) * direction);
        break;

      case 2:
        if (strlen(inputStr))
          break;

        int span = spanHz;

        float exponent = pow(10, (int)(log10(span)));
        float m = span / exponent;
        if (direction > 0)
        {
          if      (m < 2.0)
            m = 2.0;
          else if (m < 5.0)
            m = 5.0;
          else
            m = 10.0;
        }
        else
        {
          if      (m > 5.0)
            m = 5.0;
          else if (m > 2.0)
            m = 2.0;
          else if (m > 1.0)
            m = 1.0;
          else
            m = 0.5;
        }
        span = m * exponent;
        // TODO review all units manage.
        (span < 0.1 * kHz) ? (units = Hz) : (units = kHz);
        setSpan (span);
        break;

      case 3:
        setRbw(- direction);
        break;

      case 4:
        setVbw(- direction);
        break;

      case 5:
        if (marker[mkrIsDelta] != -1)
          marker[mkrIsDelta] = FIT(marker[mkrIsDelta] + direction * multiplier, firstUsedBin, lastUsedBin);
        break;

      case 7:
        setYShift(direction);
        break;
    }

  if (rk == 110) // 'home'
    if ((menu == 5) && (marker[mkrIsDelta] != -1))
      marker[mkrIsDelta] = firstUsedBin;

  if (rk == 115) // 'end'
    if ((menu == 5) && (marker[mkrIsDelta] != -1))
      marker[mkrIsDelta] = lastUsedBin;

  if (rk == 9) // Esc
    instrumentReset(0);

  if (rk == 53) // 'x' aka Ctrl+x
  {
    menu = 0;
    sprintf(resultStr, "Exit.");
    programExit = 1;
  }

  if (rk == 76) // F10
    menuPage = (menuPage + 1) % 3;
  else
  {
    if (menuPage == 1)
      menuPage = menu = 0;

    rePlot = 1;
  }

  plotBottomHelp();
  printParam();

  DBV(X, "Menu %d ", menu);
}


void processMessages()
{
  while(XPending(dpy))
  {
    DBV(X, "Keyboard, mouse or X11 process...");

    XEvent e;
    XNextEvent(dpy, &e);

    if (e.type == ClientMessage)
      if ((Atom)e.xclient.data.l[0] == wm_delete_window) // [14] [15]
        programExit = 1;

    if (e.type == Expose)
      XFlushArea(0, 0, winW, winH);

    if (e.type == KeyPress)
      processKeyboard(e.xkey.keycode);

    if (e.type == ButtonPress)
    {
      if ((e.xbutton.button == Button2) || (e.xbutton.button > Button3))
        // Mouse wheel gives 4 or 5, good for keyboard mimic: X11 keyboard codes are starts from 8.
        processKeyboard(e.xbutton.button);
      else
        processMouse(e.xbutton.x - DX, e.xbutton.y - DY, (e.xbutton.button != Button1));
    }
  }
}


// JACK threads.
typedef struct _thread_info
{
  pthread_t thread_id;
  jack_nframes_t rb_size;
  jack_client_t *client;
  unsigned int channels;
  volatile int can_capture;
  volatile int can_process;
  volatile int status;
} jack_thread_info_t;

/* JACK data */
unsigned int nports;
jack_port_t **ports;
jack_default_audio_sample_t **in;
jack_nframes_t nframes;

/* Synchronization between process thread && disk thread. */
jack_ringbuffer_t *rb;
pthread_mutex_t disk_thread_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t data_ready = PTHREAD_COND_INITIALIZER;
uint64_t overruns = 0;
jack_client_t *client;

static void *
disk_thread (void *arg)
{
  jack_thread_info_t *info = (jack_thread_info_t *) arg;
  uint64_t bufSize = (1 << MAX(maxFFTK, 16)) * channels * sample_size_4bytes * 1;
  void *buf = calloc (bufSize, 1);
  uint64_t bufPointer = 0;
  uint64_t readSpace;
  int chunksToRead;
  int64_t bufReadoutPointer;
  uint64_t bufSizeInSamples = bufSize / sample_size_4bytes;
  int planNum;

  pthread_mutex_lock (&disk_thread_lock);
  info->status = 0;
  channels = info->channels;

  void fftExecuteAndProcessOneChannel(int ch)
  {
    ch = ch % MAXCH;
    inmin[ch] = 0.0;
    inmax[ch] = 0.0;

    // Stage 1: Pre-process FFT data:
    // * Gathering;
    // * Windowing;
    // * Amplitude Zero, Min, Max checks.
    double Min = inmin[ch];
    double MinNZ = inminAbsNonzero[ch];
    double Max = inmax[ch];

    uint8_t winNum = fftWindow[ch] % MAXWIN;

    uint64_t bufReadoutSamplePointer = bufReadoutPointer / sample_size_4bytes;

#ifdef straight
    // This one can't be vectorized, because sample is not known each next cycle.
    for (uint64_t i = 0; i < fftSize; i++)
    {
      double sample;

      // NOTE Here is point of loss of precision: JACK is float.
      sample = ((float *)buf)[bufReadoutSamplePointer + ch];

      bufReadoutSamplePointer = bufReadoutSamplePointer + channels;
      // Should never be >, only ==, but still.
      if (bufReadoutSamplePointer >= bufSizeInSamples)
        bufReadoutSamplePointer = 0;

      if (1)
        // We use only left half of window, then mirroring it.
        if (i < (fftSize / 2))
          fftin[ch][i] = sample * windowfunc[planNum][winNum][i];
        else
          fftin[ch][i] = sample * windowfunc[planNum][winNum][fftSize - 1 - i];
      else
        fftin[ch][i] = sample; // When window = NoWindow

      Min = fmin(Min, sample);
      Max = fmax(Max, sample);
      if (sample != 0)
        MinNZ = fmin(MinNZ, fabs(sample));
    }
#else
    // This one can be vectorized. Nobody knows how efficient is this anyway, we added extra re-read of array.
    // -g -O3 -mavx2 -ffast-math -fopt-info-vec-optimized -march=native
    // 1. Deserialize first.
    for (uint64_t i = 0; i < fftSize; i++)
    {
      double sample;

      // NOTE Here is point of loss of precision: JACK is float.
      sample = ((float *)buf)[bufReadoutSamplePointer + ch];

      bufReadoutSamplePointer = bufReadoutSamplePointer + channels;
      // Should never be >, only ==, but still.
      if (bufReadoutSamplePointer >= bufSizeInSamples)
        bufReadoutSamplePointer = 0;

      fftin[ch][i] = sample;

      Min = fmin(Min, sample);
      Max = fmax(Max, sample);
      if (sample != 0)
        MinNZ = fmin(MinNZ, fabs(sample));
    }
    // 2a. Apply half of window (forth)...
    for (uint64_t i = 0; i < (fftSize / 2); i++)
      fftin[ch][i] = fftin[ch][i] * windowfunc[planNum][winNum][i];
    // 2b. ... then apply another half of window (backwards).
    for (uint64_t i = (fftSize / 2); i < fftSize; i++)
      fftin[ch][i] = fftin[ch][i] * windowfunc[planNum][winNum][fftSize - 1 - i];
#endif

    inmin[ch] = fmin(Min, inmin[ch]);
    inminAbsNonzero[ch] = fmin(MinNZ, inminAbsNonzero[ch]);
    inmax[ch] = fmax(Max, inmax[ch]);

    if ((inmin[ch] < -1.0) || (inmax[ch] > 1.0))
      amptOverload = ch;

    // Exact match is rare thing, but we behave as precise as possible. Good to check if our samples were not scaled on the road.
    if ((inmin[ch] == -1.0) || (inmax[ch] == 1.0))
      amptMax = ch;

    if (inmax[ch] == 0.0)
      amptZero = ch;

    // Stage 2: Do FFT.
    if (! stopped)
    {
      DBV(F, "Ch. %d Start fftw_execute().", ch);

      fftw_execute(plan[planNum][ch]);

      DBV(F, "Ch. %d Stop fftw_execute().", ch);
    }

    // Stage 3: Post-process FFT result:
    // * Convert complex (i, q) data to power;
    // * Averaging;
    // * Leveling, include Window Noise Figure (NF) apply;
    // * Fit FFT bins to screen bins;
    // * Translate to screen's dB;
    // * Apply video filter;
    // * Store to data array to allow multiple reuse it.
    double winNFbins = 1.0;
    if AVERAGE
      winNFbins = fftWindowNFbins[fftWindow[ch]];

    double coe0 = 10.0 * intDbScale / (double)(2 - isDbPwr);
    double coe1 = log10(1.0 / ((double)fftSize / 2.0)) * 2.0 + log10(1.0 / winNFbins);

    void storeBin(int bin, double power)
    {
      int fftDb = MAX(roundf(coe0 * (power + coe1)), NODATA + 1);
      // Our video filter is per-point IIR LPF.
      // Note: video filter can't work when stopped; it is run time thing.
      if ((vbw > 1) && (vbwContinue))
        data[memCurr][bin][ch] = (fftDb + data[memPrev][bin][ch] * (vbw - 1)) / (float)vbw;
      else if ((vbw == 0) && (vbwContinue)) // Max hold
        data[memCurr][bin][ch] = MAX(fftDb, data[memPrev][bin][ch]);

      else
        data[memCurr][bin][ch] = fftDb;
    }

    // It allow even more correct markers near center, while anyway they will be approximate unless zoomed-in well (narrower span to exact view).
    // float centeringShift = (squeeze) ? (fmod((((double)spanHz / 2.0 + ((startHz < 0) ? - startHz : 0)) * (double)fftSize / (double)sampleRate) - 1.0, 2.0) - 0.5) : 0;
    float centeringShift = 0;

    int firstSampleOffset = (int)(startHz * (float)fftSize / (float)sampleRate + centeringShift);

    int bins = 0;
    double fftPowerBin = -1e6;
    firstUsedBin = -1;
    lastUsedBin = 0;
    for (int sample = 0; sample <= (plotSamplesNum + 1); sample++)
    {
      int bin = squeeze ? (int)(sample * stepAbs / stepRel) : sample;
// if (sample == plotSamplesNum) printf("ch %d plotSamplesNum %d bin %d\n", ch, plotSamplesNum, bin);
      int sampleAbs = sample + firstSampleOffset;

      // The output is n/2+1 complex numbers. [6]
      if ((sampleAbs >= 0) && (sampleAbs <= (fftSize / 2)))
      {
        if (firstUsedBin == -1)
          firstUsedBin = bin;

        // fftout[] is double.
        double ffti = fftout[ch][sampleAbs][0];
        double fftq = fftout[ch][sampleAbs][1];

        if ((ffti == 0) && (fftq == 0))
          data[memCurr][bin][ch] = NODATA + optShowZero;
        else
        {
          double fftPower = log10(ffti*ffti + fftq*fftq);

          if (! squeeze)
          {
            // Exact value.
            storeBin(bin, fftPower);
          }
          else
          {
            // First, we store collected bin (if any) if switch to next bin.
            if ((lastUsedBin == (bin - 1)) && (bins > 0))
            {
              storeBin(lastUsedBin, fftPowerBin);
              fftPowerBin = -1e6;
              bins = 0;
            }
            // More data compress to less video is not easy, uses bins, and can be bin averaging or max method, but not interpolate. See also [1]:p.17, "Note that the averaging must be done with the power spectrum (PS) [...], not with their square roots.
            if AVERAGE
              fftPowerBin = (fftPower + fftPowerBin * bins) / (bins + 1);
            else
              fftPowerBin = fmaxl(fftPower, fftPowerBin);

            bins += 1;
          }
        }
        lastUsedBin = bin;
      }
    }
    // Finally, we store last collected bin, if any.
    if (bins > 0)
      storeBin(lastUsedBin, fftPowerBin);
  }

  void readSpaceAndDrawProgressbar()
  {
    if (info->can_capture)
    {
      readSpace = jack_ringbuffer_read_space (rb);
      chunksToRead = readSpace / chunkSize;

      if ((! (windowBits & (16 + 64))) && (fftsPerSecond < 1))
      {
        int w = 3;
        XFillRectangle(dpy, pm, bgColor, DX, DY - mkrSize - w, xSize, w);
        float rollPos = fmod((rollPhase + readSpace / (float)chunkSize) / roll, 1.0);

        for (int i = 0; i < (rollPos * xSize); i++)
          XDrawPoint(dpy, pm, fontColor[2], DX + i, DY - mkrSize - i % w - 1);

        XFlushArea(DX, DY - mkrSize - w, xSize, w);
      }
    }
    else
      chunksToRead = 0;
  }

  if (optOpengl) {
    if (! glXMakeCurrent(dpy, win, glcontext))
      ERR(O, "glXMakeCurrent() failed!");

    // glEnable(GL_POINT_SMOOTH);
    // glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glViewport(0, 0, winW, winH);
  }

  // Main loop.
  while (1)
  {
    lowCpu = 0;
    goto start;

    while (chunksToRead > 0)
    {
      discardCurrentFft = 0;

      uint64_t gap = bufSize - bufPointer; // bytes before end of buf

      if (gap < chunkSize * chunksToRead)
      {
        jack_ringbuffer_read (rb, buf + bufPointer, gap);
        bufPointer = chunkSize * chunksToRead - gap;
        jack_ringbuffer_read (rb, buf + 0, bufPointer);
      }
      else if (gap == chunkSize * chunksToRead)
      {
        jack_ringbuffer_read (rb, buf + bufPointer, chunkSize * chunksToRead);
        bufPointer = 0;
      }
      else
      {
        jack_ringbuffer_read (rb, buf + bufPointer, chunkSize * chunksToRead);
        bufPointer = bufPointer + chunkSize * chunksToRead;
      }

      rollPhase = (rollPhase + chunksToRead) % roll;

      bufReadoutPointer = bufPointer - fftSize * channels * sample_size_4bytes;
      if (bufReadoutPointer < 0)
        bufReadoutPointer = bufReadoutPointer + bufSize;

      planNum = fftSizeK - MINFFTK;

      if ((memAddScheduled) || ((phosphor > 0) && (phosphor < MAXPHOSPHOR)))
      {
        memCurr = (memCurr + 1) % MAXMEM;

        if (memAddScheduled)
          memQty = MIN(memQty + 1, memorySlots + 1);

        memAddScheduled = 0;
      }

      for (int ch = 0; ch < channels; ch++)
      {
        fftExecuteAndProcessOneChannel(ch);
        readSpaceAndDrawProgressbar();
        if (! stopped)
          processMessages();
        if (discardCurrentFft)
          break;
      }

      if (((! stopped) || (rePlot)) && (! discardCurrentFft))
      {
        newPlot();

        if (optOpengl) {
          XImage *xim;
          xim = XGetImage(dpy, pm, 0, 0, winW, winH, AllPlanes, ZPixmap);
          if (! xim)
              ERR(X, "XGetImage() failed.");

          glLoadIdentity();
          glOrtho(0, winW, 0, winH, -1.0, 1.0);
          glWindowPos2i(0, winH);
          glPixelZoom(1.0, -1.0); // Turn it upside down.
          glBlendFunc(GL_ONE, GL_ZERO); // Disable blending.
          glDrawPixels(winW, winH, GL_BGRA, GL_UNSIGNED_BYTE, (void*)(&(xim->data[0])) );
          XDestroyImage(xim);

          // We have special 0.5 px shifts to match openGL lines with X11.
          // Also we turn it upside down.
          glLoadIdentity();
          glOrtho(0 - 0.5, winW - 0.5, winH - 0.5, 0 - 0.5, -1.0, 1.0);
          // glEnable(GL_COLOR_LOGIC_OP);
          // glLogicOp(GL_OR); // It also works like GXor.
          glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR); // Like GXor or better.
        }

        for (int ch = 0; ch < channels; ch++)
          plotOneChannel(ch);

        // Draw markers on top of all.
        plotOneChannelMkr(mkrCh[0], 0);
        plotOneChannelMkr(mkrCh[1], 1);

        if (! optOpengl)
          XFlushArea(PLOTAREA);
        else
          glXSwapBuffers(dpy, win);

        if ((phosphor > 0) && (phosphor < MAXPHOSPHOR))
          memQty = MIN(memQty + 1, phosphor + 1);

        memPrev = memCurr;
        vbwContinue = 1;

        rePlot = 0;
      }

      lowCpu = 1;
 start:
      readSpaceAndDrawProgressbar();
      processMessages();

      if (programExit)
        goto done;
    }

    /* wait until process() signals more data */
    pthread_cond_wait (&data_ready, &disk_thread_lock);
  }

 done:
  pthread_mutex_unlock (&disk_thread_lock);
  free (buf);

  if (optOpengl)
  {
    glXDestroyContext(dpy, glcontext);
    glXMakeCurrent(dpy, None, NULL);
  }

  return 0;
}


static int
jack_process (jack_nframes_t n_frames, void *arg)
{
  unsigned chn;
  size_t i;
  jack_thread_info_t *info = (jack_thread_info_t *) arg;

  /* Do nothing until we're ready to begin. */
  if ((!info->can_process) || (!info->can_capture))
    return 0;

  for (chn = 0; chn < nports; chn++)
    in[chn] = jack_port_get_buffer (ports[chn], n_frames);

  for (i = 0; i < n_frames; i++)
    for (chn = 0; chn < nports; chn++)
      if (jack_ringbuffer_write (rb, (void *) (in[chn]+i), sample_size_4bytes) < sample_size_4bytes)
        overruns++;

  /* Tell the disk thread there is work to do.  If it is already
   * running, the lock will not be available.  We can't wait
   * here in the process() thread, but we don't need to signal
   * in that case, because the disk thread will read all the
   * data queued before waiting again. */
  if (pthread_mutex_trylock (&disk_thread_lock) == 0)
  {
    pthread_cond_signal (&data_ready);
    pthread_mutex_unlock (&disk_thread_lock);
  }

  return 0;
}

static void signal_handler(int sig)
{
  MSG(S, "Signal: Exit.");
  jack_client_close(client);
  exit(0);
  // goto cleanup;
}

static void jack_shutdown(void *arg)
{
  WRN(J, "JACK Server was lost.");
  exit(0);
  // goto cleanup;
}

static void
run_disk_thread (jack_thread_info_t *info)
{
  info->can_capture = 1;
  pthread_join (info->thread_id, NULL);
  if (overruns > 0)
  {
    WRN(J, "We have %ld overruns. Try rb_size > %d ?", overruns, info->rb_size);
    info->status = EPIPE;
  }
}

static void
setup_ports (int sources, char *source_names[], jack_thread_info_t *info)
{
  unsigned int i;
  size_t in_size;

  /* Allocate data structures that depend on the number of ports. */
  nports = sources;
  ports = (jack_port_t **) malloc (sizeof (jack_port_t *) * nports);
  in_size =  nports * sizeof (jack_default_audio_sample_t *);
  in = (jack_default_audio_sample_t **) malloc (in_size);

  rb = jack_ringbuffer_create (nports * sample_size_4bytes * info->rb_size);

  /* When JACK is running realtime, jack_activate() will have
   * called mlockall() to lock our pages into memory.  But, we
   * still need to touch any newly allocated pages before
   * process() starts using them.  Otherwise, a page fault could
   * create a delay that would force JACK to shut us down. */
  memset(in, 0, in_size);
  memset(rb->buf, 0, rb->size);

  for (i = 0; i < nports; i++)
  {
    char name[64];

    sprintf(name, "input%d", i+1);

    if ((ports[i] = jack_port_register (info->client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0)) == 0)
    {
      jack_client_close (info->client);
      ERR(J, "Cannot register input port '%s'!", name);
    }

    sprintf(portName[i], source_names[i]);

    if (jack_connect (info->client, portName[i], jack_port_name (ports[i])))
    {
      jack_client_close (info->client);
      ERR (J, "Cannot connect input port '%s' to '%s'!", jack_port_name (ports[i]), portName[i]);
    }
  }

  info->can_process = 1;    /* process() can start, now */
}


// #define CALC_GAIN 3 // Set to window number of interest.

// Precision tested, no any benefit from 'long double' yet.
void windowfunc_calc(int planNum, uint64_t size)
{
  // NOTE All windows should have unity gain. Use CALC_GAIN if in doubt.
  // double windowSine(double i, double s) {
  //   return (sin(1.0 * M_PI * i / (s - 1.0)) * (M_PI / 2.0)); }

  double windowBlackman(double ang) {
    return 0.42/0.42 - 0.50/0.42  * cos(2.0 * ang)
          + 0.08/0.42  * cos(4.0 * ang) ; }

  double windowHanning(double ang) {
    return 1.0 - cos(2.0 * ang); }

  double windowFlatTop(double ang) { // 'FTSRS' window
    return 1.0  - 1.93  * cos(2.0 * ang)
          + 1.29  * cos(4.0 * ang)
          - 0.388 * cos(6.0 * ang)
          + 0.028 * cos(8.0 * ang) ; }

  double windowHFT144D(double ang) {
    return 1.0  - 1.96760033 * cos( 2.0 * ang)
          + 1.57983607 * cos( 4.0 * ang)
          - 0.81123644 * cos( 6.0 * ang)
          + 0.22583558 * cos( 8.0 * ang)
          - 0.02773848 * cos(10.0 * ang)
          + 0.00090360 * cos(12.0 * ang) ; }

#ifdef CALC_GAIN
  double windowgain = 0;
#endif

  double half_correction = 0.5 / (size - 1);

  for (uint64_t j = 0; j < size / 2; j++)
  {
    double ang = M_PI * (j - half_correction) / (size - 1.0);

    windowfunc[planNum][0][j] = windowBlackman(ang); // 1.0 if NoWindow
    windowfunc[planNum][1][j] = windowHanning(ang);
    windowfunc[planNum][2][j] = windowFlatTop(ang);
    windowfunc[planNum][3][j] = windowHFT144D(ang);
#ifdef CALC_GAIN
    windowgain = windowgain + windowfunc[planNum][CALC_GAIN][j];
#endif
  }
#ifdef CALC_GAIN
  windowgain = windowgain / size * 2.0;
  printf("Gain = %20.18f\n", windowgain);
#endif
}

// [17]. We should work with both 32 bit RGBA, or 24 bit RGB.
void initOpengl(void)
{
  XVisualInfo *visual_pointer;
  XRenderPictFormat *pict_format;
  GLXFBConfig fbconfig = 0, *fbconfigs;

  int visualData[] = {
      GLX_RENDER_TYPE, GLX_RGBA_BIT,
      GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
      GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
      GLX_DOUBLEBUFFER, True,
      GLX_RED_SIZE, 8,
      GLX_GREEN_SIZE, 8,
      GLX_BLUE_SIZE, 8,
      GLX_ALPHA_SIZE, (optAlpha) * 8,
      // GLX_DEPTH_SIZE, 16,   We do not need depth, it is not alpha.
      GLX_SAMPLE_BUFFERS, (optMsaa > 0),
      GLX_SAMPLES, optMsaa,
      None};

  fbconfig = 0;
  int numfbconfigs;
  fbconfigs = glXChooseFBConfig(dpy, DefaultScreen(dpy), visualData, &numfbconfigs);

  for (int i = 0; i < numfbconfigs; i++)
  {
    visual_pointer = (XVisualInfo*) glXGetVisualFromFBConfig(dpy, fbconfigs[i]);
    if (!visual_pointer)
      continue;

    pict_format = XRenderFindVisualFormat(dpy, visual_pointer->visual);
    if (!pict_format) {
      XFree(visual_pointer);
      continue;
    }

    visual.visual = visual_pointer->visual;
    visual.depth = visual_pointer->depth;
    XFree(visual_pointer);

    fbconfig = fbconfigs[i];
    if ((pict_format->direct.alphaMask > 0) == optAlpha)
      break;
  }

  if (!fbconfig)
      ERR(O, "No matching FB config found!");

  // [8] NOTE: It is not necessary to create or make current to a context before calling glXGetProcAddressARB
  glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
  glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
  glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );

  int attribs[] = { // change it for your needs
      GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
      GLX_CONTEXT_MINOR_VERSION_ARB, 1,
      0};

  glcontext = glXCreateContextAttribsARB(dpy, fbconfig, 0, 1, attribs);

  XSync(dpy, False);
    if (!glcontext)
      ERR(O, "X11 server '%s' does not support OpenGL.", getenv("DISPLAY"));

  XFree(fbconfigs);
}


// JACK stuff and threads are based on [5].
int main(int argc, char *argv[])
{
  int opt,
      tmp0, tmp1, tmp2, tmp3;

  while ((opt = getopt_long_only(argc, argv, shortopts, longopts, NULL)) != -1)
  {
    uint32_t ul = (optarg == NULL) ? 0 : strtoul(optarg, NULL, 10);
    switch (opt)
    {
      case 'h':
        tmp0 = xHzMin;
        tmp1 = xHzMax;
        tmp2 = xGrids;
        tmp3 = xGridSize;

        sscanf(optarg, "%i,%i,%i,%i", &tmp0, &tmp1, &tmp2, &tmp3);
        xGridSize = FIT(tmp3, 4, 256);
        xGrids    = FIT(tmp2, 1, 32);
        xHzMax    = FIT(MAX(tmp0, tmp1), -1000000, 1000000);
        xHzMin    = FIT(MIN(tmp0, tmp1), -1000000, xHzMax - xGrids);
        break;

      case 'D':
        isDbPwr = 1;
      case 'd':
        tmp0 = yDbMin;
        tmp1 = yDbMax;
        tmp2 = yGrids;
        tmp3 = yGridSize;

        sscanf(optarg, "%i,%i,%i,%i", &tmp0, &tmp1, &tmp2, &tmp3);
        yGridSize = FIT(tmp3, 4, 256);
        yGrids    = FIT(tmp2, 1, 32);
        yDbMax    = FIT(MAX(tmp0, tmp1), -320, 100);
        yDbMin    = FIT(MIN(tmp0, tmp1), -320, yDbMax - yGrids);
        break;

      case 'k':     maxFFTK = FIT(ul, MINFFTK, MAXFFTK); break;
      case 'r':     maxRoll = FIT(ul, 1, 256);  break;
      case 'j':        jobs = FIT(ul, 1, 4);    break;
      case 'p':  defPhospor = FIT(ul, 0, 16);   break;
      case 'u': subGridSize = FIT(ul, 2, 10);   break;
      case 's': crtRayStyle = FIT(ul, 0, 7);    break;
      case 'v':     verbose = FIT(ul, 0, 4);    break;
      case 'x':     winXPos = FIT(ul, 0, 4096); break;
      case 'y':     winYPos = FIT(ul, 0, 4096); break;
      case 'c':  fontColors = strtoull(optarg, NULL, 16); break;
      case 'q':   rayColors = strtoull(optarg, NULL, 16); break;
      case 'l':     satLuma = strtoull(optarg, NULL, 16); break;
      case 'm': memorySlots = FIT(ul, 0, MAXMEM - 1);     break;
      case 'o':     opacity = FIT(ul, 0, 100);  break;
      case 'g':     gpuComp = FIT(ul, 0, 100);  break;
      case 'b':  windowBits = FIT(ul, 0, 255);  break;
      case 'M':     optMsaa = FIT(ul, 0, 4);    break;
      case 'A':    optAlpha = FIT(ul, 0, 1);    break;
      case 'O':   optOpengl = 1; break;
      case 'f':  optRayFade = 1; break;
      case 'e': optShowEnob = 1; break;
      case 'z': optShowZero = 1; break;
      case 'w': optRevWheel = 1; break;
      default:
        usage(argv[0]);
        return -1;
    }
  }

// Plot geometry
  if ((yDbMax - yDbMin) % yGrids != 0)
    ERR(P, "dB span %d to grids %d ratio must be integer.", yDbMax - yDbMin, yGrids);

  yDbStep = (yDbMax - yDbMin) / yGrids;

  if ((yDbMax % yDbStep) != 0)
    ERR(P, "dB value %d to grid step %d ratio must be integer.", yDbMax, yDbStep);

  xSize = xGridSize * xGrids;
  ySize = yGridSize * yGrids;

  if ((xSize > MAXXSIZE) || (ySize > MAXYSIZE))
    ERR(P, "Plot size %dx%d exceeds %dx%d.", xSize, ySize, MAXXSIZE, MAXYSIZE);

// JACK Part 1
  const char *client_name = "jasmine-sa";
  const char *server_name = NULL;
  jack_status_t status;

  channels = argc - optind;

  if (channels <= 0)
    ERR(J, "No ports given. Use 'jack_lsp -p | grep -B 1 output' to find some.");

  if (channels > MAXCH)
    ERR(J, "Channels (JACK ports) %ld more than %d.\n", channels, MAXCH);

  client = jack_client_open(client_name, JackNullOption, &status, server_name);

  if (client == NULL)
  {
    if (status & JackServerFailed)
      WRN(J, "Unable to connect to server.");
    ERR(J, "jack_client_open() failed, status = 0x%02x.", status);
  }

  if (status & JackServerStarted)
    MSG(J, "Server started.");

  if (status & JackNameNotUnique)
  {
    client_name = jack_get_client_name(client);
    WRN(J, "Unique name `%s' assigned.", client_name);
  }

  sampleRate = jack_get_sample_rate(client);
  uint64_t periodsize = jack_get_buffer_size(client);
// It is important to keep arrays as small as possible to minimize memory page switch latency effects.
  uint64_t rb_size = (1 << maxFFTK) * channels / sample_size_4bytes * 2;

  MSG(J, "Connected, sampleRate %ld, buf (period) %ld, channels %ld, rb_size %ld.",  sampleRate, periodsize, channels, rb_size);

  jack_thread_info_t thread_info;
  memset (&thread_info, 0, sizeof (thread_info));
  thread_info.rb_size = rb_size;

  thread_info.client = client;
  thread_info.channels = channels;
  thread_info.can_process = 0;

// Init FFT
  if (jobs > 1)
  {
    if (! fftw_init_threads())
      ERR(F, "Thread creation error.");

    fftw_plan_with_nthreads(jobs);
    MSG(F, "Using %d threads.", jobs);
  }

  for (int i = 0; i < channels; i++)
  {
    // The input is n real numbers, while the output is n/2+1 complex numbers. [6]
    fftin[i]  = fftw_alloc_real(1 << maxFFTK);
    // fftw_complex is double.
    fftout[i] = fftw_alloc_complex((1 << maxFFTK) / 2 + 1);
  }

  plans = (maxFFTK - MINFFTK + 1);

  for (int p = 0; p < plans; p++)
  {
    fftSizeK = MINFFTK + p;
    uint64_t size = 1 << fftSizeK; // should be < INT_MAX

    DBG(F, "Plan calculate for %ld points, 4 windows.", size);
    for (int c = 0; c < channels; c++)
      plan[p][c] = fftw_plan_dft_r2c_1d((ulong)(((ulong)(1) << fftSizeK)), fftin[c], fftout[c], FFTW_ESTIMATE);

    for (int w = 0; w < MAXWIN; w++)
      windowfunc[p][w] = fftw_alloc_real(size * sizeof(double) / 2);

    windowfunc_calc(p, size);
  }

// Init GUI.
  const char *title = "Jasmine-SA";

  dpy = XOpenDisplay(0);
  if (! dpy)
    ERR(X, "Can't connect to X server '%s'", getenv("DISPLAY"));

  if (optOpengl)
    initOpengl();
  else
    XMatchVisualInfo(dpy, DefaultScreen(dpy), 32, TrueColor, &visual); // [18]

  attr.colormap = XCreateColormap(dpy, DefaultRootWindow(dpy), visual.visual, AllocNone);

  attr.background_pixmap = None;
  attr.border_pixel = 0;
  int attr_mask = CWColormap | CWEventMask | CWBackPixmap | CWBorderPixel;

  if (windowBits & 16)
    DX = DY = mkrSize;

  winW = DX + xSize + DX + !!(windowBits & 16);
  if (! (windowBits & (16 + 32)))
    winW = winW + LEGENDWIDTH - 2;
  winH = DY + ySize + DX + !!(windowBits & 16); // 32

  win = XCreateWindow(dpy, DefaultRootWindow(dpy), winXPos, winYPos, winW, winH, 0, visual.depth, InputOutput, visual.visual, attr_mask, &attr);
  if (!win)
    ERR(X, "XCreateWindow() failed.");

  // Set Always-On-Top [19]. Do not move it later, will not work randomly.
  if (windowBits & 4)
    XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_STATE", False), XA_ATOM, 32, PropModeReplace, (unsigned char*)(Atom[]){XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False), None}, 1);

  // Set ARGB icon.
  XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_ICON", False), XInternAtom(dpy, "CARDINAL", False), 32, PropModeReplace, (const unsigned char*) icon, 2 + icon[0] * icon[1]);

  XSelectInput(dpy, win, ExposureMask | ButtonPressMask | KeyPressMask);

  XStoreName(dpy, win, title);

  XSizeHints sizehints; // [13] Is XAllocSizeHints() need?
  sizehints.flags = PPosition | PSize | PMinSize | PMaxSize;
  sizehints.x = winXPos;
  sizehints.y = winYPos;
  sizehints.width  = sizehints.min_width  = sizehints.max_width  = winW;
  sizehints.height = sizehints.min_height = sizehints.max_height = winH;
  XSetWMNormalHints(dpy, win, &sizehints);

  XMapWindow(dpy, win);

  XWindowAttributes wa;
  XGetWindowAttributes(dpy, win, &wa);
  MSG(X, "Depth: %d", wa.depth);
  // Optional: Double check the resulting depth, else buffers etc. can fail.
  depth = 24 + optAlpha * 8; // What is required
  if (wa.depth != depth)
  {
    WRN(X, "Got depth %d instead of %d.", wa.depth, depth);
    if (optOpengl)
      WRN(O, "Trying both Alpha & MSAA on Intel GPU?"); // [7]
    depth = wa.depth;
  }

  if ((optOpengl) && (depth < 32) && (opacity < 100))
    WRN(O, "Opacity given, but can't be used with openGL but no Alpha.");

  setFontAndColors();

  pm = XCreatePixmap(dpy, win, winW, winH, wa.depth);
  if (optOpengl)
    pmMkr = XCreatePixmap(dpy, win, mkrW, mkrH, wa.depth);

  wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", 0);
  XSetWMProtocols(dpy, win, &wm_delete_window, 1);

  // [16]. Do not move it earlier, window will be disappear randomly.
  // This one is seldom and not work witn XA_ATOM only on _some_ systems.
  Atom mwh = XInternAtom(dpy, "_MOTIF_WM_HINTS", 0);
  XChangeProperty(dpy, win, mwh, mwh, 32, PropModeReplace, (unsigned char*)(Atom[]){(windowBits & 3), None, None, None, None}, 5);

  if (windowBits & 8)
    XScreenSaverSuspend (dpy, 1);

  DBG(X, "Window %dx%dx%dbpp created.", winW, winH, wa.depth);


// JACK Part 2: Now we know that GUI setup, which takes some time, is done.
  thread_info.can_capture = 0;
  pthread_create (&thread_info.thread_id, NULL, disk_thread, &thread_info);

  jack_set_process_callback (client, jack_process, &thread_info);
  jack_on_shutdown (client, jack_shutdown, &thread_info);

  if (jack_activate(client))
    ERR(J, "Cannot activate client.");

  setup_ports (argc - optind, &argv[optind], &thread_info);

// Init internals
  (spanHz < 0.1 * kHz) ? (units = Hz) : (units = kHz);

  minHz = 0;
  maxHz = sampleRate / 2;
  minSpanHz = xGrids;
  maxSpanHz = sampleRate;

  instrumentReset(1);
  if (! (windowBits & 16))
    sprintf(resultStr, "Greetings! Please read instruction manual before use.");

  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);

// Main job
  run_disk_thread (&thread_info);

// Finish & cleanup
// cleanup:
  jack_client_close (client);
  jack_ringbuffer_free (rb);
  free(ports);
  free(in);

  for (int i = 0; i < channels; i++)
  {
    fftw_free (fftin[i]);
    fftw_free (fftout[i]);
  }

  for (int p = 0; p < plans; p++)
  {
    for (int i = 0; i < MAXWIN; i++)
      fftw_free (windowfunc[p][i]);
    for (int i = 0; i < channels; i++)
      fftw_destroy_plan (plan[p][i]);
  }

  if (jobs > 1)
    fftw_cleanup_threads();

  fftw_cleanup();

  XFreeFont(dpy, xfont); // XUnloadFont() ?
  XFreeGC(dpy, bgColor);
  XFreeGC(dpy, transparentColor);
  for (int i = 0; i < 10; i++)
    XFreeGC(dpy, fontColor[i]);

  for (int ch = 0; ch < MAXCH; ch++)
    for (int grad = 0; grad < GRADIENTS; grad++)
      for (int t = 0; t < 2; t++)
        XFreeGC(dpy, lineColor[ch * GRADIENTS + grad][t]);

  XFreePixmap(dpy, pm);
  if (optOpengl)
    XFreePixmap(dpy, pmMkr);

  if (windowBits & 8)
    XScreenSaverSuspend (dpy, 0);

  XCloseDisplay(dpy);
  return (0);
}


/*
CREDITS:
-------
[1] https://holometer.fnal.gov/GH_FFT.pdf
[2] https://kluedo.ub.rptu.de/frontdoor/deliver/index/docId/4293/file/exact_fft_measurements.pdf
[3] https://www.gaussianwaves.com/2020/09/equivalent-noise-bandwidth-enbw-of-window-functions/
[4] Analog Devices MT-001.pdf
[5] https://github.com/jackaudio/jack-example-tools/blob/main/example-clients/capture_client.c
[6] https://fftw.org/fftw3_doc/One_002dDimensional-DFTs-of-Real-Data.html
[7] https://stackoverflow.com/questions/79065474/transparent-background-and-msaa-at-same-time-for-opengl-window-on-x11
[7a] https://www.reddit.com/r/opengl/comments/1dhwgn1/alpha_to_coverage_and_intel_gpus/
[8] https://www.khronos.org/opengl/wiki/Tutorial:_OpenGL_3.0_Context_Creation_(GLX)
[9] https://github.com/kovidgoyal/kitty/commit/2f0b6e24c904fc84d640ab06646ad8e798cf1943#diff-0dad3090fe3ad3d31d01ba687ffea0cc81029a25bd08dd186d1349c2537eba6fL481
[10] "See https://github.com/glfw/glfw/issues/1538 for why we use pre-multiplied alpha"
[11] http://axonflux.com/handy-rgb-to-hsl-and-rgb-to-hsv-color-model-c
[12] https://stackoverflow.com/a/64090995
[13] https://forums.raspberrypi.com/viewtopic.php?t=357944
[14] https://stackoverflow.com/questions/1157364/intercept-wm-delete-window-on-x11
[15] https://gist.github.com/javiercantero/7753445
[16] https://stackoverflow.com/questions/1904445/borderless-windows-on-linux
[17] https://gist.github.com/meuchel/ad06b8951d6b99e840297d985a3b6b48
[18] https://stackoverflow.com/questions/13395179/empty-or-transparent-window-with-xlib-showing-border-lines-only
[19] https://stackoverflow.com/a/17576405
[20] https://stackoverflow.com/a/18821559
*/
