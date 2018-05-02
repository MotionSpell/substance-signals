function rapidjson_build {
  host=$1
  pushDir $WORK/src

	lazy_git_clone https://github.com/miloyip/rapidjson.git rapidjson 67d8a99477b4e1b638b920cc9b02f8910dfb05e7
	cp -r rapidjson/include/rapidjson $PREFIX/$host/include/

  popDir
}

function rapidjson_get_deps {
  local a=0
}
