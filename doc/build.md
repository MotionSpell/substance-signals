<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [Build](#build)
      - [Visual Studio](#visual-studio)
      - [Make](#make)
- [Run](#run)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# Build

We support two build systems:
 - Visual Studio (tested >= 2015) (recommended on Windows)
 - Make (tested under gcc >= 4.9) (all other platforms, possible on Windows using msys2)

## Visual Studio
For Visual Studio, dependencies are already built and available from this repository: https://github.com/rbouqueau/signals-deps.git

To use them, clone the repository and copy the pre-compiled dependencies (drag and drop your signals folder on the ```CopyToSignals.bat``` batch).
The dependencies are built using Visual Studio 2015.

## Make
If you want to use the Make build system, the dependencies for Signals need to be downloaded and built first.

### Dependencies

To do that, simply run the following script :
```
$ ./extra.sh
```

This script will ask you to install some tools (make, libtools, nasm, rsync ...).
On completion, it will create a 'sysroot' directory at the top-level.
By default, the makefile will look for 'lib', 'include', etc. in this directory.
For convenience, it's possible to generate the 'sysroot' directory to
another location in your system, using the environment variable PREFIX,
and tell the makefile the new location, using
the environment variable 'EXTRA':

For example:
```
$ PREFIX=/home/john/source/signals-sysroot ./extra.sh
$ export EXTRA=/home/john/source/signals-sysroot
$ make
```

You can use the ```CPREFIX``` environment variable to indicate another build toolchain e.g.:

```
CPREFIX=x86_64-w64-mingw32 ./extra.sh
```

For CMake (needed by some dependencies), make sure you have its subpackages (modules) installed.


#### MinGW-w64

On Windows, to be able to use Make, we recommend using [msys2](https://msys2.github.io/) which comes with the package manager pacman to install those tools. However, some environment variables including PATH need to be tweaked (especially if it contains spaces) as follows:
64 bits:
```
$ export PATH=/mingw64/bin:$PWD/extra/bin:$PATH
$ export MSYSTEM=MINGW32
$ export PKG_CONFIG_PATH=/mingw64/lib/pkgconfig
```

32 bits:
```
$ export PATH=/mingw32/bin:$PWD/extra/bin:$PATH
$ export MSYSTEM=MINGW32
$ export PKG_CONFIG_PATH=/mingw32/lib/pkgconfig
```

Once the dependencies are built, on Windows with msys2, in bin/make/config.mk, remove ```-XCClinker``` (introduced by SDL2).

Note: when using a MinGW-w64 toolchains, you may have a failure when trying to build signals for the first time. Please modify the bin/make/config.mk file accordingly:
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

```
$ make BIN=subdir
```

# Run
The binaries are in generated in ```bin/```including a sample player, the dashcastx application, and all the unit test apps.

#### Linux

If you encounter this error message: ```[SDLVideo render] Couldn't initialize: No available video device```, please install the ```libxext-dev``` package on your system.

