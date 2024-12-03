vcpkg_minimum_required(VERSION 2022-10-12) # for ${VERSION}

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO  gpac/gpac 
    REF d0321826c09e3cd7b8c1ed4019626520d166b8f6
    SHA512 b3421f51665a16de2b0d2c5b57754fde61a0190b6627f88b47d71e3bd6d4c4c3839ff35c167307924b54f9df0040085124c5503b1b58eaa676e4dd139d216ba1
    HEAD_REF master
)

if (VCPKG_TARGET_IS_WINDOWS)
    message(ERROR "Cannot build for ${TARGET_TRIPLET}")
else()
    vcpkg_list(SET OPTIONS)

    if(VCPKG_TARGET_IS_OSX)

        
    #    list(APPEND OPTIONS
    #        --target-os=Darwin
    #        --extra-cflags=-I${CURRENT_INSTALLED_DIR}/include
    #		--extra-ldflags=-L${CURRENT_INSTALLED_DIR}/lib
    #    )
    elseif(VCPKG_TARGET_IS_LINUX)
    else()
        message(ERROR "Unknown target ${TARGET_TRIPLET}")
    endif()

    vcpkg_configure_make(
        SOURCE_PATH "${SOURCE_PATH}"
        DETERMINE_BUILD_TRIPLET
        NO_ADDITIONAL_PATHS
        OPTIONS
            ${OPTIONS}
            # --disable-player xxxjack this will disable the compositor, which will break the build.
            --disable-ssl
            --disable-alsa
            --disable-jack
            --disable-oss-audio
            --disable-pulseaudio
            --use-png=no
            --use-jpeg=no
            --disable-3d
            --disable-atsc
            --disable-avi
            --disable-bifs
            --disable-bifs-enc
            --disable-crypt
            --disable-dvb4linux
            --disable-dvbx
            --disable-ipv6
            --disable-laser
            --disable-loader-bt
            --disable-loader-isoff
            --disable-loader-xmt
            --disable-m2ps
            --disable-mcrypt
            --disable-od-dump
            --disable-odf
            --disable-ogg
            --disable-opt
            --disable-platinum
            --disable-qtvr
            --disable-saf
            --disable-scene-dump
            --disable-scene-encode
            --disable-scene-stats
            # --disable-scenegraph  xxxjack this will disable the compositor, which will break the build.
            --disable-seng
            --disable-sman
            --disable-streaming
            # --disable-svg xxxjack this will disable evg, which will disable the compositor, which will break the build.
            --disable-swf
            --disable-vobsub
            --disable-vrml
            --disable-wx
            --disable-x11
            --disable-x11-shm
            --disable-x11-xv
            --disable-x3d
            --enable-export
            --enable-import
            --enable-parsers
            --enable-m2ts
            --enable-m2ts-mux
            --enable-ttxt
            --enable-hevc
            --enable-isoff
            --enable-isoff-frag
            --enable-isoff-hds
            --enable-isoff-hint
            --enable-isoff-write
            --enable-isom-dump
            ${OPTIONS}
    )

    vcpkg_install_make()

    vcpkg_fixup_pkgconfig()
endif()