#!/usr/bin/env bash

function libjpeg-turbo_build {
  pushDir $WORK/src

  lazy_git_clone https://github.com/libjpeg-turbo/libjpeg-turbo.git libjpeg-turbo 5abf2536

  pushDir "libjpeg-turbo"
  autoreconf -fiv
  popDir

  autoconf_build $host "libjpeg-turbo"

  popDir
}

function libjpeg-turbo_get_deps {
  local a=0
}

