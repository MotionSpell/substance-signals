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

pushd extra >/dev/null
./zen-extra.sh $CPREFIX
popd >/dev/null

## move files
rsync -ar extra/release/$HOST/* extra/

echo "Done"
