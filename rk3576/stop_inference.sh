#!/bin/bash
# stop_inference.sh - 停止推理桥接服务
#
# 停止本项目管理进程: nn_bridge / test_bridge / dposter
# 不影响设备原有的 nn_server / nnmgd / dmg

set +e

echo "停止推理服务..."

pkill -9 -f "test_bridge.py" 2>/dev/null && echo "  test_bridge 已停止"
pkill -9 -f "nn_bridge.py" 2>/dev/null && echo "  nn_bridge 已停止"
pkill -9 -f "dposter/main.py" 2>/dev/null && echo "  dposter 已停止"
pkill -9 -f "dposter/process.py" 2>/dev/null && true

echo "完成"
