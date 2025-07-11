#!/usr/bin/env bash

set -euo pipefail

readonly workDir=${workDir-/tmp/mem/zen-work}
readonly cacheDir=${cacheDir-/tmp/mem/zen-cache}

function main {
  readonly scriptDir=$(get_abs_dir $(dirname $0))

  for f in $scriptDir/pkg/zen-*.sh ; do
    source $f
  done

  checkForCommonBuildTools

  for pkg in $(get_root_packages $1) ; do
    buildPackage $workDir "$pkg" $1
  done
}

# these are the packages we directly depend upon
function get_root_packages
{
  local host=$1

  if ([ "$host" == "x86_64-pc-linux-gnu" ] || [ "$host" = "x86_64-linux-gnu" ]) && [ $ENABLE_AWS == "1" ]; then
    echo aws
  fi

  echo libepoxy
  echo openssl
  echo sqlite3
  echo postgres
  echo asio
  echo ffmpeg
  echo gpac
  echo libcurl
  echo libjpeg-turbo
  echo libsdl2
  echo rapidjson
}

#####################################
# ZenBuild utility functions
#####################################

function prefixLog {
  pfx=$1
  shift
  "$@" 2>&1 | sed -u "s/^.*$/$pfx&/"
}

function printMsg {
  echo -n "[32m"
  echo $*
  echo -n "[0m"
}

function isMissing {
  progName=$1
  echo -n "Checking for $progName ... "
  if which $progName 1>/dev/null 2>/dev/null; then
    echo "ok"
    return 1
  else
    return 0
  fi
}

function installErrorHandler {
  trap "printMsg 'Spawning a rescue shell in current build directory'; PS1='\\w: rescue$ ' bash --norc" EXIT
}

function uninstallErrorHandler {
  trap - EXIT
}

function lazy_download {
  local file="$1"
  local url="$2"

  local hashKey=$(echo "$url" | md5sum - | sed 's/ .*//')

  mkdir -p $cacheDir
  if [ ! -e "$cacheDir/$hashKey" ]; then
    echo "Downloading: $file"
    wget "$url" -c -O "$cacheDir/${hashKey}.tmp" --no-verbose
    mv "$cacheDir/${hashKey}.tmp" "$cacheDir/$hashKey" # ensure atomicity
  fi

  cp "$cacheDir/${hashKey}" "$file"
}

function lazy_extract {
  local archive="$1"
  echo -n "Extracting $archive ... "
  local name=$(basename $archive .tar.gz)
  name=$(basename $name .tar.bz2)
  name=$(basename $name .tar.xz)

  local tar_cmd="tar"
  if [ $(uname -s) == "Darwin" ]; then
    tar_cmd="gtar"
  fi

  if [ -d $name ]; then
    echo "already extracted"
  else
    rm -rf ${name}.tmp
    mkdir ${name}.tmp
    $tar_cmd -C ${name}.tmp -xlf "$archive"  --strip-components=1
    mv ${name}.tmp $name
    echo "ok"
  fi
}

function lazy_git_clone {
  local url="$1"
  local to="$2"
  local rev="$3"
  local depth=1

  if [ -d "$to/.git" ] ;
  then
    pushDir "$to"
    git reset -q --hard
    git clean -q -f
    popDir
  else
    echo "git clone: $url"
    git clone --depth=$depth "$url" "$to"
  fi

  pushDir "$to"
  while ! git checkout -q $rev ; do
    depth=$(($depth * 10))
    # "git clone --depth" defaults to only downloading the default branch,
    # but not tags.
    git config remote.origin.fetch '+refs/heads/*:refs/remotes/origin/*'
    git fetch origin --depth=$depth
  done
	git submodule update --init
  popDir
}

function mkgit {
  dir="$1"

  pushDir "$dir"
  if [ -d ".git" ]; then
    printMsg "Restoring $dir from git restore point"
    git reset -q --hard
    git clean -q -f
  else
    printMsg "Creating git for $dir"
    git init
    git config user.email "nobody@localhost"
    git config user.name "Nobody"
    git config core.autocrlf false
    git add -f *
    git commit --quiet -m "Zenbuild restore point"
  fi
  popDir
}

function applyPatch {
  local patchFile=$1
  printMsg "Patching $patchFile"
  if [ $(uname -s) == "Darwin" ]; then
    patch  --no-backup-if-mismatch -p1 -i $patchFile
  else
    patch  --no-backup-if-mismatch --merge -p1 -i $patchFile
  fi
}

