<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [Build](#build)
- [Run](#run)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# Build

 Use GNU Make (tested under gcc >= 4.9) (all other platforms, possible on Windows using msys2)

## Make
If you want to use the Make build system, the dependencies for Signals need to be downloaded and built first.

### Dependencies

To do that, simply run the following script :
```
$ PREFIX=/path/to/your/destination/prefix ./extra.sh
```

This script will first ask you to install some tools (make, libtools, nasm, rsync ...).
On completion, it will create a 'sysroot' directory at the location given by "PREFIX".
Choose the destination path wisely, as the generated 'sysroot' isn't guaranteed
to be relocatable.

```
$ PREFIX=/home/john/source/signals-sysroot ./extra.sh
```

Once 'extra.sh' is done, you need to set the environment variable 'EXTRA'
so it points to your newly created sysroot directory.

```
$ export EXTRA=/home/john/source/signals-sysroot
```

You can then run make:

```
$ make
```

Note:

You can use the ```CPREFIX``` environment variable to indicate another build toolchain e.g.:

```
PREFIX=/home/john/source/signals-sysroot-w32 CPREFIX=x86_64-w64-mingw32 ./extra.sh
```

#### MinGW-w64

On Windows, to be able to use GNU Make, we recommend using [msys2](https://msys2.github.io/)
and using its native package manager ('pacman') to install those tools.

However, some environment variables including PATH need to be tweaked (especially if it contains spaces) as follows:
64 bits:
```
$ export PATH=/mingw64/bin:$PATH
$ export MSYSTEM=MINGW32
$ export PKG_CONFIG_PATH=/mingw64/lib/pkgconfig
```

32 bits:
```
$ export PATH=/mingw32/bin:$PATH
$ export MSYSTEM=MINGW32
$ export PKG_CONFIG_PATH=/mingw32/lib/pkgconfig
```

Once the dependencies are built, on Windows with msys2, in bin/make/config.mk, remove ```-XCClinker``` (introduced by SDL2).

Note: when using a MinGW-w64 toolchains, you may have a failure when trying to build signals for the first time.
Please modify the bin/make/config.mk file accordingly:
- add ```-D_WIN32_WINNT=0x0501 -DWIN32_LEAN_AND_MEAN```
- Linux only: remove ```-mwindows``` and make you you selected posix threads with ```sudo update-alternatives --config x86_64-w64-mingw32-g++```

#### MacOS

You must set the ```NASM``` environment variable. Check the latest on http://www.nasm.us/pub/nasm/releasebuilds.

```
NASM=/usr/local/bin/nasm ./extra.sh
```

### build signals

Finally, you can run:
```
$ ./check.sh
```

or

```
$ PKG_CONFIG_PATH=$PWD/extra/x86_64-w64-mingw32/lib/pkgconfig CXX=x86_64-w64-mingw32-g++ ./check.sh
```

### Out of tree builds

You can tell the build system to generate its binaries elsewhere,
by setting the 'BIN' environment variable (which defaults to 'bin'):

```
$ make BIN=subdir
```

# Run
The binaries, by default, are in generated in ```bin/```including a sample player, the dashcastx application, and all the unit test apps.

#### Linux

If you encounter this error message: ```[SDLVideo render] Couldn't initialize: No available video device```, please install the ```libxext-dev``` package on your system.

