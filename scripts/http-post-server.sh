#!/usr/bin/env bash
set -eou pipefail

readonly scriptDir=$(dirname $0)

tcpserver -D 127.0.0.1 9000 $scriptDir/http-post-server-cgi.sh
