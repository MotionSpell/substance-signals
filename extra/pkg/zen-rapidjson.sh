function rapidjson_build {
  host=$1

	lazy_git_clone https://github.com/miloyip/rapidjson.git rapidjson 2bbd33b33217ff4a73434ebf10cdac41e2ef5e34
	cp -r rapidjson/include/rapidjson $PREFIX/include/
}

function rapidjson_get_deps {
  local a=0
}
