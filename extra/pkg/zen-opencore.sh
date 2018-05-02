
function opencore-amr_get_deps {
  local a=0
}

function opencore-amr_build {
  local host=$1

  lazy_download "opencore-amr.tar.gz" "http://sourceforge.net/projects/opencore-amr/files/opencore-amr/opencore-amr-0.1.3.tar.gz"
  lazy_extract "opencore-amr.tar.gz"

  autoconf_build $host "opencore-amr" --enable-shared
}


