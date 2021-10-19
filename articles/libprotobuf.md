---
layout: default
---

# Introduction

[Please](https://github.com/google/fuzzing/blob/master/tutorial/libFuzzerTutorial.md)
[read](https://github.com/google/fuzzing/blob/master/docs/structure-aware-fuzzing.md)
[all](https://bshastry.github.io/2019/01/18/Deconstructing-LPM.html)
[of](http://llvm.org/devmtg/2017-10/slides/Serebryany-Structure-aware%20fuzzing%20for%20Clang%20and%20LLVM%20with%20libprotobuf-mutator.pdf)
[these](https://chromium.googlesource.com/chromium/src/testing/libfuzzer/+/7fe0c11b615abbe1c3d6543cff1474cacd9fb870/efficient_fuzzer.md)
[pages](https://llvm.org/docs/LibFuzzer.html)

Unfortunately there is no quick and easy way to learn this, you'll have to become reasonably familiar with make/cmake depending on the project and a whole bunch of llvm/clang switches. When starting on a new fuzzzing project the initial investment is relatively big, you'll have to learn how to build the project, what parts of it are worth fuzzing, and what inputs those parts expect. It's also worth clarifying that if your project cannot be built via clang (is not C/C++ code) then this approach is not for you. I will be demonstrating this on the [janus-gateway](https://github.com/meetecho/janus-gateway) project, which is a pretty cool WebRTC server, but you can use any C/C++ project.

# Tools

## libfuzzer

https://llvm.org/docs/LibFuzzer.html

libfuzzer is a coverage-based fuzzing engine similar to AFL but with it's own mutation engine. The main difference is that while AFL primarily works on input pipes, libfuzzer requires you to build separate fuzz target binaries that invoke functions from the target application. This way fuzzing is specific to chosen functions and their dependencies. Can be identified by the call of something like this:

```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size){

 targetFunctionToBeFuzzed(data, size);

}
```
Comes bundled in llvm, no additonal isntallation necessary

## protocol-buffers

https://developers.google.com/protocol-buffers

Protocol-buffers (protobuf) is an structure description language. You can describe a structure with optional or repeating fields in it. This allows you to have a large set of formally correct but very varied subset or superset of elements from the set of all formally correct entities. With this, you can constrain libfuzzer to only create fuzzed inputs within a correctly defined structure, so that it doesn't try to send "A"\*999 into a field that requires e.g. XML formatted input. This obviously excludese some trivial cases, but also tries less-trivial cases with far higher efficiency. For example:

```proto
syntax = "proto2";

package janus_try;


message Msg {

  optional float optional_float = 1;

  optional uint64 optional_uint64 = 2;

  optional string optional_string = 3;

}
```

This structure can later be compiled into a variety of languages, including C++

Linking this into C++ projects requres the -lprotobuf switch!

It may already be installed on your system, it's located in /usr/include/google usually.

## libprotobuf-mutator

https://github.com/google/libprotobuf-mutator

https://chromium.googlesource.com/chromium/src/+/master/testing/libfuzzer/libprotobuf-mutator.md

Libprotobuf-mutator is what brings these two together. It provides a macro for libfuzzer, that can be called in the fuzztarget to generate protobuf formatted fuzz objects. This looks as such:

```c
#include "src/libfuzzer/libfuzzer_macro.h"

DEFINE_PROTO_FUZZER(const MyMessageType& input) {
  // Code which needs to be fuzzed.
  ConsumeMyMessageType(input);
}
```

I found it easier to download the git repo and build it myself, but you can also install it via ninja.

# Using the tools
## Compiling a protobuf file
Create a test proto file, for example, copy the one above, and compile it. To compile a protobuf file `<test>.proto` enter the following command:

```
protoc  -I=$PWD --cpp_out=$PWD $PWD/<test>.proto
```
This should create `<test>.pb.cc` and `<test>.pb.h` files. You'll later have to return and fill in the .proto file with the required strucutre and re-compile it.

## Linking the protobuf file in the fuzztarget

To link the protobuf file, you'll want to include the header (in this case test.pb.h) it in your <fuzztarget>.cc file like so:
```c
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include "../debug.h"
#include "../sdp-utils.h"


#include "test.pb.h"

#include "src/libfuzzer/libfuzzer_macro.h"
protobuf_mutator::protobuf::LogSilencer log_silincer;

int janus_log_level = LOG_NONE;
gboolean janus_log_timestamps = FALSE;
gboolean janus_log_colors = FALSE;
int refcount_debug = 0;

DEFINE_PROTO_FUZZER(const janus_try::Msg& data) {

        static PostProcessorRegistration reg = {
      [](janus_try::Msg* message, unsigned int seed) {
              int i=1;
      }};
        char error_str[512];
        int size = sizeof(data);
        char sdp_string[size];
        memcpy(sdp_string, (void*)&data, size);


         // Code which needs to be fuzzed.
        janus_sdp *parsed_sdp = janus_sdp_parse((const char *)sdp_string, error_str, sizeof(error_str));
}
```
Don't worry about the rest of the file for now. You'll also need to link the `<test>.pb.cc` file at linking time, something like this:

1. Compiling fuzztarget.cc
```
clang -c -O1 -fno-omit-frame-pointer -g -ggdb3 -fsanitize=address,undefined -fsanitize-address-use-after-scope -fno-sanitize-recover=undefined -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION  -fsanitize=fuzzer-no-link -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include  -I. -I./libprotobuf-mutator/ -I/home/dna/git/janus/janus-protobuf /home/dna/git/janus/janus-protobuf/fuzzers/fuzztarget.cc -o /home/dna/git/janus/janus-protobuf/fuzzers/fuzztarget.o
```

2. Compiling test.pb.cc
```
clang -c -O1 -fno-omit-frame-pointer -g -ggdb3 -fsanitize=address,undefined -fsanitize-address-use-after-scope -fno-sanitize-recover=undefined -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION  -fsanitize=fuzzer-no-link -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include  -I. -I./libprotobuf-mutator/ -I/home/dna/git/janus/janus-protobuf test.pb.cc -o /home/dna/git/janus/janus-protobuf/fuzzers/test.pb.o
```

3. Linking them together (note the -lprotobuf switch at the end)
```
clang++ -O1 -fno-omit-frame-pointer -g -ggdb3 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fsanitize-address-use-after-scope  -fsanitize=fuzzer /home/dna/git/janus/janus-protobuf/fuzzers/fuzztarget.o /home/dna/git/janus/janus-protobuf/fuzzers/test.pb.o -o /home/dna/git/janus/janus-protobuf/fuzzers/out/fuzztarget  /home/dna/git/janus/janus-protobuf/fuzzers/janus-lib.a -lglib-2.0 -ljansson -lz  -pthread -lprotobuf
```

## building libprotobuf-mutator (you can skip this if you installed it)
1. cd into the home directory
2. cmake
3. ./configure
4. make

you should now have a ./libprotobuf-mutator/src/libprotobuf-mutator.a file.

## linking libprotobuf-mutator

```
clang++ -O1 -fno-omit-frame-pointer -g -ggdb3 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fsanitize-address-use-after-scope  -fsanitize=fuzzer /home/dna/git/janus/janus-protobuf/fuzzers/fuzztarget.o /home/dna/git/janus/janus-protobuf/fuzzers/test.pb.o /home/dna/git/janus/janus-protobuf/fuzzers/libprotobuf-mutator/src/libprotobuf-mutator.a -o /home/dna/git/janus/janus-protobuf/fuzzers/out/fuzztarget  /home/dna/git/janus/janus-protobuf/fuzzers/janus-lib.a -lglib-2.0 -ljansson -lz  -pthread -lprotobuf
```

Also add this to the build path of your fuzztarget.cc
```c
#include "src/libfuzzer/libfuzzer_macro.h"
```
then do
```
clang -c -O1 -fno-omit-frame-pointer -g -ggdb3 -fsanitize=address,undefined -fsanitize-address-use-after-scope -fno-sanitize-recover=undefined -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION  -fsanitize=fuzzer-no-link -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include  -I. -I./libprotobuf-mutator/ -I/home/dna/git/janus/janus-protobuf test.pb.cc -o /home/dna/git/janus/janus-protobuf/fuzzers/test.pb.o
```

you'll also have to link libfuzzer-mutator.o and libfuzzer-macr.o separately (these are all redundant if you already installed libprotobuf-mutator and included it via a flag).

```
clang++ -O1 -fno-omit-frame-pointer -g -ggdb3 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fsanitize-address-use-after-scope  -fsanitize=fuzzer /home/dna/git/janus/janus-protobuf/fuzzers/fuzztarget.o /home/dna/git/janus/janus-protobuf/fuzzers/test.pb.o  /home/dna/git/janus/janus-protobuf/fuzzers/libfuzzer_macro.o /home/dna/git/janus/janus-protobuf/fuzzers/libfuzzer_mutator.o /home/dna/git/janus/janus-protobuf/fuzzers/libprotobuf-mutator/src/libprotobuf-mutator.a  -o /home/dna/git/janus/janus-protobuf/fuzzers/out/fuzztarget  /home/dna/git/janus/janus-protobuf/fuzzers/janus-lib.a -lglib-2.0 -ljansson -lz  -pthread -lprotobuf
```

At this point, the linking step should only complain about not knowing the actual function you are trying to fuzz, and like some weird issue about a single test function being unknown. These only should happen if your target application is in C, and your fuzzer is in C++ (C is not supported natively by the fuzzer).

# Linking all of this to your target application

You'll have to find your targeted function and all it's dependencies for this to work. Ideally you would compile this into a single .a static library to be included at linking.

you can ccreate a .a archive via
```
ar rcs outputfile.a input.o files.o and.o other.o libs.o
```
Here's how you'd do that:

1.    pick a function you want to fuzz. In this case it will be janus_sdp_parse()
2.    Identify the file that implements that function. In this case, sdp-utils.c
3.    Based on the makefile, this file will compile into janus-sdp-utils.o
4.    also compile any other dependencies you identify if necessary. I found it easier to just run make, and chose from the resuling files.
5.    Run ar rcs output_static_lib.a janus-sdp-utils.o other.o dependencies.o and.o stuff.o
6.    link the resulting static library at the linking of your fuzzer as such:

```
clang++ -O1 -fno-omit-frame-pointer -g -ggdb3 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fsanitize-address-use-after-scope  -fsanitize=fuzzer /home/dna/git/janus/janus-protobuf/fuzzers/fuzztarget.o /home/dna/git/janus/janus-protobuf/fuzzers/test.pb.o  /home/dna/git/janus/janus-protobuf/fuzzers/libfuzzer_macro.o /home/dna/git/janus/janus-protobuf/fuzzers/libfuzzer_mutator.o /home/dna/git/janus/janus-protobuf/fuzzers/libprotobuf-mutator/src/libprotobuf-mutator.a  -o /home/dna/git/janus/janus-protobuf/fuzzers/out/fuzztarget  /home/dna/git/janus/janus-protobuf/fuzzers/janus-lib.a -lglib-2.0 -ljansson -lz  -pthread -lprotobuf
```
If your application was made in C, then you'll also have to modify the target function and add directives
```
// this is here so that the c++ fuzzer compiler understands the block as c++
#ifdef __cplusplus
extern "C" {
#endif

janus_sdp *janus_sdp_parse(const char *sdp, char *error, size_t errlen);

#ifdef __cplusplus
}
#endif
```

it is much nicer if you actually do this around your `#includes` inststead

This will make the C++ compiler of the fuzzer see the target function.

If you do this correctly, the resulting output binary should be your fuzzer.

## contents of fuzztarget

```c
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include "../debug.h"
#include "../sdp-utils.h"


#include "sdp.pb.h"
#include "src/libfuzzer/libfuzzer_macro.h"
protobuf_mutator::protobuf::LogSilencer log_silincer;


int janus_log_level = LOG_NONE;
gboolean janus_log_timestamps = FALSE;
gboolean janus_log_colors = FALSE;
int refcount_debug = 0;


std::string convertProtobufToSDP(session_description_protocol::SDP data){

//I defined a pretty big and ugly function here to cnvert a protobuf class into a string that the targeted function expects

}

DEFINE_PROTO_FUZZER(const session_description_protocol::SDP& data) {
        /* Free resources */
        static PostProcessorRegistration reg = {
        [](session_description_protocol::SDP* message, unsigned int seed) {
             //message->set_version(0); //sets version to int 0, and will be visible in corpus. (try other values tho :D)
        }};

        //post processing on protobuf messages
        char error_str[512];

        //convert protobuf to valid string sdp
        std::string ret_string = convertProtobufToSDP(data);
        char sdp_string[ret_string.size()+1];
        strcpy(sdp_string,ret_string.c_str());

        // Code which needs to be fuzzed
        janus_sdp *parsed_sdp = janus_sdp_parse((const char *)sdp_string, error_str, sizeof(error_str));
        char *generated_sdp = janus_sdp_write(parsed_sdp);

        //if you dont free stuff you are going to run out of memory
        janus_sdp_destroy(parsed_sdp);
        g_free(generated_sdp);
}
```

# Setting up your .proto file

You basically want to describe your desired format in google's protocol-buffer language. For example the SDP message protocol would look simething like this:

```proto
syntax = "proto2";
package session_description_protocol;

message SDP {

  // v=  (protocol version number, currently only 0) "v=0\n"
  required int32 version = 1;

  message Originator {
        required string name = 1;
        required int32 username = 2;
        required int32 id = 3;
        required string version = 4;
        required string networkaddr =5; // maybe have it as int32 and split it apart in post?
  }

  // o=  (originator and session identifier : username, id, version number, network address)
  required Originator originator = 2;

  // s=  (session name : mandatory with at least one UTF-8-encoded character) "s=-"
  required string session = 3;

  // i=* (session title or short information)
  optional string info = 4;

  // u=* (URI of description)
  optional string uri = 5;

  // e=* (zero or more email address with optional name of contacts)
  repeated string email = 6;

  // p=* (zero or more phone number with optional name of contacts)
  repeated string phoneNum = 7;

  // c=* (connection information??not required if included in all media)
  optional string connInfo = 8;

  // b=* (zero or more bandwidth information lines)
  repeated string bandwidthInfo = 9;

  // One or more Time descriptions ("t=" and "r=" lines; see below)
  message TimeDescription{

        // t=  (time the session is active)
        required int32 time = 1;

        // r=* (zero or more repeat times) change to string for other format?
        repeated int32 times = 2;
  }

  required TimeDescription mandatoryTime = 10;
  repeated TimeDescription optionalTime = 11;

  // z=* (time zone adjustments)
  optional int32 zoneAdjustment = 12;

  // k=* (encryption key)
  optional string key = 13;

  // a=* (zero or more session attribute lines)
  repeated string attribute =14;

  // Zero or more Media descriptions (each one starting by an "m=" line; see below)
  message MediaDescription {
        // m=  (media name and transport address)
        required string mediaName = 1;

        // i=* (media title or information field)
        optional string mediaInfo = 2;

        // c=* (connection information ?? optional if included at session level)
        optional string mediaConnInfo = 3;

        // b=* (zero or more bandwidth information lines)}
        repeated string mediaBandwidthInfo = 4;

        // k=* (encryption key)
        optional string mediaKey = 5;

        //    a=* (zero or more media attribute lines ?? overriding the Session attribute lines)
        repeated string mediaAttribute = 6;
  }
  repeated MediaDescription mediaDescription = 15;
}
```
The more accurate your format is, the less unnecessary tests are performed, but you can leave in some incorrect formats if you think the parser my crash on those errors.

# Running the fuzzer

Running the fuzzer is basically the same as running libfuzzer

# Checking for coverage
To see what you're doing and what parts of the target function is actually hit, you should probably use llvm-cov

For example with janus, this would look as follows:

```
./out/janus_cover ./out/janus_fuzzer_corpus/* && \
llvm-profdata merge -sparse *.profraw -o default.profdata && \
llvm-cov show ./out/janus_cover -instr-profile=default.profdata -name=janus_process_incoming_admin_request
```

This will visualize how many times each line of code is hit. Red means that it was hit 0 times = bad

For this to work you'll have to build a separate binary with the following flags:
```
COVERAGE_CFLAGS="-O1 -fno-omit-frame-pointer -g -ggdb3 -fprofile-instr-generate -fcoverage-mapping"
```
Everything else is the same, just set these when compiling everything instead of the `-fsanitize` flags

After your fuzzer ran a bit and you have a lot of corpus files you can just run the above command on the folder and viola.
If you modify the source code, be sure to recompile the coverage binary too.




# <=To be continued=
