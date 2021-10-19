---
layout: default
---

# The 411
Here is a similar example about setting up a fuzzer for [h.264](openh264) based on a unit test.
This will be much more compressed in size as I don't plan to explain the steps again. Hopefully it will be useful as a reminder so we don't have to scroll through the first part's million pages to find examples for things.

## The build
You can build this library with a simple make command
```
make OS=linux ARCH=x86_64
```
It will give you a bunch of static libs
```
ls  *.a *.so* | sort
libcommon.a
libconsole_common.a
libdecoder.a
libencoder.a
libopenh264.a
libopenh264.so
libopenh264.so.2.1.0
libopenh264.so.6
libprocessing.a
```

To build with clang just set the env variables

```
make OS=linux ARCH=x86_64 CC=clang CXX=clang++
```

To build for fuzzing just set the `CFLAGS` `CXXFLAGS` and `LDFLAGS`

```
OS=linux ARCH=x86_64 CC=clang CXX=clang++ CFLAGS+=-fsanitize=fuzzer-no-link CXXFLAGS+=-fsanitize=fuzzer-no-link LDFLAGS+=-fsanitize=fuzzer-no-link make
```

## The harness/fuzz-target

I've found this comment in one of the files after I finished rewriting a unit test, but it's very informative about how the lib works.
```

/**
* @page DecoderUsageExample
*
* @brief
* * An example for using the decoder for Decoding only or Parsing only
*
* Step 1:decoder declaration
* @code
*
* //decoder declaration
* ISVCDecoder *pSvcDecoder;
* //input: encoded bitstream start position; should include start code prefix
* unsigned char *pBuf =...;
* //input: encoded bit stream length; should include the size of start code prefix
* int iSize =...;
* //output: [0~2] for Y,U,V buffer for Decoding only
* unsigned char *pData[3] =...;
* //in-out: for Decoding only: declare and initialize the output buffer info, this should never co-exist with Parsing only
* SBufferInfo sDstBufInfo;
* memset(&sDstBufInfo, 0, sizeof(SBufferInfo));
* //in-out: for Parsing only: declare and initialize the output bitstream buffer info for parse only, this should never co-exist with Decoding only
* SParserBsInfo sDstParseInfo;
* memset(&sDstParseInfo, 0, sizeof(SParserBsInfo));
* sDstParseInfo.pDstBuff = new unsigned char[PARSE_SIZE]; //In Parsing only, allocate enough buffer to save transcoded bitstream for a frame
*
* @endcode
*
* Step 2:decoder creation
* @code
* WelsCreateDecoder(&pSvcDecoder);
* @endcode
*
* Step 3:declare required parameter, used to differentiate Decoding only and Parsing only
* @code
* SDecodingParam sDecParam = {0};
* sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
* //for Parsing only, the assignment is mandatory
* sDecParam.bParseOnly = true;
* @endcode
*
* Step 4:initialize the parameter and decoder context, allocate memory
* @code
* pSvcDecoder->Initialize(&sDecParam);
* @endcode
*
* Step 5:do actual decoding process in slice level;
* this can be done in a loop until data ends
* @code
* //for Decoding only
* iRet = pSvcDecoder->DecodeFrameNoDelay(pBuf, iSize, pData, &sDstBufInfo);
* //or
* iRet = pSvcDecoder->DecodeFrame2(pBuf, iSize, pData, &sDstBufInfo);
* //for Parsing only
* iRet = pSvcDecoder->DecodeParser(pBuf, iSize, &sDstParseInfo);
* //decode failed
* If (iRet != 0){
* //error handling (RequestIDR or something like that)
* }
* //for Decoding only, pData can be used for render.
* if (sDstBufInfo.iBufferStatus==1){
* //output handling (pData[0], pData[1], pData[2])
* }
* //for Parsing only, sDstParseInfo can be used for, e.g., HW decoding
* if (sDstBufInfo.iNalNum > 0){
* //Hardware decoding sDstParseInfo;
* }
* //no-delay decoding can be realized by directly calling DecodeFrameNoDelay(), which is the recommended usage.
* //no-delay decoding can also be realized by directly calling DecodeFrame2() again with NULL input, as in the following. In this case, decoder would immediately reconstruct the input data. This can also be used similarly for Parsing only. Consequent decoding error and output indication should also be considered as above.
* iRet = pSvcDecoder->DecodeFrame2(NULL, 0, pData, &sDstBufInfo);
* //judge iRet, sDstBufInfo.iBufferStatus ...
* @endcode
*
* Step 6:uninitialize the decoder and memory free
* @code
* pSvcDecoder->Uninitialize();
* @endcode
*
* Step 7:destroy the decoder
* @code
* DestroyDecoder(pSvcDecoder);
* @endcode
*
*/
```

