
function ffnvenc_build {

	echo "Extracting $NVIDIA_CODEC_SDK"
    unzip $NVIDIA_CODEC_SDK 
    pushDir $(basename $NVIDIA_CODEC_SDK .zip)
    popDir
    rm -rf $(basename $NVIDIA_CODEC_SDK .zip)


    git clone https://github.com/FFmpeg/nv-codec-headers.git
    pushDir nv-codec-headers
    sed -i "s|/usr/local|$PREFIX|g" Makefile
    make
    make install
    popDir
}

function ffnvenc_get_deps {
  local a=0
}