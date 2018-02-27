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
	darwin*|linux-musl)
		CPREFIX=-
		if [ ! -e extra/config.guess ] ; then
			wget -O extra/config.guess 'http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=HEAD'
			chmod +x extra/config.guess
		fi
		HOST=$(extra/config.guess)
		echo "$OSTYPE detected: forcing use of prefix \"$CPREFIX\" (host=$HOST)"
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
	git clone https://github.com/libjpeg-turbo/libjpeg-turbo.git
	pushd libjpeg-turbo
	git checkout 5abf2536
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
	git checkout curl-7_58_0
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
	touch extra/release/curl/releaseOk
fi

#-------------------------------------------------------------------------------
echo OpenSSL
#-------------------------------------------------------------------------------
if [ ! -f extra/src/openssl-1.1.0g/include/openssl/aes.h ] ; then
	mkdir -p extra/src
	rm -rf extra/src/openssl-1.1.0g
	wget https://www.openssl.org/source/openssl-1.1.0g.tar.gz -O openssl.tar.gz
	tar xvf openssl.tar.gz -C extra/src
	pushd extra/src/openssl-1.1.0g
	popd
fi

if [ ! -f extra/release/openssl/releaseOk ] ; then
	mkdir -p extra/release/openssl
	pushd extra/release/openssl
	../../src/openssl/configure \
		--prefix=$EXTRA_DIR \
		--host=$HOST
	$MAKE depend
	$MAKE
	$MAKE install
	popd
	touch extra/release/openssl/releaseOk
fi


#-------------------------------------------------------------------------------
echo AWS-SDK
#-------------------------------------------------------------------------------
if [ ! -f extra/src/aws/CMakeLists.txt ] ; then
	mkdir -p extra/src
	rm -rf extra/src/aws
	git clone https://github.com/aws/aws-sdk-cpp.git extra/src/aws
	pushd extra/src/aws
	git checkout 1.3.48
	popd
fi

if [ ! -f extra/release/aws/releaseOk ] ; then
	mkdir -p extra/release/aws
	pushd extra/release/aws
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_SOURCE_DIR=../../src/aws -DCMAKE_CXX_FLAGS=-I$EXTRA_DIR/include -DOPENSSL_LIBRARIES=$EXTRA_DIR/lib -DCURL_LIBRARY=$EXTRA_DIR/lib \
	 -DCURL_INCLUDE_DIR=$EXTRA_DIR/include -DCMAKE_CXX_FLAGS=-L$EXTRA_DIR/lib -DCMAKE_INSTALL_PREFIX=$EXTRA_DIR ../../src/aws -DBUILD_ONLY="s3;mediastore;mediastore-data"
	$MAKE
	$MAKE install
	popd
	touch extra/release/aws/releaseOk
fi


#-------------------------------------------------------------------------------	
echo ASIO
#-------------------------------------------------------------------------------
if [ ! -f extra/src/asio/asio/include/asio.hpp ] ; then
	mkdir -p extra/src
	rm -rf extra/src/asio
	git clone --depth 1000 https://github.com/chriskohlhoff/asio extra/src/asio
	pushd extra/src/asio
	git checkout b44805ff00
	popd	
fi	
	
if [ ! -f extra/include/asio/asio.hpp ] ; then
	mkdir -p extra/include/asio	
	cp -r extra/src/asio/asio/include/* extra/include/asio/	
fi

echo "Done"
