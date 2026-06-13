#!/bin/bash
# deploy.sh - 从开发机一键部署到 rk3576 设备
#
# 推送源码 → 远程编译 → 配置自启动
#
# 用法:
#   ./deploy.sh <设备IP> [用户名]
#
# 示例:
#   ./deploy.sh 192.168.77.145          # 使用默认用户 root
#   ./deploy.sh 192.168.77.145 admin    # 指定用户

set -e

DEVICE_IP="${1:?用法: $0 <设备IP> [用户名]}"
DEVICE_USER="${2:-root}"
REMOTE_DIR="/models/screen_system"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "========================================="
echo " 大屏推理显示系统 - 远程部署"
echo " 目标: ${DEVICE_USER}@${DEVICE_IP}"
echo " 本地: ${PROJECT_DIR}"
echo " 远程: ${REMOTE_DIR}"
echo "========================================="

# 1. 创建远程目录
echo ""
echo "[1/4] 创建远程目录..."
ssh "${DEVICE_USER}@${DEVICE_IP}" "mkdir -p ${REMOTE_DIR}/src ${REMOTE_DIR}/config ${REMOTE_DIR}/rk3576"

# 2. 推送文件
echo "[2/4] 推送源码..."

# 核心源码
scp "${PROJECT_DIR}"/src/*.h "${PROJECT_DIR}"/src/*.cpp \
    "${DEVICE_USER}@${DEVICE_IP}:${REMOTE_DIR}/src/"

# 构建文件和入口
scp "${PROJECT_DIR}/CMakeLists.txt" "${PROJECT_DIR}/main.cpp" \
    "${DEVICE_USER}@${DEVICE_IP}:${REMOTE_DIR}/"

# 配置
scp "${PROJECT_DIR}/config/default_config.json" \
    "${DEVICE_USER}@${DEVICE_IP}:${REMOTE_DIR}/config/"

# Qt资源文件 (如果存在)
[ -f "${PROJECT_DIR}/resources.qrc" ] && \
    scp "${PROJECT_DIR}/resources.qrc" "${DEVICE_USER}@${DEVICE_IP}:${REMOTE_DIR}/"

# rk3576 推理端文件
scp "${SCRIPT_DIR}/nn_bridge.py" \
    "${SCRIPT_DIR}/bridge_config.json" \
    "${SCRIPT_DIR}/test_bridge.py" \
    "${SCRIPT_DIR}/setup_device.sh" \
    "${DEVICE_USER}@${DEVICE_IP}:${REMOTE_DIR}/rk3576/"

echo "  文件推送完成"

# 3. 远程执行部署脚本
echo ""
echo "[3/4] 远程执行部署脚本..."
ssh "${DEVICE_USER}@${DEVICE_IP}" "bash ${REMOTE_DIR}/rk3576/setup_device.sh"

# 4. 完成
echo ""
echo "========================================="
echo " 部署完成!"
echo ""
echo " 快速命令:"
echo "   启动全部:  ssh ${DEVICE_USER}@${DEVICE_IP} 'systemctl start nn_bridge screen_display'"
echo "   停止全部:  ssh ${DEVICE_USER}@${DEVICE_IP} 'systemctl stop screen_display nn_bridge'"
echo "   查看日志:  ssh ${DEVICE_USER}@${DEVICE_IP} 'journalctl -u screen_display -f'"
echo "   重启设备:  ssh ${DEVICE_USER}@${DEVICE_IP} 'reboot'"
echo "========================================="
