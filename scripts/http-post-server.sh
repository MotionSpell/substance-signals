#!/usr/bin/env bash
# Under Debian: apt install ucspi-tcp
set -eou pipefail

readonly scriptDir=$(dirname $0)

tcpserver -D 127.0.0.1 9000 $scriptDir/http-post-server-cgi.sh