function buildPackage {
  local packageName=$2
  local hostPlatform=$3

  if [ -z "$1" ] || [ -z "$packageName" ] || [ -z "$hostPlatform" ] ; then
    echo "Usage: $0 <workDir> <packageName> <hostPlatform|->"
    echo "Example: $0 /tmp/work libav x86_64-w64-mingw32"
    echo "Example: $0 /tmp/work gpac -"
    exit 1
  fi

  mkdir -p "$1"
  WORK=$(get_abs_dir "$1")

  if echo $PATH | grep " " ; then
    echo "Your PATH contain spaces, this may cause build issues."
    echo "Please clean-up your PATH and retry."
    echo "Example:"
    echo "$ PATH=/mingw64/bin:/bin:/usr/bin ./zenbuild.sh <options>"
    exit 3
  fi

  if [ -z "${PREFIX-}" ] ; then
    echo "PREFIX needs to be defined" >&2
    exit 4
  fi

  if [ ! -e "$scriptDir/config.guess" ]; then
    wget -O "$scriptDir/config.guess" 'http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=HEAD'
    chmod +x "$scriptDir/config.guess"
  fi
  BUILD=$($scriptDir/config.guess | sed 's/-unknown//' | sed 's/-msys$/-mingw32/')
  mkdir -p patches

  if [ $hostPlatform = "-" ]; then
    hostPlatform=$BUILD
  fi

  if [ -z ${alreadyDone:-} ] ; then
    printMsg "Building in: $WORK"

    printMsg "Build platform: $BUILD"
    printMsg "Target platform: $hostPlatform"

    initSymlinks
    checkForCrossChain "$BUILD" "$hostPlatform"
    alreadyDone="yes"
  fi

  mkdir -p $WORK/src

  export PREFIX
  for dir in "lib" "bin" "include"
  do
    mkdir -p "$PREFIX/${dir}"
  done

  initCflags

  build ${hostPlatform} ${packageName}
}

function initSymlinks {
  local symlink_dir=$WORK/symlinks
  mkdir -p $symlink_dir
  local tools="cc gcc g++ ar as nm strings strip"
  case $hostPlatform in
   # *darwin*)
   #   echo "Detected new Darwin host ($host): disabling ranlib"
   #   ;;
    *)
      tools="$tools ranlib"
      ;;
  esac
  case $hostPlatform in
    *mingw*)
      tools+=" dlltool windres"
      ;;
  esac
  for tool in $tools
  do
    if which $hostPlatform-$tool > /dev/null ; then
      continue
    fi
    local dest=$symlink_dir/$hostPlatform-$tool
    if [ ! -f $dest ]; then
      ln -s $(which $tool) $dest
    fi
  done
  export PATH=$PATH:$symlink_dir
}

function initCflags {
  # avoid interferences from environment
  unset CC
  unset CXX
  unset CFLAGS
  unset CXXFLAGS
  unset LDFLAGS

  CFLAGS="-O2"
  CXXFLAGS="-O2"
  LDFLAGS="-s"

  CFLAGS+=" -w"
  CXXFLAGS+=" -w"

  # Don't statically link libgcc: this is incompatible with the pthreads
  # library, which dynamically loads libgcc_s.
  # See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56101#c2
  # LDFLAGS+=" -static-libgcc"

  if [ $(uname -s) != "Darwin" ]; then
    LDFLAGS+=" -static-libstdc++"
  fi

  export CFLAGS
  export CXXFLAGS
  export LDFLAGS

  if [ $(uname -s) == "Darwin" ]; then
    local cores=$(sysctl -n hw.logicalcpu)
  else
    local cores=$(nproc)
  fi

  if [ -z "$MAKE" ]; then
    MAKE="make -j$cores"
  fi

  export MAKE
}

function importPkgScript {
  local name=$1
  if ! test -f zen-${name}.sh; then
    echo "Package $name does not have a zenbuild script"
    exit 1
  fi

  source zen-${name}.sh
}

function lazy_build {
  local host=$1
  local name=$2

  export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig
  export PKG_CONFIG_LIBDIR=$PREFIX/lib/pkgconfig

  if is_built $host $name ; then
    printMsg "$name: already built"
    return
  fi


  printMsg "$name: building ..."

  local deps=$(${name}_get_deps)
  for depName in $deps ; do
    build $host $depName
  done

  pushDir $WORK/src

  # launch the builder in a separate process
  # so it cannot modify our environment.
  (
  installErrorHandler
  ${name}_build $host
  uninstallErrorHandler
  )

  popDir

  printMsg "$name: build OK"
  mark_as_built $host $name
}

function mark_as_built {
  local host=$1
  local name=$2

  local flagfile="$WORK/flags/$host/${name}.built"
  mkdir -p $(dirname $flagfile)
  touch $flagfile
}

function is_built {
  local host=$1
  local name=$2

  local flagfile="$WORK/flags/$host/${name}.built"
  if [ -f "$flagfile" ] ;
  then
    return 0
  else
    return 1
  fi
}

