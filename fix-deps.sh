#!/bin/bash

darwin_version=`uname -r`
before_prefix="$PWD/extra/release/x86_64-apple-darwin$darwin_version/lib/"
echo $before_prefix
for i in $PWD/extra/lib/*dylib* 
do
  printf "Fixing deps for... $i\n"
  libs=`otool -L $i | grep $before_prefix | cut -d ' ' -f 1`
  for lib in $libs 
  do
    newlib=`echo $lib | sed -e "s|$before_prefix|$PWD/extra/lib/|g"`
    install_name_tool -change $lib $newlib $i 
  done 
  has_rtmp=`otool -L $i | grep rtmp`
  if [ -n "$has_rtmp" ]; then
    install_name_tool -change librtmp.so.1 $PWD/extra/lib/librtmp.so.1 $i 
  fi
done
install_name_tool -change librtmp.so.1 $PWD/extra/lib/librtmp.so.1 $PWD/extra/lib/librtmp.so.1
