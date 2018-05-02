
function aws_build {
  lazy_git_clone "https://github.com/aws/aws-sdk-cpp.git" "aws" "1.4.40"

  mkdir -p aws/bin/$host
  pushDir aws/bin/$host
  cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS=-I$PREFIX/$host/include \
    -DCMAKE_LD_FLAGS=-L$PREFIX/$host/lib \
    -DCMAKE_INSTALL_PREFIX=$PREFIX/$host \
    -DENABLE_TESTING=OFF \
    -DBUILD_ONLY="s3;mediastore;mediastore-data" \
    ../..
  $MAKE
  $MAKE install
  popDir
}

function aws_get_deps {
  echo libcurl
}