function autoconf_build {
  local host=$1
  shift
  local name=$1
  shift

  if [ ! -f $name/configure ] ; then
    printMsg "WARNING: package '$name' has no configure script, running autoreconf"
    pushDir $name
    aclocal && libtoolize --force && autoreconf
    popDir
  fi

  rm -rf $name/build/$host
  mkdir -p $name/build/$host
  pushDir $name/build/$host
  ../../configure \
    --build=$BUILD \
    --host=$host \
    --prefix=$PREFIX \
    "$@"
  $MAKE
  $MAKE install
  popDir
}

function build {
  local host=$1
  local name=$2
    lazy_build $host $name
}

function pushDir {
  local dir="$1"
  pushd "$dir" 1>/dev/null 2>/dev/null
}

function popDir {
  popd 1>/dev/null 2>/dev/null
}

function get_cross_prefix {
  local build=$1
  local host=$2

  if [ "$host" = "-" ] ; then
    echo ""
  else
    echo "$host-"
  fi
}

function checkForCrossChain {
  local build=$1
  local host=$2
  local error="0"

  local cross_prefix=$(get_cross_prefix $build $host)

  # ------------- GCC -------------
  if isMissing "${cross_prefix}g++" ; then
    echo "No ${cross_prefix}g++ was found in the PATH."
    error="1"
  fi

  if isMissing "${cross_prefix}gcc" ; then
    echo "No ${cross_prefix}gcc was found in the PATH."
    error="1"
  fi

  # ------------- Binutils -------------
  if isMissing "${cross_prefix}nm" ; then
    echo "No ${cross_prefix}nm was found in the PATH."
    error="1"
  fi

  if isMissing "${cross_prefix}ar" ; then
    echo "No ${cross_prefix}ar was found in the PATH."
    error="1"
  fi

  if [ $(uname -s) != "Darwin" ]; then
    if isMissing "${cross_prefix}ranlib" ; then
      echo "No ${cross_prefix}ranlib was found in the PATH."
       error="1"
    fi
  fi

  if isMissing "${cross_prefix}strip" ; then
    echo "No ${cross_prefix}strip was found in the PATH."
    error="1"
  fi

  if isMissing "${cross_prefix}strings" ; then
    echo "No ${cross_prefix}strings was found in the PATH."
    error="1"
  fi

  if isMissing "${cross_prefix}as" ; then
    echo "No ${cross_prefix}as was found in the PATH."
    error="1"
  fi

  local os=$(get_os "$host")
  if [ $os == "mingw32" ] ; then
    if isMissing "${cross_prefix}dlltool" ; then
      echo "No ${cross_prefix}dlltool was found in the PATH."
      error="1"
    fi

    if isMissing "${cross_prefix}windres" ; then
      echo "No ${cross_prefix}windres was found in the PATH."
      error="1"
    fi
  fi

  if [ $error == "1" ] ; then
    exit 1
  fi
}

