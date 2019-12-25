function rapidjson_build {
  host=$1

	lazy_git_clone https://github.com/miloyip/rapidjson.git rapidjson dfbe1db9da455552f7a9ad5d2aea17dd9d832ac1
	cp -r rapidjson/include/rapidjson $PREFIX/include/
}

function rapidjson_get_deps {
  local a=0
}
