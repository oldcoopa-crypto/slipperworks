#!/bin/sh
# 重建 ASAN 模拟器并跑按键脚本(用法: tools/simtest.sh 脚本.txt)
set -e
cd "$(dirname "$0")/.."
source /Users/hao/code/emsdk/emsdk_env.sh >/dev/null 2>&1
export PYTHONPATH=/Users/hao/code/coopa-sdk/tools
python3 - <<'PY'
from pathlib import Path
from coopa import fonts, manifest, webbuild
from coopa.sdk import Sdk
sdk = Sdk.load(Path('/Users/hao/code/coopa-sdk'))
game = Path('.').resolve()
m, _ = manifest.load(game, sdk.version, sdk.kit_api)
font_dir, _ = fonts.ensure(sdk, __import__('coopa.source', fromlist=['charset']).charset(game, m), Path('.coopa/fonts'))
objs, p = webbuild.compile_game(sdk, game, m, font_dir, "webasan", Path(".coopa/obj-webasan"))
assert not p, p
p = webbuild.link(sdk, objs, "webasan", Path(".coopa/sim"))
assert not p, p
print("sim rebuilt")
PY
node .coopa/sim/passport_sim.js --headless --clock step --seed 1 --wipe --watchdog 120 \
  --state .coopa/tour/state --out .coopa/tour/out \
  --partitions /Users/hao/code/coopa-sdk/card/partitions.csv --script "${1:-.coopa/tour/play.txt}"
