
function libjpeg-turbo_build {
  lazy_download "libjpeg-turbo.tar.gz" "https://sourceforge.net/projects/libjpeg-turbo/files/1.5.3/libjpeg-turbo-1.5.3.tar.gz/download"
  lazy_extract "libjpeg-turbo.tar.gz"
  autoconf_build $host "libjpeg-turbo"
}

function libjpeg-turbo_get_deps {
  local a=0
}

