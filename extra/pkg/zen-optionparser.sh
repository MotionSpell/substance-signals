
function optionparser_build {
  host=$1

  lazy_download "optionparser.tar.gz" "http://sourceforge.net/projects/optionparser/files/optionparser-1.3.tar.gz/download"
  lazy_extract "optionparser.tar.gz"

  if [ ! -f $PREFIX/$host/include/optionparser/optionparser.h ] ; then
    mkdir -p $PREFIX/$host/include/optionparser
    cp $WORK/src/optionparser/src/optionparser.h $PREFIX/$host/include/optionparser/
  fi
}

function optionparser_get_deps {
  local a=0
}