I used parts of the unit test [DecUT_DecExt.cpp](https://github.com/cisco/openh264/blob/master/test/decoder/DecUT_DecExt.cpp) to write my fuzzer, as you can see it's mostly just the re-writing part of the unit test:

```c

#include "codec_def.h"
#include "codec_app_def.h"
#include "codec_api.h"
#include "error_code.h"
#include "wels_common_basis.h"
#include "memory_align.h"
#include "ls_defines.h"

using namespace WelsDec;
#define BUF_SIZE 100
//payload size exclude 6 bytes: 0001, nal type and final '\0'
#define PAYLOAD_SIZE (BUF_SIZE - 6)


//Init members
int32_t Init();

//Decoder real bitstream
void DecoderBs (const char* sFileName);


ISVCDecoder* m_pDec;
SDecodingParam m_sDecParam;
SBufferInfo m_sBufferInfo;
SParserBsInfo m_sParserBsInfo;
uint8_t* m_pData[3];
unsigned char m_szBuffer[BUF_SIZE]; //for mocking packet
int m_iBufLength; //record the valid data in m_szBuffer
bool firstrun = true;


//Init members
int32_t Init() {
  firstrun = false;
  memset (&m_sBufferInfo, 0, sizeof (SBufferInfo));
  memset (&m_sParserBsInfo, 0, sizeof (SParserBsInfo));
  memset (&m_sDecParam, 0, sizeof (SDecodingParam));
  m_sDecParam.pFileNameRestructed = NULL;
  m_sDecParam.uiCpuLoad = rand() % 100;
  m_sDecParam.uiTargetDqLayer = rand() % 100;
  m_sDecParam.eEcActiveIdc = (ERROR_CON_IDC) (rand() & 7);
  m_sDecParam.sVideoProperty.size = sizeof (SVideoProperty);
  m_sDecParam.sVideoProperty.eVideoBsType = (VIDEO_BITSTREAM_TYPE) (rand() % 2);
  int rv = WelsCreateDecoder (&m_pDec);

  m_pData[0] = m_pData[1] = m_pData[2] = NULL;
  m_szBuffer[0] = m_szBuffer[1] = m_szBuffer[2] = 0;
  m_szBuffer[3] = 1;
  m_iBufLength = 4;
  CM_RETURN eRet = (CM_RETURN) m_pDec->Initialize (&m_sDecParam);
  return (int32_t)eRet;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

  uint8_t* pBuf = NULL;
  int32_t iBufPos = 0;
  int32_t iFileSize;
  int32_t i = 0;
  int32_t iSliceSize;
  int32_t iSliceIndex = 0;
  int32_t iEndOfStreamFlag = 0;

  uint8_t uiStartCode[4] = {0, 0, 0, 1};

  if (firstrun){
    Init();
  };
  pBuf = new uint8_t[size + 4];
  memcpy(pBuf, data, size);
  memcpy(pBuf + size, &uiStartCode[0], 4); //confirmed_safe_unsafe_usage

  iFileSize = size;


  while (true) {
    if (iBufPos >= iFileSize) {
      iEndOfStreamFlag = true;
      if (iEndOfStreamFlag)
      m_pDec->SetOption (DECODER_OPTION_END_OF_STREAM, (void*)&iEndOfStreamFlag);
      break;
    }
    for (i = 0; i < iFileSize; i++) {
      if ((pBuf[iBufPos + i] == 0 && pBuf[iBufPos + i + 1] == 0 && pBuf[iBufPos + i + 2] == 0 && pBuf[iBufPos + i + 3] == 1
        && i > 0)) {
          break;
        }
      }
      iSliceSize = i;
      m_pDec->DecodeFrame2 (pBuf + iBufPos, iSliceSize, m_pData, &m_sBufferInfo);
      m_pDec->DecodeFrame2 (NULL, 0, m_pData, &m_sBufferInfo);
      iBufPos += iSliceSize;
      ++ iSliceIndex;
    }

    if (pBuf) {
      delete[] pBuf;
      pBuf = NULL;
    }
    return 0;
}

```

My only real addition to this is these few lines


```c
if (firstrun){
  Init();
  };

pBuf = new uint8_t[size + 4];
memcpy(pBuf, data, size);
memcpy(pBuf + size, &uiStartCode[0], 4); //confirmed_safe_unsafe_usage

iFileSize = size;
```
Basically it only initializes the codec once os it doesnt run out of memory instantly, and then I've just did some memcopy operations to replace the file input with our fuzzing input and size.

# Building the fuzzer
To build the fuzzer just run this:
```
clang++ -O1 -fno-omit-frame-pointer -g -ggdb3 -I ../../codec/api/svc -I ../../codec/decoder/core/inc/ -I ../../codec/common/inc/ -fsanitize=fuzzer dec_fuzzer.cpp -o fuzzer ../../libopenh264.a
```

# Checking coverage
Here are the corresponding coverage builds for the harness and the library.
Harness:
```
clang++ -O1 -fno-omit-frame-pointer -g -ggdb3 -I ../../codec/api/svc -I ../../codec/decoder/core/inc/ -I ../../codec/common/inc/ -fprofile-instr-generate -fcoverage-mapping -fsanitize=fuzzer dec_fuzzer.cpp -o fuzzer ../../libopenh264.a -lpthread
```
Library:
```
OS=linux ARCH=x86_64 CC=clang CXX=clang++ CFLAGS+="-fprofile-instr-generate -fcoverage-mapping -O1 -fno-omit-frame-pointer -g -ggdb3 -fsanitize=fuzzer-no-link" CXXFLAGS+="-fprofile-instr-generate -fcoverage-mapping -O1 -fno-omit-frame-pointer -g -ggdb3 -fsanitize=fuzzer-no-link" LDFLAGS+=-fsanitize=fuzzer-no-link make
```
To check coverage:
```
./fuzzer ./inputs/* && \
llvm-profdata merge -sparse *.profraw -o default.profdata && \
llvm-cov show ./fuzzer -instr-profile=default.profdata -name=DecodeFrame2
```
