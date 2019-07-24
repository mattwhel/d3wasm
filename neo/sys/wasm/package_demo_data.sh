#!/usr/bin/env bash
python $(dirname $(which emcc))/tools/file_packager.py demo00.data --preload $1@/usr/local/share/d3wasm/base/demo00.pk4 --js-output=demo00.js --use-preload-cache --no-heap-copy
