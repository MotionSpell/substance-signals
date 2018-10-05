#!/usr/bin/env bash
set -eou pipefail

trap "echo '----' >&2" EXIT

hasContent=0

function main
{
  echo "-----------" >&2

  req=$(read_line)
  echo "Request: '$req'" >&2

  while true ; do
    header=$(read_line)
    echo "Header: '$header'" >&2
    if [ -z "$header" ] ; then
      break
    fi
    case $header in
      Transfer-Encoding:\ chunked*)
        hasContent=1
        ;;
    esac
  done

  case $req in
    POST*)
      handle_post
      ;;
    *)
      echo "Unknown request" >&2
      ;;
  esac
}

function handle_post
{
  if [ $hasContent = 1 ] ; then

    echo -e "100 continue\r"
    echo ""

    echo "Expecting POST data ..." >&2
    while true ; do
      data=$(read_line)
      if [ -z "$data" ] ; then
        break
      fi
      echo "$data" | hexdump -vC >&2
    done
    echo "POST data OK" >&2
  fi

  echo -e "200 OK\r"
  echo ""
}

function read_line
{
  read line
  echo $line | tr -d '\r\n'
}

main