function checkForCommonBuildTools {
  local error="0"

  if isMissing "pkg-config"; then
    echo "pkg-config not installed.  Please install with:"
    echo "pacman -S pkgconfig"
    echo "or"
    echo "apt-get install pkg-config"
    echo ""
    error="1"
  fi

  # Check CUDA's availability
  if [ $ENABLE_NVIDIA == "1" ] && isMissing "nvcc"; then
    echo "nvcc not found, Please install nVidia Cuda Toolkit, and make sure nvcc is available in the PATH"
    echo "Please check nvidia's website"
    echo ""
    error="1"
  fi

  if isMissing "patch"; then
    echo "patch not installed.  Please install with:"
    echo "pacman -S patch"
    echo "or"
    echo "apt-get install patch"
    echo ""
    error="1"
  fi

  if isMissing "python2"; then
    echo "python2 not installed.  Please install with:"
    echo "pacman -S python2"
    echo "or"
    echo "apt-get install python2"
    echo "or"
    echo "port install python27 && ln -s /opt/local/bin/python2.7 /opt/local/bin/python2"
    echo ""
    error="1"
  fi

  if isMissing "autoreconf"; then
    echo "autoreconf not installed. Please install with:"
    echo "pacman -S autoconf"
    echo "or"
    echo "apt-get install autoconf"
    echo "or"
    echo "port install autoconf"
    echo ""
    error="1"
    exit 1
  fi

  if isMissing "aclocal"; then
    echo "aclocal not installed. Please install with:"
    echo "pacman -S automake"
    echo "or"
    echo "apt-get install automake"
    echo "or"
    echo "port install automake"
    echo ""
    error="1"
    exit 1
  fi

  if isMissing "libtool"; then
    echo "libtool not installed.  Please install with:"
    echo "pacman -S msys/libtool"
    echo "or"
    echo "apt-get install libtool libtool-bin"
    echo ""
    error="1"
  fi

  # We still need to check that on Mac OS
  if [ $(uname -s) == "Darwin" ]; then
    if isMissing "glibtool"; then
      echo "libtool is not installed. Please install with:"
      echo "brew install libtool"
      echo "or"
      echo "port install libtool"
      echo ""
      error="1"
    fi
  fi

  if isMissing "make"; then
    echo "make not installed.  Please install with:"
    echo "pacman -S make"
    echo "or"
    echo "apt-get install make"
    echo ""
    error="1"
  fi

  if isMissing "cmake"; then
    echo "make not installed.  Please install with:"
    echo "pacman -S mingw-cmake"
    echo "or"
    echo "apt-get install cmake"
    echo ""
    error="1"
  fi

  if isMissing "autopoint"; then
    echo "autopoint not installed.  Please install with:"
    echo "pacman -S gettext gettext-devel"
    echo "or"
    echo "apt-get install autopoint"
    echo ""
    error="1"
  fi

  if isMissing "msgfmt"; then
    echo "msgfmt not installed.  Please install with:"
    echo "pacman -S gettext gettext-devel"
    echo "or"
    echo "apt-get install gettext"
    echo ""
    error="1"
  fi

  if isMissing "yasm"; then
    echo "yasm not installed.  Please install with:"
    echo "apt-get install yasm"
    echo ""
    error="1"
  fi

  if isMissing "nasm"; then
    echo "nasm not installed.  Please install with:"
    echo "apt-get install nasm"
    echo ""
    error="1"
  fi

  if isMissing "wget"; then
    echo "wget not installed.  Please install with:"
    echo "pacman -S msys/wget"
    echo "or"
    echo "apt-get install wget"
    echo "or"
    echo "sudo port install wget"
    echo ""
    error="1"
  fi


  if [ $(uname -s) == "Darwin" ]; then
    if isMissing "gsed"; then
      echo "gsed not installed. Please install with:"
      echo "brew install gnu-sed"
      echo "or"
      echo "port install gsed"
      echo ""
      error="1"
    else
      sed=gsed
    fi
  else
    if isMissing "sed"; then
      echo "sed not installed.  Please install with:"
      echo "pacman -S msys/sed"
      echo "or"
      echo "apt-get install sed"
      echo ""
      error="1"
    else
      sed=sed
    fi
  fi

  if [ $(uname -s) == "Darwin" ]; then
    if isMissing "gtar"; then
      echo "gnu-tar not installed.  Please install with:"
      echo "brew install gnu-tar"
      echo "or"
      echo "port install gnutar && sudo ln -s /opt/local/bin/gnutar /opt/local/bin/gtar"
      echo ""
      error="1"
    fi
  else
    if isMissing "tar"; then
      echo "tar not installed.  Please install with:"
      echo "mingw-get install tar"
      echo "or"
      echo "apt-get install tar"
      echo ""
      error="1"
    fi
  fi

  if isMissing "xz"; then
    echo "xz is not installed. Please install with:"
    echo "apt-get install xz"
    echo "or"
    echo "brew install xz"
    echo ""
    error="1"
  fi

  if isMissing "git" ; then
    echo "git not installed.  Please install with:"
    echo "pacman -S mingw-git"
    echo "or"
    echo "apt-get install git"
    echo ""
    error="1"
  fi

  if isMissing "hg" ; then
    echo "hg not installed.  Please install with:"
    echo "pacman -S msys/mercurial"
    echo "or"
    echo "apt-get install mercurial"
    echo ""
    error="1"
  fi

  if isMissing "meson" ; then
    echo "meson not installed.  Please install with:"
    echo "pacman -S msys/meson"
    echo "or"
    echo "apt-get install meson"
    echo ""
    error="1"
  fi

  if isMissing "svn" ; then
    echo "svn not installed.  Please install with:"
    echo "pacman -S msys/subversion"
    echo "or"
    echo "apt-get install subversion"
    echo ""
    error="1"
  fi

  if isMissing "gperf" ; then
    echo "gperf not installed.  Please install with:"
    echo "pacman -S msys/gperf"
    echo "or"
    echo "apt-get install gperf"
    echo ""
    error="1"
  fi

  if isMissing "perl" ; then
    echo "perl not installed.  Please install with:"
    echo "pacman -S perl"
    echo "or"
    echo "apt-get install perl"
    echo ""
    error="1"
  fi

  if [ $error == "1" ] ; then
    exit 1
  fi
}

function get_arch {
  host=$1
  echo $host | sed "s/-.*//"
}

function get_os {
  host=$1
  echo $host | sed "s/.*-//"
}

function get_abs_dir {
  local relDir="$1"
  pushDir $relDir
  pwd
  popDir
}

main "$@"
