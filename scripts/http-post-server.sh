#!/usr/bin/env bash
# This is a fake HTTP server that will swallow all POST data.
# Use it to test the HTTP output.
set -euo pipefail

readonly PORT=9000

rm -f /tmp/fifo
mkfifo /tmp/fifo
trap "rm -f /tmp/fifo" EXIT

function main
{
  while true
  do
    echo "================================"
    echo "Listening ..."
    run_session
  done
}

function reply
{
  echo -e "HTTP/1.1 100 Continue\r"
  echo ""
  echo "ThisIsADummyReply"
}

function run_session
{
  cat /tmp/fifo | nc -l $PORT > >(
    while read -r line
    do
      req=$(echo "$line" | tr -d '\r\n')
      echo "Req: '$req'"

      while read -r line; do
        header=$(echo "$line" | tr -d '\r\n')
        if [ -z "$header" ] ; then
          break;
        fi
        echo "Recv: '$header'"
      done

      echo -n "Send: '">&2
      reply >&2
      echo "'" >&2
      reply >/tmp/fifo

      case $req in
        POST*)
          echo "Waiting for POST data ..." >&2
          while read -r line; do
            line=$(echo "$line" | tr -d '\r\n')
            if [ -z "$line" ] ; then
              break;
            fi
            echo "Received POST data: '$line'" >&2
          done
          ;;
        *)
          echo "Unsupported request: '$req'" >&2
          ;;
      esac

    done
  )
}

main
