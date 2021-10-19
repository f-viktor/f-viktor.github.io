---
layout: default
---

https://github.com/mykter/afl-training/blob/master/harness/README.md
https://volatileminds.net/2015/08/20/advanced-afl-usage-preeny.html  
https://0x00sec.org/t/fuzzing-projects-with-american-fuzzy-lop-afl/6498

# Serial experiments AFL
Idk if it's just me, but I haven't been able to find a simple writeup on how to use AFL QEMU mode to fuzz binaries without source code.

# Setting up AFL

To build AFL with QEMU support, clone [this repo](https://github.com/AFLplusplus/AFLplusplus/tree/stable/qemu_mode
) and run `make binary-only`

For me it just worked, which i was pretty amazed about.

To make AFL work you'll need to set a few other thigns:
```
sudo su
echo core >/proc/sys/kernel/core_pattern
cd /sys/devices/system/cpu
echo performance | tee cpu*/cpufreq/scaling_governor
```

When you finish you can set it back to default with:
```
sudo su
cd /sys/devices/system/cpu
echo ondemand | tee cpu*/cpufreq/scaling_governor
echo "|/usr/lib/systemd/systemd-coredump %P %u %g %s %t %c %h" >/proc/sys/kernel/core_pattern
```

# Harness setup

I'll be re-using the harness set up on the end of [this post](../articles/libvpxfuzz)

AFL generally assumes that input comes from `stdin`.
To make it easier on us, I'll create a simple harness that will read input from `stdin` and use that in our function.
I realize that most software is not that nice, but let's just go with it for now.
I made this modification to the harness to replace `LLVMFuzzerTestOneInput`

```c
#define SIZE 4000
extern "C" int main( int argc, const char* argv[] ) {
  uint8_t *data;
  size_t size;

  unsigned char input[SIZE] = {0};

  size = read(STDIN_FILENO, input, SIZE);
  data = input;
```

I built this without any fuzzing instrumentation

Let's start AFL QEMU mode with the following command

```
afl-fuzz -Q -i input -o output vpx_enc_afl_vp8
```

It sure is doing... something...

It was able to measure paths, but I can't really tell if it's reaching the code paths I'd like it to reach. It would be useful to get some sort of a coverage map before going forward.

# Getting coverage
So most blogpost I've seen describe some version of what can be read [here](https://github.com/gaasedelen/lighthouse/blob/710b13f38eaa548109c26ca3cefa68e598682219/coverage/README.md). There is no mention here about doing this in linux, or visualizing the coverage on something that doesn't cost thousands of dollars.

Enter [dragondance](https://github.com/0ffffffffh/dragondance).  
This tool visualizes coverage in Ghidra and has [pintool libraries](https://github.com/0ffffffffh/dragondance/tree/master/coveragetools) for all operating systems.

After experimenting a bit, it turns out that the claimed DynamoRio support doesnt work, however the pintool support does.

Just run it on your queued files to get coverage output for each.
```
./pin -t ddph64.so -- ./vpx_enc_afl_vp8 <<< ./output/queue/id:000006,src:000005,time:449941,op:havoc,rep:128
```
You'll get `.out` files that you can load into the plugin in ghidra.
