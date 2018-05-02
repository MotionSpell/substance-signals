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

	export MAKE="make -j$CORES"
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
pushd extra >/dev/null
./zen-extra.sh $CPREFIX
popd >/dev/null

## move files
rsync -ar extra/release/$HOST/* extra/

if [ "$HOST" == "x86_64-linux-gnu" ]; then
	#-------------------------------------------------------------------------------
	echo OpenSSL
	#-------------------------------------------------------------------------------
	if [ ! -f extra/src/openssl-1.1.0g/include/openssl/aes.h ] ; then
		mkdir -p extra/src
		rm -rf extra/src/openssl-1.1.0g
		wget https://www.openssl.org/source/openssl-1.1.0g.tar.gz -O openssl.tar.gz
		tar xvf openssl.tar.gz -C extra/src
		rm openssl.tar.gz
		pushd extra/src/openssl-1.1.0g
		popd
	fi

	if [ ! -f extra/release/openssl/releaseOk ] ; then
		mkdir -p extra/release/openssl
		pushd extra/release/openssl
		../../src/openssl-1.1.0g/config \
			--prefix=$EXTRA_DIR
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
		rm -rf extra/release/aws
		mkdir -p extra/release/aws
		pushd extra/release/aws
		cmake \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_SOURCE_DIR=../../src/aws \
			-DCMAKE_CXX_FLAGS=-I$EXTRA_DIR/include \
			-DCMAKE_LD_FLAGS=-L$EXTRA_DIR/lib \
			-DCMAKE_INSTALL_PREFIX=$EXTRA_DIR \
			-DENABLE_TESTING=OFF \
			-DBUILD_ONLY="s3;mediastore;mediastore-data" \
			../../src/aws
		$MAKE
		$MAKE install
		popd
		touch extra/release/aws/releaseOk
	fi

	#-------------------------------------------------------------------------------
	echo sqlite3
	#-------------------------------------------------------------------------------
	if [ ! -f extra/src/libsqlite3/sqlite3.h ] ; then
		mkdir -p extra/src
		rm -rf extra/src/libsqlite3
		git clone https://github.com/LuaDist/libsqlite3.git extra/src/libsqlite3
		pushd extra/src/libsqlite3
		autoreconf -fiv
		popd
	fi

	if [ ! -f extra/release/libsqlite3/releaseOk ] ; then
		rm -rf extra/release/libsqlite3
		mkdir -p extra/release/libsqlite3
		pushd extra/release/libsqlite3
		../../src/libsqlite3/configure \
			--prefix=$EXTRA_DIR \
			--host=$HOST
		$MAKE
		$MAKE install
		popd
		touch extra/release/libsqlite3/releaseOk
	fi

fi #"$HOST" == "x86_64-linux-gnu"

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
