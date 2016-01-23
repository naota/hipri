# hipri

User-space code to test the preadv2/pwritev2 syscalls with a polling
block layer.

## Introduction

This codebase can be used to test out the proposed new system calls
[1] that will tie into the polling mode of the block layer [2] adding in
version 4.4 of the Linux kernel.

## Compiling the Code

Since almost every kernel lacks preadv2 and pwritev2 system calls at
this stage and since no libc support exists for these calls we have to
be a bit hacky in how we get this code tested ;-). We use #define to
set the system call numbers so make sure they match what is in
arch/x86/entry/syscalls/syscall_*.tbl (for x86 based architectures).

To compile the code using the older, supported preadv and pwritev
(which will NOT invoke polling) use
```
make clean && make
```
To build the code with the new system calls use
```
make clean && CFLAGS=-DCONFIG_PREADV2 make
```
Note that the code should build on systems that lack the p*v2 system
calls.

## Running the Code

When you run the executable on non-supporting systems the program will
most proably fail with this message:
```
name@machine:~/hipri$ ./hipri hipri.dat
error: syscall(): Function not implemented (38)
```
When you run the code on a supported system this is the expected
output:
```
name@machine:~/hipri$ ./hipri hipri.dat
check: all 2048 entries match!
info: this is thread 1
[  164.929194] direct_io (batesste): here!
[  164.943186] direct_io (batesste): there!
info: this is thread 0
[  164.946694] direct_io (batesste): here!
[  164.948267] direct_io (batesste): there!
report: exchanged 33554432 Bytes in 0.041 seconds.
report: exchanged 33554432 Bytes in 0.070 seconds.
invoked=201364, success=0

```
Note the code right now is really early. Needs quite a bit of
functionality so we can start doing a real analysis. Also, long term
the plan will be to add support in FIO and do more analysis using
that.

## References

[1] http://thread.gmane.org/gmane.linux.kernel.api/17278

[2] https://lwn.net/Articles/663879/
