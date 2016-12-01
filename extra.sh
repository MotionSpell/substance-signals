#!/bin/bash
#
# Download, build and locally deploy external dependencies
#

set -e
EXTRA_DIR=$PWD/extra
export CFLAGS=-w
export PKG_CONFIG_PATH=$EXTRA_DIR/lib/pkgconfig

if [ -z "$MAKE" ]; then
	if [ $(uname -s) == "Darwin" ]; then
		CORES=$(sysctl -n hw.logicalcpu)
	else
		CORES=$(nproc)
	fi

	MAKE="make -j$CORES"
fi

if [ -z "$CPREFIX" ]; then
	case $OSTYPE in
	msys)
		CPREFIX=x86_64-w64-mingw32
		echo "MSYS detected ($OSTYPE): forcing use of prefix \"$CPREFIX\""
		;;
	darwin*)
		CPREFIX=-
		HOST=$(gcc -dumpmachine)
		echo "Darwin detected ($OSTYPE): forcing use of prefix \"$CPREFIX\" (host=$HOST)"
		;;
	*)
		CPREFIX=$(gcc -dumpmachine)
		echo "Platform $OSTYPE detected."
		;;
	esac
fi
if [ -z "$HOST" ]; then
	HOST=$($CPREFIX-gcc -dumpmachine)
fi
echo "Using compiler host ($HOST) with prefix ($CPREFIX)"

case $OSTYPE in
darwin*)
    if [ -z "$NASM" ]; then
        echo "You must set the NASM env variable."
    fi
    ;;
esac


#-------------------------------------------------------------------------------
echo zenbuild extra script
#-------------------------------------------------------------------------------
if [ ! -f extra/zenbuild.built ] ; then
	pushd extra
	./zen-extra.sh $CPREFIX
	popd

	## move files
	rsync -ar extra/release/$HOST/* extra/
	touch extra/zenbuild.built
fi

#-------------------------------------------------------------------------------
echo libjpeg-turbo
#-------------------------------------------------------------------------------
if [ ! -f extra/src/libjpeg-turbo/configure.ac ] ; then
	mkdir -p extra/src
	rm -rf extra/src/libjpeg-turbo
	pushd extra/src
	git clone https://github.com/PyYoshi/libjpeg-turbo.git
	pushd libjpeg-turbo
	git checkout 1.3.1
	autoreconf -fiv
	popd
	popd
fi

if [ ! -f extra/release/libjpeg-turbo/releaseOk ] ; then
	mkdir -p extra/release/libjpeg-turbo
	pushd extra/src/libjpeg-turbo
	../../src/libjpeg-turbo/configure \
		--prefix=$EXTRA_DIR \
		--host=$HOST
	$MAKE
	$MAKE install
	popd
	touch extra/release/libjpeg-turbo/releaseOk
fi

#-------------------------------------------------------------------------------
echo optionparser
#-------------------------------------------------------------------------------
if [ ! -f extra/src/optionparser-1.3/src/optionparser.h ] ; then
	mkdir -p extra/src
	rm -rf extra/src/optionparser-1.3
	wget http://sourceforge.net/projects/optionparser/files/optionparser-1.3.tar.gz/download -O optionparser-1.3.tar.gz
	tar xvf optionparser-1.3.tar.gz -C extra/src
	rm optionparser-1.3.tar.gz
fi

if [ ! -f extra/include/optionparser/optionparser.h ] ; then
	mkdir -p extra/include/optionparser
	cp extra/src/optionparser-1.3/src/optionparser.h extra/include/optionparser/
fi

#-------------------------------------------------------------------------------
echo rapidjson
#-------------------------------------------------------------------------------
if [ ! -f extra/src/rapidjson/include/rapidjson/rapidjson.h ] ; then
	mkdir -p extra/src
	rm -rf extra/src/rapidjson
	git clone https://github.com/miloyip/rapidjson.git extra/src/rapidjson
	pushd extra/src/rapidjson
	git checkout 67d8a99477b4e1b638b920cc9b02f8910dfb05e7
	git submodule update --init
	popd
fi

if [ ! -f extra/release/rapidjson/releaseOk ] ; then
	mkdir -p extra/release/rapidjson
	pushd extra/release/rapidjson
	#CC=$CPREFIX-gcc cmake -G "Unix Makefiles" -DCMAKE_CXX_COMPILER=$CPREFIX-gcc -DCMAKE_INSTALL_PREFIX:PATH=$EXTRA_DIR ../../src/rapidjson
	#CFLAGS="$CFLAGS -I$PWD/include"
	#$MAKE
	#$MAKE install
	popd
	touch extra/release/rapidjson/releaseOk
fi

if [ ! -f extra/include/rapidjson/rapidjson.h ] ; then
	cp -r extra/src/rapidjson/include/rapidjson extra/include/
fi

#-------------------------------------------------------------------------------
echo cURL
#-------------------------------------------------------------------------------
if [ ! -f extra/src/curl/include/curl/curl.h ] ; then
	mkdir -p extra/src
	rm -rf extra/src/curl
	git clone https://github.com/curl/curl.git extra/src/curl
	pushd extra/src/curl
	git checkout curl-7_50_1
	autoreconf -fiv
	popd
fi

if [ ! -f extra/release/curl/releaseOk ] ; then
	mkdir -p extra/release/curl
	pushd extra/release/curl
	../../src/curl/configure \
		--prefix=$EXTRA_DIR \
		--host=$HOST
	$MAKE
	$MAKE install
	popd
	touch extra/release/rapidjson/releaseOk
fi

echo "Done"
