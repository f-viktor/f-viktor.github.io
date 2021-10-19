---
layout: default
---

# The 411
in this blogpost I'll explain the basics of how to make a fuzzer using libVPX as an example project.

# Fuzzing overview
Fuzzing projects tend to follow the same main steps in most cases.

1. Pick a library and try to build it
2. Identify a suitable fuzzer for the lib (AFL++/LibFuzzer/etc)
3. Build the lib (or part of it) with instrumentation
4. Find a target function
5. Create a suitable seed corpus (+ grammar)
6. The machine turns, the creator rests
7. Check coverage and adjust the seed
8. Go back to step 4

We'll go into more details for most of these steps

## 1. Pick a library and try to build it

[LibVPX](https://github.com/webmproject/libvpx) is the codec used for VP8 and VP9 video decoding/encoding. It's used by a lot of software, including chrome. It's mandatory to implement if you plan on supporting WebRTC, so all major browsers make use of it. WebRTC is also a peer to peer protocol, which means that the video you send, gets decoded directly by your peer's computer. Given all this, an exploit in libVPX's decoder could grant you RCE on the machine of whoever you videocall with. Importantly, the codec is written in a memory-unsafe language.

Thus, it is not surprising that it is included in oss-fuzz, and the decoder is being actively fuzzed by the project. The encoder isn't tho, so let's make that our aim, even though it's kinda pointless in practice.

Let's try to build it. As a first step, it's useful to read the readme. We are basically in the same shoes as any developer at this point, if they can build it somehow, so can we.

```
2. Out-of-tree builds
Out of tree builds are a supported method of building the application. For
an out of tree build, the source tree is kept separate from the object
files produced during compilation. For instance:

  $ mkdir build
  $ cd build
  $ ../libvpx/configure <options>
  $ make

3. Configuration options
The 'configure' script supports a number of options. The --help option can be
used to get a list of supported options:
  $ ../libvpx/configure --help

```
That looks very handy, and we also get configure options for different build presets:
```
--help print this message
--log=yes|no|FILE file configure log is written to [config.log]
--target=TARGET target platform tuple [generic-gnu]
--cpu=CPU optimize for a specific cpu rather than a family
--extra-cflags=ECFLAGS add ECFLAGS to CFLAGS []
--extra-cxxflags=ECXXFLAGS add ECXXFLAGS to CXXFLAGS []
--enable-extra-warnings emit harmless warnings (always non-fatal)
--enable-werror treat warnings as errors, if possible
(not available with all compilers)
--disable-optimizations turn on/off compiler optimization flags
--enable-pic turn on/off Position Independent Code
--enable-ccache turn on/off compiler cache
--enable-debug enable/disable debug mode
--enable-gprof enable/disable gprof profiling instrumentation
--enable-gcov enable/disable gcov coverage instrumentation
--enable-thumb enable/disable building arm assembly in thumb mode
--disable-dependency-tracking disable to speed up one-time build

Codecs:
Codecs can be selectively enabled or disabled individually, or by family:
   --disable-<codec>
is equivalent to:
   --disable-<codec>-encoder
   --disable-<codec>-decoder

Codecs available in this distribution:
   vp8: encoder decoder
   vp9: encoder decoder
```

So I tried to just build as described in the readme and it worked:
```
mkdir build
cd build
../libvpx/configure --enable-vp8 --enable-vp9
make
```

The build results in the vpxenc and vpxdec binaries, and a lot of undeleted object files that were used during the build.

They are self-contained executeables and do not depend on eternal codec libraries:
```
%built ldd ./vpxenc
   linux-vdso.so.1 (0x00007fff447e8000)
   libpthread.so.0 => /usr/lib/libpthread.so.0 (0x00007f69816b2000)
   libstdc++.so.6 => /usr/lib/libstdc++.so.6 (0x00007f69814d5000)
   libm.so.6 => /usr/lib/libm.so.6 (0x00007f698138f000)
   libgcc_s.so.1 => /usr/lib/libgcc_s.so.1 (0x00007f6981375000)
   libc.so.6 => /usr/lib/libc.so.6 (0x00007f69811ac000)
  /lib64/ld-linux-x86-64.so.2 => /usr/lib64/ld-linux-x86-64.so.2 (0x00007f69819c0000)
```

Importantly we also get *libvpx.a*, a static library that we can link against our harness later, so that we don't have to rebuild both at the same time if we want to modify something. If your project doesn't build one of these by default you can use the `ar rcs staticlib.a input.o files.o and.o other.o objects.o ` command to make one.

You can see what objects are in the static library as such:
```
objdump -a libvpx.a | grep "file format"
vpx_decoder.c.o: file format elf64-x86-64
vpx_encoder.c.o: file format elf64-x86-64
vpx_codec.c.o: file format elf64-x86-64
vpx_image.c.o: file format elf64-x86-64
```

Ideally we want 3 build processes:
    1. Building a standalone fuzz-target
    2. Building the library with instrumentation
    3. Building the library for coverage

## 2. Identify a suitable fuzzer for the lib (AFL++/LibFuzzer/etc)
Since this is an open-source C++ library, the classic AFL++ or LibFuzzer would both work fine. Let's use LibFuzzer for the example as it's a bit easier to understand, but AFL is largely the same process that we'll investigate in a different post.


## 3. Build the lib (or part of it) with instrumentation
So we know that we can build a static library from the repo with relative ease. We now want to "instrument" it. This basically means that we want to compile it in a way, that code blocks can report if they have been run. This is useful as our fuzzer will know how many code blocks it reached with a given input, and based on that it will be able to decide whether it's a good or a bad input. To instrument for LibFuzzer, we'll use llvm's clang. clang has compile options that make it trivial to build an instrumented binary. If our project can be built with clang, we'll just have to add a switch.

As a first step it's worth checking if there are already built in options for compilation with clang, if not, you'll have to scrutinize the build process in depth.

In our case `configure` has options for adding `CFLAGS` and `CXXFLAGS` which will be handy once we manage to build with clang:
```
--extra-cflags=ECFLAGS add ECFLAGS to CFLAGS []
--extra-cxxflags=ECXXFLAGS add ECXXFLAGS to CXXFLAGS []
```

But it doesn not have options to overwrite the compiler directly. What we can do however is to overwrite the env variables. Most make based build processes will use whatever is in CC and CXX if anything is set.
```
CC=clang CXX=clang++ LD=clang++ ../libvpx/configure --enable-vp8 --enable-vp9
```
This built the library with clang:
```
readelf --string-dump .comment vpxdec

String dump of section '.comment':
[ 0] GCC: (GNU) 10.2.0
[ 12] clang version 10.0.1
```

So now we can chose the compiler and linker, which means we can use LLVM's libfuzzer.

To actually instrument with LLVM you need to set the `-fsanitize=fuzzer-no-link` compiler option. You may also read people using `-fsanitiz=fuzzer,address` options. `fuzzer` tells the compiler that what you are compiling should be linked as a fuzzer, but in this case we just want to link the library normally with instrumentation, hence the `fuzzer-no-link`. Unfortunately this still presents me some weird linking errors sometimes so I usually just use whichever works :D

the `address` option is for building with ASAN support.

This is where things will start to break so I included here my misguided attempt for demonstration purposes. I tried to build as such:

```
CC=clang CXX=clang++ LD=clang++ ../libvpx/configure --extra-cflags=-fsanitize=fuzzer,address --extra-cxxflags=-fsanitize=fuzzer,address
```

That broke

```
Toolchain is unable to link executables

Configuration failed. This could reflect a misconfiguration of your
toolchains, improper options selected, or another problem. If you
don't see any useful error messages above, the next step is to look
at the configure error log file (config.log) to determine what
configure was trying to do when it died.
```
so let's consult `conf.log`

```
clang -m64 -DNDEBUG -O3 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wdeclaration-after-statement -Wdisabled-optimization -Wfloat-conversion -Wparentheses-equality -Wpointer-arith -Wtype-limits -Wcast-qual -Wvla -Wimplicit-function-declaration -Wmissing-declarations -Wmissing-prototypes -Wuninitialized -Wunreachable-code-loop-increment -Wunused -Wextra -Wundef -Wframe-larger-than=52000 -std=gnu89 -Wshorten-64-to-32 -fsanitize=fuzzer,address -c -o /tmp/vpx-conf-130441-1499.o /tmp/vpx-conf-130441-1499.c
clang++ -m64 -o /tmp/vpx-conf-130441-1499.x /tmp/vpx-conf-130441-1499.o -lpthread
/usr/bin/ld: /tmp/vpx-conf-130441-1499.o: in function `asan.module_ctor':
vpx-conf-130441-1499.c:(.text+0x2): undefined reference to `__asan_init'
/usr/bin/ld: vpx-conf-130441-1499.c:(.text+0x7): undefined reference to `__asan_version_mismatch_check_v8'
/usr/bin/ld: /tmp/vpx-conf-130441-1499.o: in function `sancov.module_ctor_8bit_counters':
vpx-conf-130441-1499.c:(.text.sancov.module_ctor_8bit_counters[sancov.module_ctor_8bit_counters]+0x10): undefined reference to `__sanitizer_cov_8bit_counters_init'
/usr/bin/ld: vpx-conf-130441-1499.c:(.text.sancov.module_ctor_8bit_counters[sancov.module_ctor_8bit_counters]+0x23): undefined reference to `__sanitizer_cov_pcs_init'
clang-10: error: linker command failed with exit code 1 (use -v to see invocation)
```

So the linker apparently cannot find ASAN. This is likely because it's external dependency isn't being linked.

This is because the linker needs to have `-fsanitize=address` set to link ASAN to the binary. To correct this, we need to add LDFLAGS, but there is no option for it, so I just tried to set the env variable and it worked:
```
CC=clang CXX=clang++ LD=clang++ LDFLAGS=-fsanitize=address ../libvpx/configure --extra-cflags=-fsanitize=fuzzer,address --extra-cxxflags=-fsanitize=fuzzer,address
```

To the final build I added some extra flags that I stole from the example fuzzer for faster build times and memory management:
```
Some extra flags for good measure:

CC=clang CXX=clang++ LD=clang++ LDFLAGS="-fsanitize=address -shared-libasan"\
../libvpx/configure \
--extra-cflags="-fsanitize=fuzzer-no-link,address -DVPX_MAX_ALLOCABLE_MEMORY=1073741824" \
--extra-cxxflags="-fsanitize=fuzzer-no-link,address -DVPX_MAX_ALLOCABLE_MEMORY=1073741824" \
--disable-unit-tests \
--disable-webm-io \
--enable-debug \
--disable-vp8-decoder \
--disable-vp9-decoder
```

limiting memory will be useful to not run out of memory during runs,
disabling unnecessary stuff will shorten build time and make it easier to debug
debug symbols will also help with that.


## 4. Find a target function
So we know that we want to fuzz the encoder as it doesn't have a fuzzer yet, but we need to identify the actual function that would be called for encoding. If you are lucky there will be some examples or unit-tests included in the repository that will help you understand how to use certain functions. You can also check how the programs using this codec usually do it. If you have a specific target that uses a given library, it might be a smart idea to mimic exactly how it's being used in your target application. If none of these are available for you, you'll have to go source code diving to understand how the library works.

Luckily for us, we have an `simple_encoder.c` file that we can use as a guideline. Let's first try to understand what it does.

This example file takes a YV12 file and encodes it into IVF. There is a lot of things here to be slimmed down, and make into a fuzzer.

So this is our starting point to create fuzzing input:
```c
// The details of the IVF format have been elided from this example for
// simplicity of presentation, as IVF files will not generally be used by
// your application. In general, an IVF file consists of a file header,
// followed by a variable number of frames. Each frame consists of a frame
// header followed by a variable length payload. The length of the payload
// is specified in the first four bytes of the frame header. The payload is
// the raw compressed data.
```

We also get a bit more insight into how math works, if we can do an integer overflow on the width+height, we may overflow something:

```c
// The frame is read as a continuous block (size width * height * 3 / 2)
// from the input file. If a frame was read (the input file has not hit
// EOF) then the frame is passed to the encoder. Otherwise, a NULL
// is passed, indicating the End-Of-Stream condition to the encoder. The
// `frame_cnt` is reused as the presentation time stamp (PTS) and each
// frame is shown for one frame-time in duration. The flags parameter is
// unused in this example. The deadline is set to VPX_DL_REALTIME to
// make the example run as quickly as possible.
Extremely important for a stable fuzzer:
// Cleanup
// -------
// The `vpx_codec_destroy` call frees any memory allocated by the codec.
```

To start off, we have a few arguments, some of these are clear, the rest I tried to guess based on their names
```c
  int main(int argc, char **argv) {
    FILE *infile = NULL; //input file handle
    vpx_codec_ctx_t codec; //thing that implements codec interface
    vpx_codec_enc_cfg_t cfg; //config of the encoder
    int frame_count = 0;
    vpx_image_t raw;
    vpx_codec_err_t res;
    VpxVideoInfo info = { 0, 0, 0, { 0, 0 } };
    VpxVideoWriter *writer = NULL; //likely the thing that assembles the output frames into a video
    const VpxInterface *encoder = NULL; // does this also implement a codec interface?

    //output video parameters
    const int fps = 30;
    const int bitrate = 200;
    int keyframe_interval = 0;
    int max_frames = 0;
    int frames_encoded = 0;

    // input arguments
    const char *codec_arg = NULL;
    const char *width_arg = NULL;
    const char *height_arg = NULL;
    const char *infile_arg = NULL;
    const char *outfile_arg = NULL;
    const char *keyframe_interval_arg = NULL;
```

After that we are parsing the input arguments, nothing special here although it is weird to me that they are parsing `max_frames` to long and then casting to int.
```c
exec_name = argv[0];

if (argc != 9) die("Invalid number of arguments");

codec_arg = argv[1];
width_arg = argv[2];
height_arg = argv[3];
infile_arg = argv[4];
outfile_arg = argv[5];
keyframe_interval_arg = argv[6];
max_frames = (int)strtol(argv[8], NULL, 0);
```

After this, there is an attempt to instantiate the encoder, we'll likely yoink this into our fuzzer.

```c
encoder = get_vpx_encoder_by_name(codec_arg);
if (!encoder) die("Unsupported codec.");
```

On further investigation it seems like this is a sortof bloat-ish approach to get the encoder, by iterating over all of them and comparing names.

Fuzzing really should be optimized for speed so we may, in the end, just hardwire the encoder much like the decoder example did.

```c
const VpxInterface *get_vpx_encoder_by_name(const char *name) {
int i;

for (i = 0; i < get_vpx_encoder_count(); ++i) {
const VpxInterface *encoder = get_vpx_encoder_by_index(i);
if (strcmp(encoder->name, name) == 0) return encoder;
}
```

After this, there is this `VpxVideoInfo` set up with arguments from the encoder and from the input.

it seems like a struct that holds parameters about the output video? I'm sure we'll see this passed to some encoding function.

```c
info.codec_fourcc = encoder->fourcc;
info.frame_width = (int)strtol(width_arg, NULL, 0);
info.frame_height = (int)strtol(height_arg, NULL, 0);
info.time_base.numerator = 1;
info.time_base.denominator = fps;

if (info.frame_width <= 0 || info.frame_height <= 0 ||
  (info.frame_width % 2) != 0 || (info.frame_height % 2) != 0) {
    die("Invalid frame size: %dx%d", info.frame_width, info.frame_height);
}
```

Next we try to allocate memory for a single `vpx_image`
```c
if (!vpx_img_alloc(&raw, VPX_IMG_FMT_I420, info.frame_width, info.frame_height, 1)) {
    die("Failed to allocate image.");
}
```

It might be interesting to see if `vpx_img_alloc` is just wrapping `malloc` or if it implements its own memory allocation process.

Next we'll set the minimum keyframe interval, the only requirement is that it is a positive number.

The readme notes that keyframes may be placed more frequently regardless, if the encoding process requires that.

Interesting that this is'nt stored in `VpxVideoInfo` but whatever, i guess its not the actual keyframe interval so its logical.

```c
keyframe_interval = (int)strtol(keyframe_interval_arg, NULL, 0);
if (keyframe_interval < 0) die("Invalid keyframe interval value.");
```
Then we'll inform the user about which codec he is using, any printf is gonna get cut from our final fuzzer.
```c
printf("Using %s\n", vpx_codec_iface_name(encoder->codec_interface()));
```

Then it gets the default codec config. `res` is just the error, `cfg` has the config in the end

hopefully this can also be streamlined in the finished product

```c
res = vpx_codec_enc_config_default(encoder->codec_interface(), &cfg, 0);
if (res) die_codec(&codec, "Failed to get default codec config.");
Then cfg gets modified by our arguments
cfg.g_w = info.frame_width;
cfg.g_h = info.frame_height;
cfg.g_timebase.num = info.time_base.numerator;
cfg.g_timebase.den = info.time_base.denominator;
cfg.rc_target_bitrate = bitrate;
cfg.g_error_resilient = (vpx_codec_er_flags_t)strtoul(argv[7], NULL, 0);
```

Open the input and output files for wrting and reading, this will also be unnecessary

```c
writer = vpx_video_writer_open(outfile_arg, kContainerIVF, &info);
if (!writer) die("Failed to open %s for writing.", outfile_arg);
if (!(infile = fopen(infile_arg, "rb")))
die("Failed to open %s for reading.", infile_arg);
Finally initiate the **** codec
if (vpx_codec_enc_init(&codec, encoder->codec_interface(), &cfg, 0))
die("Failed to initialize encoder");
```

Finally encode some **** frames

```c
// Encode frames.
while (vpx_img_read(&raw, infile)) {
  int flags = 0;
  if (keyframe_interval > 0 && frame_count % keyframe_interval == 0)
  flags |= VPX_EFLAG_FORCE_KF;
  encode_frame(&codec, &raw, frame_count++, flags, writer);
  frames_encoded++;
  if (max_frames > 0 && frames_encoded >= max_frames) break;
}
flush the encoder? I guess it will push the last frame out by encoding a null over it
// Flush encoder.
while (encode_frame(&codec, NULL, -1, 0, writer)) {
}
```

bla bla bla

```c
printf("\n");
fclose(infile);
printf("Processed %d frames.\n", frame_count);
Free memory! verrrryyy important! Always free memory!!!!
vpx_img_free(&raw);
if (vpx_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec.");
```

Okay so that seems pretty straightforward, now let's extract the important parts only.

So to create a fuzzer, I removed everything obviously unnecessary and replaced `main()` with `LLVMFuzzerTestOneInput()`.
This function will be the entrypoint for our fuzzer, and should be implemented in every LibFuzzer harness.
The two arguments are the fuzzing input `*data` and it's size `size`.
Therefore I modified calls so that they use the `*data` pointer instead of the infile.
There could be many optimizations in a real fuzzer, but let's see if we can even link this.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vpx/vpx_encoder.h"

#include "../../libvpx/tools_common.h" //maybe unnecessary?
#include "../../libvpx/video_common.h" //maybe unnecessary?

static const char *exec_name;


static void encode_frame(vpx_codec_ctx_t *codec, vpx_image_t *img,
  int frame_index, int flags) {

    const vpx_codec_err_t res =
    vpx_codec_encode(codec, img, frame_index, 1, flags, VPX_DL_GOOD_QUALITY);
    if (res != VPX_CODEC_OK) die_codec(codec, "Failed to encode frame");

    return;
  }


  extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {


    FILE *infile = NULL; //input file handle
    vpx_codec_ctx_t codec; //thing that implements codec itnerface
    vpx_codec_enc_cfg_t cfg; //config of the encoder
    int frame_count = 0;
    vpx_image_t raw;
    vpx_codec_err_t res;
    VpxVideoInfo info = { 0, 0, 0, { 0, 0 } };
    const VpxInterface *encoder = NULL; // does this also implement a codec interface?

    //output video parameters
    const int fps = 30;
    const int width = 60;
    const int height = 60;
    const int bitrate = 200;
    int min_keyframe_interval = 2;
    int max_frames = 10; //making more than 10 frames is redundant, max frames to create will terminate after done
    int frames_encoded = 0;
    const char *codec_arg = "vp8";
    //const char *codec_arg = "vp9";


    //create encoder if it exists
    // we could make all of this in a frist run function
    encoder = get_vpx_encoder_by_name(codec_arg);
    //will become redundant, i just wanna see if it works
    if (!encoder) die("Unsupported codec.");

    // set up vpxinfo
    info.codec_fourcc = encoder->fourcc;
    info.frame_width = width;
    info.frame_height = height;
    info.time_base.numerator = 1;
    info.time_base.denominator = fps;

    // allocate space for a single image in memory
    if (!vpx_img_alloc(&raw, VPX_IMG_FMT_I420, info.frame_width,
      info.frame_height, 1)) {
        die("Failed to allocate image.");
      }

      //gets the encoer config i guess its using the default one
      //we might be able to hard create a config instead or make this only once
      res = vpx_codec_enc_config_default(encoder->codec_interface(), &cfg, 0);
      if (res) die_codec(&codec, "Failed to get default codec config.");

      // overwrite preferred attributes
      cfg.g_w = info.frame_width;
      cfg.g_h = info.frame_height;
      cfg.g_timebase.num = info.time_base.numerator;
      cfg.g_timebase.den = info.time_base.denominator;
      cfg.rc_target_bitrate = bitrate;
      cfg.g_error_resilient = (vpx_codec_er_flags_t)strtoul("1", NULL, 0);

      if (vpx_codec_enc_init(&codec, encoder->codec_interface(), &cfg, 0))
      die("Failed to initialize encoder");

      // Encode frames.

      // so this is really the thing we should do more than once
      while (vpx_img_read(&raw, data)) {

        int flags = 0;
        if (min_keyframe_interval > 0 && frame_count % min_keyframe_interval == 0)
        flags |= VPX_EFLAG_FORCE_KF;
        encode_frame(&codec, &raw, frame_count++, flags);
        frames_encoded++;
        if (max_frames > 0 && frames_encoded >= max_frames) break;
      }

      // Flush encoder.
      encode_frame(&codec, NULL, -1, 0);

      //this may not need to be freed every iteration
      vpx_img_free(&raw);


      //codec may also be
      if (vpx_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec.");
      //vpx_video_writer_close(writer);

      return EXIT_SUCCESS;
    }
```

To try to link it this is my ~~first~~ 80th attempt:

```
clang++ -fsanitize=fuzzer,address -I../../libvpx -I../ -Wl,--start-group \
../../libvpx/examples/vpx_enc_fuzzer.cc -o ./vpx_enc_fuzzer_vp8 \
.\./libvpx.a -Wl,--end-group

vpx_enc_fuzzer.cc:225:10: error: no matching function for call to 'vpx_img_read'
while (vpx_img_read(&raw, data)) {
^~~~~~~~~~~~
../../libvpx/./tools_common.h:151:5: note: candidate function not viable: no known conversion from 'const uint8_t *' (aka 'const unsigned char *') to 'FILE *' (aka '_IO_FILE *') for 2nd argument
int vpx_img_read(vpx_image_t *img, FILE *file);
^
1 error generated.
```

So the `vpx_img_read` that reads the next frame from a `FILE *`, does not like to  get ` const uint8_t *` instead.

So I either need to cast my fuzzing input to a `FILE *` handler, or I have to see what this function does and I need to modify it to work on `uint8_t *`.

This is the function in question from tools common
```c
int vpx_img_read(vpx_image_t *img, FILE*file) {
int plane;

 for (plane = 0; plane < 3; ++plane) {
  unsigned char *buf = img->planes[plane];
  const int stride = img->stride[plane];
  const int w = vpx_img_plane_width(img, plane) *
  ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
  const int h = vpx_img_plane_height(img, plane);
  int y;

  for (y = 0; y < h; ++y) {
    if (fread(buf, 1, w, file) != (size_t)w) return 0;
    buf += stride;
   }
  }

return 1;
}
```
It's doing a fair bit of stuff that I assume make more sense for someone who did a Phd on YV12, but I'm just gonna try to replace the `fread` call here, since that's the only thing that uses the `FILE *`.
```c
int vpx_img_read_from_fuzz(vpx_image_t *img, const uint8_t *file) {
int plane;

for (plane = 0; plane < 3; ++plane) {
 unsigned char *buf = img->planes[plane];
 const int stride = img->stride[plane];
 const int w = vpx_img_plane_width(img, plane) *
 ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
 const int h = vpx_img_plane_height(img, plane);
 int y;

 for (y = 0; y < h; ++y) {
  //if (fread(buf, 1, w, file) != (size_t)w) return 0;
  if (sizeof(file) < (size_t)w) return 0; //if we can't read enough, don't try
   memcpy(buf, file, w); //do fread but from memory
   file += w; //shift the pointer in the array to simulate a stream

   buf += stride;
  }
 }

return 1;
}
```

I ended up replacing the `fread` with `memcpy` there are functions like `fmemopen` but I want to open as few streams as possible.

This is because there is a kernel limit on open filestreams and running the fuzzer in parallel may be limited if too many filestreams are open.

Now it finally compiles, but it doesn't link correctly:
```
clang++ -fsanitize=fuzzer,address -I../../libvpx -I../ -Wl,--start-group \
../../libvpx/examples/vpx_enc_fuzzer.cc -o ./vpx_enc_fuzzer_vp8 \
.\./libvpx.a -Wl,--end-group


/usr/bin/ld: /tmp/vpx_enc_fuzzer-bb9c27.o: in function `vpx_img_read_from_fuzz(vpx_image*, unsigned char const*)':
vpx_enc_fuzzer.cc:(.text._Z22vpx_img_read_from_fuzzP9vpx_imagePKh[_Z22vpx_img_read_from_fuzzP9vpx_imagePKh]+0x12c): undefined reference to `vpx_img_plane_width'
/usr/bin/ld: vpx_enc_fuzzer.cc:(.text._Z22vpx_img_read_from_fuzzP9vpx_imagePKh[_Z22vpx_img_read_from_fuzzP9vpx_imagePKh]+0x1c3): undefined reference to `vpx_img_plane_height'
/usr/bin/ld: /tmp/vpx_enc_fuzzer-bb9c27.o: in function `LLVMFuzzerTestOneInput':
vpx_enc_fuzzer.cc:(.text.LLVMFuzzerTestOneInput[LLVMFuzzerTestOneInput]+0x31b): undefined reference to `get_vpx_encoder_by_name'
/usr/bin/ld: vpx_enc_fuzzer.cc:(.text.LLVMFuzzerTestOneInput[LLVMFuzzerTestOneInput]+0x351): undefined reference to `die'
/usr/bin/ld: vpx_enc_fuzzer.cc:(.text.LLVMFuzzerTestOneInput[LLVMFuzzerTestOneInput]+0x6f5): undefined reference to `die'
/usr/bin/ld: vpx_enc_fuzzer.cc:(.text.LLVMFuzzerTestOneInput[LLVMFuzzerTestOneInput]+0x7ac): undefined reference to `die_codec'
/usr/bin/ld: vpx_enc_fuzzer.cc:(.text.LLVMFuzzerTestOneInput[LLVMFuzzerTestOneInput]+0xc85): undefined reference to `die'
/usr/bin/ld: vpx_enc_fuzzer.cc:(.text.LLVMFuzzerTestOneInput[LLVMFuzzerTestOneInput]+0xeec): undefined reference to `die_codec'
/usr/bin/ld: /tmp/vpx_enc_fuzzer-bb9c27.o: in function `encode_frame(vpx_codec_ctx*, vpx_image*, int, int)':
vpx_enc_fuzzer.cc:(.text._ZL12encode_frameP13vpx_codec_ctxP9vpx_imageii[_ZL12encode_frameP13vpx_codec_ctxP9vpx_imageii$6c121d6a4c153a11e7afe7c7ed6b17be]+0xd8): undefined reference to `die_codec'
clang-10: error: linker command failed with exit code 1 (use -v to see invocation)
```

So it seems to be missing a few die/die_codec commands and some stuff that should be in tools_common. Let's see where these are defined and how we can link them here. Linking the tools_common object however, opens up new problems.

```
clang++ -fsanitize=fuzzer,address -I../../libvpx -I../ -I. -Wl,--start-group \
../../libvpx/examples/vpx_enc_fuzzer.cc -o ./vpx_enc_fuzzer_vp8 \
../libvpx.a ../tools_common.c.o -Wl,--end-group

/usr/bin/ld: ../tools_common.c.o: in function `die':
tools_common.c:(.text+0x17b): undefined reference to `usage_exit'
/usr/bin/ld: ../tools_common.c.o: in function `read_frame':
tools_common.c:(.text.read_frame[read_frame]+0x7a): undefined reference to `y4m_input_fetch_frame'
/usr/bin/ld: ../tools_common.c.o: in function `open_input_file':
tools_common.c:(.text.open_input_file[open_input_file]+0x2d3): undefined reference to `y4m_input_open'
/usr/bin/ld: ../tools_common.c.o: in function `close_input_file':
tools_common.c:(.text.close_input_file[close_input_file]+0x7a): undefined reference to `y4m_input_close'
clang-10: error: linker command failed with exit code 1 (use -v to see invocation)
```

After adding a `y4input` object the linker seems to be somewhat appeased. It seems that usage exit is just something you have to implement if you want to `die`.

```
clang++ -fsanitize=fuzzer,address -I../../libvpx -I../ -I. -Wl,--start-group \
../../libvpx/examples/vpx_enc_fuzzer.cc -o ./vpx_enc_fuzzer_vp8 \
../libvpx.a ../tools_common.c.o ../y4minput.c.o -Wl,--end-group
/usr/bin/ld: ../tools_common.c.o: in function `die':
tools_common.c:(.text+0x17b): undefined reference to `usage_exit'
clang-10: error: linker command failed with exit code 1 (use -v to see invocation)
```
So that's what I did:
```
void usage_exit(void) {
  exit(EXIT_FAILURE);
}
```

And viola, we have a "working" fuzzer:

```
%examples clang++ -fsanitize=fuzzer,address -I../../libvpx -I../ -I. -Wl,--start-group \
../../libvpx/examples/vpx_enc_fuzzer.cc -o ./vpx_enc_fuzzer_vp8 \
../libvpx.a ../tools_common.c.o ../y4minput.c.o -Wl,--end-group


%examples ./vpx_enc_fuzzer_vp8
INFO: Seed: 3103525224
INFO: Loaded 1 modules (37441 inline 8-bit counters): 37441 [0x5622279e4030, 0x5622279ed271),
INFO: Loaded 1 PC tables (37441 PCs): 37441 [0x5622279ed278,0x562227a7f688),
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: A corpus is not provided, starting from an empty corpus
#2 INITED cov: 282 ft: 283 corp: 1/1b exec/s: 0 rss: 35Mb
^C==286085== libFuzzer: run interrupted; exiting
```

The thing is, our coverage is pretty much stuck on 282. Now some might say that 282 is a lot, however in this case, we instrumented the fuzzing harness/ fuzz-target itself, so that 282 is likely just the very basic run of our harness. This means that something is not right, and there is actually a bug in my fuzzing harness. To see what it is, let's create a coverage build.

## ~~5. Create a suitable seed corpus (+ grammar)~~
## 7. Check coverage and adjust the seed


To see what our fuzzer covers, we need to instrument the binaries for coverage calculation.

LLVM provides the `-fcoverage-mapping` switch to do this

We can also add a few other flags to make the binary more verbose

```
-O1 -fno-omit-frame-pointer -g -ggdb3 -fprofile-instr-generate -fcoverage-mapping
```

Let's rebuild the entire library like this to be sure we cover correctly.

```
CC=clang CXX=clang++ LD=clang++ \
../libvpx/configure \
--extra-cflags="-O1 -fno-omit-frame-pointer -g -ggdb3 -fprofile-instr-generate -fcoverage-mapping \
-DVPX_MAX_ALLOCABLE_MEMORY=1073741824" \
--extra-cxxflags="-O1 -fno-omit-frame-pointer -g -ggdb3 -fprofile-instr-generate -fcoverage-mapping \
-DVPX_MAX_ALLOCABLE_MEMORY=1073741824" \
--enable-debug \
--disable-vp8-decoder \
--disable-vp9-decoder \
--disable-webm-io \
--disable-unit-tests
```

Then let's rebuild the fuzzer with coverage enabled, I added `-lpthread` as it was crying about it.

```
clang++ -O1 -fno-omit-frame-pointer -g -ggdb3 -fprofile-instr-generate -fcoverage-mapping \
-fsanitize=fuzzer \
-I../../libvpx -I../ -I. -Wl,--start-group \
../../libvpx/examples/vpx_enc_fuzzer.cc -o ./vpx_enc_fuzzer_vp8_cov \
../libvpx.a ../tools_common.c.o ../y4minput.c.o -Wl,--end-group \
-lpthread
```
So will this work? Let's try to use it on the fuzzing entry point `LLVMFuzzerTestOneInput`

```
./vpx_enc_fuzzer_vp8_cov ../../llvmbuilt/fuzz/corpus/* && \
llvm-profdata merge -sparse *.profraw -o default.profdata && \
llvm-cov show ./vpx_enc_fuzzer_vp8_cov -instr-profile=default.profdata -name=LLVMFuzzerTestOneInput
```

Sample from the output:

![coverage1](/img/libvpxfuzz/1.png)


So it does look like it's working What's more we are avoiding all of the code paths that invoke `die_codec` so that's a good thing

What isn't such a good thing is that we are also avoiding the code path for literally the whole point of this entire thing:

![coverage2](/img/libvpxfuzz/2.png)


So this would explain the static coverage, our input does absolutely nothing as it's never used.
Let's see our modified read function that always fails here in the "while"
```
./vpx_enc_fuzzer_vp8_cov ../../llvmbuilt/fuzz/corpus/* && \
llvm-profdata merge -sparse *.profraw -o default.profdata && \
llvm-cov show ./vpx_enc_fuzzer_vp8_cov -instr-profile=default.profdata -name=vpx_img_read_from_fuzz
```

![coverage3](/img/libvpxfuzz/3.png)

I guess that comparison doesn't really work as I intended it to work. Let's try a quick fix:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vpx/vpx_encoder.h"

#include "../../libvpx/tools_common.h" //maybe unnecessary?
#include "../../libvpx/video_common.h" //maybe unnecessary?

//#include "../video_writer.h" unnecesary as we do not write anything

void usage_exit(void) {

  exit(EXIT_FAILURE);

}


int vpx_img_read_from_fuzz(vpx_image_t *img, const uint8_t *file, size_t size, int *readoffset) {
  int plane;

  for (plane = 0; plane < 3; ++plane) {
    unsigned char *buf = img->planes[plane];
    const int stride = img->stride[plane];
    const int w = vpx_img_plane_width(img, plane) *
    ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = vpx_img_plane_height(img, plane);
    int y;

    for (y = 0; y < h; ++y) {
      //if (fread(buf, 1, w, file) != (size_t)w) return 0;
      if (size < (size_t)(w+*readoffset)) return 0; //if we cant read enough, dont try
      memcpy(buf, file+*readoffset, w); //do fread but from memory
      *readoffset += w; //shift the pointer in the array to simulate a stream

      buf += stride;
    }
  }

  return 1;
}

static void encode_frame(vpx_codec_ctx_t *codec, vpx_image_t *img,
  int frame_index, int flags) {
    // int got_pkts = 0;
    // vpx_codec_iter_t iter = NULL;
    // const vpx_codec_cx_pkt_t *pkt = NULL;
    const vpx_codec_err_t res =
    vpx_codec_encode(codec, img, frame_index, 1, flags, VPX_DL_GOOD_QUALITY);
    if (res != VPX_CODEC_OK) die_codec(codec, "Failed to encode frame");

    return;
  }

  // TODO(tomfinegan): Improve command line parsing and add args for bitrate/fps.
  //int main(int argc, char **argv) {
  extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    int readoffset = 0;
    FILE *infile = NULL; //input file handle
    vpx_codec_ctx_t codec; //thing that implements codec itnerface
    vpx_codec_enc_cfg_t cfg; //config of the encoder
    int frame_count = 0;
    vpx_image_t raw;
    vpx_codec_err_t res;
    VpxVideoInfo info = { 0, 0, 0, { 0, 0 } };

    // VpxVideoWriter *writer = NULL; //likely the thing that assambles the output frames into a video
    const VpxInterface *encoder = NULL; // does this also implement a codec interface?

    //output video parameters
    const int fps = 30;
    const int width = 60;
    const int height = 60;
    const int bitrate = 200;
    int min_keyframe_interval = 2;
    int max_frames = 10; //making more than 10 frames is redundant, max frames to create will terminate after done
    int frames_encoded = 0;
    const char *codec_arg = "vp8";
    //const char *codec_arg = "vp9";


    //create encoder if it exists
    // we could make all of this in a frist run function
    encoder = get_vpx_encoder_by_name(codec_arg);
    //will become redundant, i just wanna see if it works
    if (!encoder) die("Unsupported codec.");

    // set up vpxinfo
    info.codec_fourcc = encoder->fourcc;
    info.frame_width = width;
    info.frame_height = height;
    info.time_base.numerator = 1;
    info.time_base.denominator = fps;

    // allocate space for a single image in memory
    if (!vpx_img_alloc(&raw, VPX_IMG_FMT_I420, info.frame_width,
      info.frame_height, 1)) {
        die("Failed to allocate image.");
      }

      //gets the encoer config i guess its using the default one
      //we might be able to hard create a config instead or make this only once
      res = vpx_codec_enc_config_default(encoder->codec_interface(), &cfg, 0);
      if (res) die_codec(&codec, "Failed to get default codec config.");

      // overwrite preferred attributes
      cfg.g_w = info.frame_width;
      cfg.g_h = info.frame_height;
      cfg.g_timebase.num = info.time_base.numerator;
      cfg.g_timebase.den = info.time_base.denominator;
      cfg.rc_target_bitrate = bitrate;
      cfg.g_error_resilient = (vpx_codec_er_flags_t)strtoul("1", NULL, 0);

      if (vpx_codec_enc_init(&codec, encoder->codec_interface(), &cfg, 0))
      die("Failed to initialize encoder");

      // Encode frames.
      //while (vpx_img_read(&raw, infile)) {
        // so this is really the thing we should do more than once
        while (vpx_img_read_from_fuzz(&raw, data, size, &readoffset)) {

          int flags = 0;
        if (min_keyframe_interval > 0 && frame_count % min_keyframe_interval == 0) // make every other frame a keyframe
          flags |= VPX_EFLAG_FORCE_KF;
          encode_frame(&codec, &raw, frame_count++, flags);
          frames_encoded++;
          if (max_frames > 0 && frames_encoded >= max_frames) break;
        }

        // Flush encoder.
        encode_frame(&codec, NULL, -1, 0);

        //this may not need to be freed every iteration
        vpx_img_free(&raw);


        //codec may also be
        if (vpx_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec.");
        //vpx_video_writer_close(writer);

        return EXIT_SUCCESS;
      }
```

And now we broke through the 282 barrier:

```
./vpx_enc_fuzzer_vp8 corpus
INFO: Seed: 2027026999
INFO: Loaded 1 modules (37441 inline 8-bit counters): 37441 [0x55df7ec16030, 0x55df7ec1f271),
INFO: Loaded 1 PC tables (37441 PCs): 37441 [0x55df7ec1f278,0x55df7ecb1688),
INFO: 8 files found in corpus
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: seed corpus: files: 8 min: 1b max: 25b total: 84b rss: 34Mb
#9 INITED cov: 282 ft: 283 corp: 1/1b exec/s: 0 rss: 39Mb
#5979 NEW cov: 283 ft: 284 corp: 2/64b lim: 63 exec/s: 2989 rss: 261Mb L: 63/63 MS: 5 ShuffleBytes-InsertByte-ChangeASCIIInt-ChangeBit-CrossOver-
#6021 REDUCE cov: 283 ft: 284 corp: 2/63b lim: 63 exec/s: 3010 rss: 261Mb L: 62/62 MS: 2 ShuffleBytes-EraseBytes-
```
Let's see the coverage on this, 283 is still not a whole lot.

![coverage4](/img/libvpxfuzz/4.png)

That looks... better I guess, although I don't really understand how we managed to hit return 1 both 0  and 15 times. I guess we hit the semicolon 15 times but not the rest of the statement? :D

The real problem is that we are still just fuzzing our own binary for instead of the target lib:

![coverage5](/img/libvpxfuzz/5.png)

So apparently, the pproblem was that i didnt have an input file large enough.

After adding a somewhat larger input file, the coverage expanded significantly, I think my inputs simply were smaller than a standard frame so nothing was ever read from them.

```
%fuzz ./vpx_enc_fuzzer_vp8 corpus
INFO: Seed: 3496353863
INFO: Loaded 1 modules (37441 inline 8-bit counters): 37441 [0x560e21901030, 0x560e2190a271),
INFO: Loaded 1 PC tables (37441 PCs): 37441 [0x560e2190a278,0x560e2199c688),
INFO: 16 files found in corpus
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 14418 bytes
INFO: seed corpus: files: 16 min: 1b max: 14418b total: 22060b rss: 34Mb
#17 INITED cov: 1890 ft: 1963 corp: 10/21Kb exec/s: 0 rss: 44Mb
NEW_FUNC[1/5]: 0x560e21299bb0 in vp8_quantize_mby (/home/dna/git/libvpx/llvmbuilt/fuzz/vpx_enc_fuzzer_vp8+0x2a6bb0)
NEW_FUNC[2/5]: 0x560e2156f610 in vp8_encode_intra16x16mby (/home/dna/git/libvpx/llvmbuilt/fuzz/vpx_enc_fuzzer_vp8+0x57c610)
#24 NEW cov: 1960 ft: 2453 corp: 11/35Kb lim: 14418 exec/s: 0 rss: 49Mb L: 14418/14418 MS: 2 CopyPart-CrossOver-
```
On that bombshell, let's get back to our regular programming:

## 5. Create a suitable seed corpus (+ grammar)
The best seed is usually a small sample of valid inputs. The problem with YV12 is that there simply aren't many exapmle files I could use as it's an intermediary raw format that gets compressed by wrappers such as mp4 etc. So finding a raw YV12 video is just not very easy.

From the readme:
```
VP8/VP9 TEST VECTORS: The test vectors can be downloaded and verified using the build system after running configure. To specify an alternate directory the LIBVPX_TEST_DATA_PATH environment variable can be used. $ ./configure --enable-unit-tests $ LIBVPX_TEST_DATA_PATH=../libvpx-test-data make testdata
```

But we'll need YV12 files to seed the encoder

https://wiki.videolan.org/YUV#YUV_4:2:0_.28I420.2FJ420.2FYV12.29

Something similar would also probably suffice like j420 etc

Because nobody actually uses these formats in raw, they are kinda hard to come by.
I guesssss we could try to make our own file based on the description if worst comes to worst.

I found this, idk if this is even what i need:

http://trace.eas.asu.edu/yuv/

added a .yuv from here cut to 4000 bytes and it seems to have done the trick

after this generate 1368 possible inputs, I've decided to merge them :
```
./vpx_enc_fuzzer_vp8 merged corpus -merge=1
```
You may also want to set up a grammar for your seed, with libprotobuf but that's out of the scope of this blogpost, it's already waaaay too long.
Anyway just do your best with this part... it's important :D

## 6. The machine turns, the creator rests
So realistically you would run this on something with a lot of CPU cores and memory and wait a bit. How long? A day, a week, a month, however long you have. The hard part really comes when something does actually crash, you'll need to find out how and why it happened, and then you'll have to exploit it. While you wait feel free to check your coverage to see if the fuzzer got stuck on something, and maybe you can provide an input that passes that specific check. You may also start to fuzz other function calls in the library.
