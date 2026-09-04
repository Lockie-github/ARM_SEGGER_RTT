#!/bin/sh
exec /usr/bin/clang \
  -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  -g \
  "$@"
