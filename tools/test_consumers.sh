#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
scratch_dir=$(mktemp -d "${TMPDIR:-/tmp}/grib-consumers.XXXXXX")
trap 'rm -rf "$scratch_dir"' EXIT
sdk_build_dir="${GRIB_CONSUMER_BUILD_DIR:-$source_dir/build}"
cmake -S "$source_dir" -B "$sdk_build_dir" -DCMAKE_BUILD_TYPE=Release -DGRIB_ENABLE_LTO=OFF "$@"
cmake --build "$sdk_build_dir" --target grib_api --parallel 4
cmake --install "$sdk_build_dir" --prefix "$scratch_dir/installed"
mv "$scratch_dir/installed" "$scratch_dir/relocated"
for dependency in glaze gtest gmock eccodes.h; do
    test ! -e "$scratch_dir/relocated/include/$dependency"
done
fixture="$source_dir/tests/fixtures/sample_2t.grib2"
cmake -S "$source_dir/tests/consumer" -B "$scratch_dir/package" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$scratch_dir/relocated"
cmake --build "$scratch_dir/package" --parallel 4
"$scratch_dir/package/consumer" "$fixture"
cmake -S "$source_dir/tests/consumer" -B "$scratch_dir/fetch" \
    -DCMAKE_BUILD_TYPE=Release -DGRIB_SOURCE_DIR="$source_dir" "$@"
cmake --build "$scratch_dir/fetch" --parallel 4
"$scratch_dir/fetch/consumer" "$fixture"
echo 'Relocated installed and FetchContent decoder consumers pass'

python3 - "$scratch_dir/fetch/compile_commands.json" <<'PYCODE'
import json,sys
commands = [entry['command'] for entry in json.load(open(sys.argv[1])) if entry['file'].endswith('/tests/consumer/main.cpp')]
assert len(commands) == 1, commands
assert 'eccodes-prefix/include' not in commands[0], 'Private ecCodes headers leaked to consumer'
print('Consumer cannot inherit private decoder include directories')
PYCODE
