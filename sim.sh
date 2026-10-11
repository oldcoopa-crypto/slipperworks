#!/bin/sh
# 启动《库巴的拖鞋工坊》本地模拟器。
# 用法: ./sim.sh          前台跑,Ctrl+C 停
#       ./sim.sh --seed 12345  复现同一串随机数
# 页面在 http://localhost:8793/(要 Chrome 或 Safari 15.2+,需要跨源隔离)
set -e
cd "$(dirname "$0")"
PORT=8793
# 已经开着就直接打开,别撞自己的端口
if curl -s --max-time 2 -o /dev/null "http://localhost:$PORT/"; then
  echo "模拟器已在 http://localhost:$PORT/ 运行"
  open "http://localhost:$PORT/"
  exit 0
fi
source /Users/hao/code/emsdk/emsdk_env.sh >/dev/null 2>&1
exec coopa run --port "$PORT" "$@"
