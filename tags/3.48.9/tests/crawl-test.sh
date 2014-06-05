#!/bin/bash
#

function warning {
  echo "** $*" >&2
  return 0
}

function die {
  warning "$*"
  exit 1
}

function debug {
  if test -n "$verbose"; then
    echo "$*" >&2
  fi
}

function info {
  printf "[$*] ..\t" >&2
}

function result {
  echo "$*" >&2
}

function cleanup {
  debug "cleaning function called"
  if test -n "$tmpdir"; then
    if test -d "$tmpdir"; then
      if test -z "$nopurge"; then
        debug "cleaning up $tmpdir"
        rm -rf "$tmpdir"
      fi
    fi
  fi
  if test -n "$crawlpid"; then
    debug "killing $crawlpid"
    kill -9 "$crawlpid"
    crawlpid=
  fi
}

function usage {
  cat << EOF
usage: $0
EOF
}

function assert_equals {
  info "$1"
  if test ! "$2" == "$3"; then
    result "expected '$2', got '$3'"
    exit 1
  else
    result "OK ($2)"
  fi
}

function start-crawl {
  # parse args
  pos=1
  while test "$#" -ge "$pos" ; do
    case "${!pos}" in
    --debug)
      verbose=1
      ;;
    --no-purge|--summary|--print-files)
      ;;
    --errors|--files|--found|--not-found|--directory)
      pos=$[${pos}+1]
      test "$#" -ge "$pos" || warning "missing argument" || return 1
      ;;
    httrack)
      pos=$[${pos}+1]
      break;
      ;;
    *)
      warning "unrecognized option ${!pos}"
      return 1
      ;;
    esac
    pos=$[${pos}+1]
  done
  debug "remaining args: ${@:${pos}}"

  # ut/ won't exceed 2 minutes
  moreargs="--quiet --max-time=120 --timeout=30 --connection-per-second=5"

  # proxy environment ?
  if test -n "$http_proxy"; then
    moreargs="$moreargs --proxy $http_proxy"
  fi

  test -n "$tmpdir" || ! warning "no tmpdir" || return 1
  tmp="${tmpdir}/crawl"
  rm -rf "$tmp"
  mkdir "$tmp" || ! warning "could not create $tmp" || return 1

  which httrack >/dev/null || ! warning "could not find httrack" || return 1
  ver=$(httrack -O /dev/null --version | sed -e 's/HTTrack version //')
  test -n "$ver" || ! warning "could not run httrack" || return 1

  # start crawl
  log="${tmp}/log"
  debug starting httrack -O "${tmp}" ${moreargs} ${@:${pos}}
  info "running httrack ${@:${pos}}"
  httrack -O "${tmp}" --user-agent="httrack $ver ut ($(uname -omrs))" ${moreargs} ${@:${pos}} >"${log}" 2>&1 &
  crawlpid="$!"
  debug "started cralwer on pid $crawlpid"
  wait "$crawlpid"
  result="$?"
  crawlpid=
  test "$result" -eq 0 || ! result "error code $result" || return 1
  result "OK"
  grep -iE "^[0-9\:]*[[:space:]]Error:" "${tmp}/hts-log.txt" >&2

  # now audit
  while test "$#" -gt 0; do
    case "$1" in
    --no-purge)
      nopurge=1
      ;;
    --summary)
      grep -E "^HTTrack Website Copier/[^ ]* mirror complete in " "${tmp}/hts-log.txt"
      ;;
    --print-files)
      find "${tmp}" -mindepth 1 -type f
      ;;
    --errors)
      shift
      assert_equals "checking errors" "$1" "$(grep -iEc "^[0-9\:]*[[:space:]]Error:" "${tmp}/hts-log.txt")"
      ;;
    --found)
      shift
      info "checking for $1"
      if test -f "${tmp}/$1" ; then
        result "OK"
      else
        result "not found"
        exit 1
      fi
      ;;
    --not-found)
      shift
      info "checking for $1"
      if test -f "${tmp}/$1" ; then
        result "OK"
      else
        result "not found"
        exit 1
      fi
      ;;
    --directory)
      shift
      info "checking for $1"
      if test -d "${tmp}/$1" ; then
        result "OK"
      else
        result "not found"
        exit 1
      fi
      ;;
    --files)
      shift
      nFiles=$(grep -E "^HTTrack Website Copier/[^ ]* mirror complete in " "${tmp}/hts-log.txt" \
        | sed -e 's/.*[[:space:]]\([^ ]*\)[[:space:]]files written.*/\1/g')
      assert_equals "checking files" "$1" "$nFiles"
      ;;
    httrack)
      break;
      ;;
    esac
    shift
  done

  # cleanup
  if test -z "$nopurge"; then
    rm -rf "$tmp"
  else
    tmpdir=
  fi
}

# check args
test "$#" -le 0 && usage && exit 0

# <http://pubs.opengroup.org/onlinepubs/000095399/basedefs/xbd_chap08.html>
tmptopdir=${TMPDIR:-/tmp}
test -d "$tmptopdir" || mkdir -p "${tmptopdir}" || die "can not find a temporary directory ; please set TMPDIR"

# final cleanup
tmpdir=
crawlpid=
nopurge=
verbose=
trap "cleanup" 0 1 2 3 4 5 6 7 8 9 11 13 14 15 16 19 24 25

# working directory
tmpdir="${tmptopdir}/httrack_ut.$$"
mkdir "${tmpdir}"

# rock'in
start-crawl "${@}"

# that's all, folks!
