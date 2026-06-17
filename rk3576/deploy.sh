#!/bin/bash
# deploy.sh - 从开发机一键部署到 rk3576 设备
#
# 推送推理端配置 + 桥接脚本 + 通道配置 + 启动脚本
#
# 用法:
#   ./deploy.sh <设备IP> [用户名] [远程目录] [SSH端口]
#
# 示例:
#   ./deploy.sh 192.168.77.145                          # 默认 root, /models/screen_system, port 22
#   ./deploy.sh 192.168.77.145 root ~/screen_system     # 指定远程目录
#   ./deploy.sh 42.193.140.103 root /models/screen_system 61837  # 自定义端口

set -e

DEVICE_IP="${1:?用法: $0 <设备IP> [用户名] [远程目录] [SSH端口]}"
DEVICE_USER="${2:-root}"
REMOTE_DIR="${3:-/models/screen_system}"
SSH_PORT="${4:-22}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

SSH_OPTS="-p ${SSH_PORT}"
SCP_OPTS="-P ${SSH_PORT}"

echo "========================================="
echo " 推理端部署"
echo " 目标: ${DEVICE_USER}@${DEVICE_IP}:${REMOTE_DIR}  (端口: ${SSH_PORT})"
echo "========================================="

# 1. 创建远程目录结构
echo ""
echo "[1/3] 创建远程目录..."
ssh ${SSH_OPTS} "${DEVICE_USER}@${DEVICE_IP}" "mkdir -p \
    ${REMOTE_DIR}/rk3576/m200 \
    ${REMOTE_DIR}/rk3576/db/mpp \
    ${REMOTE_DIR}/rk3576/chma/m200/{ch0,ch1,ch2,ch3} \
    ${REMOTE_DIR}/model \
    ${REMOTE_DIR}/log"

# 2. 推送配置文件 (tar 打包一次传输)
echo "[2/3] 推送推理端配置..."

cd "${SCRIPT_DIR}"
tar -czf /tmp/rk3576_deploy.tar.gz \
    m200/*.yaml \
    m200/*.json \
    m200/nn_server/nn_server.conf \
    db/mpp/*.json \
    chma/m200/ch*/area.json \
    chma/m200/ch*/freq.json \
    nn_bridge.py \
    bridge_config.json \
    test_bridge.py \
    start_inference.sh \
    stop_inference.sh \
    setup_device.sh

scp ${SCP_OPTS} /tmp/rk3576_deploy.tar.gz "${DEVICE_USER}@${DEVICE_IP}:/tmp/"
ssh ${SSH_OPTS} "${DEVICE_USER}@${DEVICE_IP}" "cd ${REMOTE_DIR}/rk3576 && tar -xzf /tmp/rk3576_deploy.tar.gz && rm /tmp/rk3576_deploy.tar.gz && chmod +x *.sh"
rm /tmp/rk3576_deploy.tar.gz
echo "  配置文件推送完成"

# 3. 完成
echo ""
echo "========================================="
echo " 部署完成!"
echo ""
echo " 下一步:"
echo "   1. 将 .rknn 模型文件复制到设备: ${REMOTE_DIR}/model/"
echo "        scp ${SCP_OPTS} your_model.rknn ${DEVICE_USER}@${DEVICE_IP}:${REMOTE_DIR}/model/"
echo "   2. 启动推理端:"
echo "        ssh ${SSH_OPTS} ${DEVICE_USER}@${DEVICE_IP} 'cd ${REMOTE_DIR}/rk3576 && ./start_inference.sh'"
echo ""
echo " 测试模式 (无需模型/摄像头):"
echo "        ssh ${SSH_OPTS} ${DEVICE_USER}@${DEVICE_IP} 'cd ${REMOTE_DIR}/rk3576 && ./start_inference.sh --test'"
echo ""
echo " 停止:"
echo "        ssh ${SSH_OPTS} ${DEVICE_USER}@${DEVICE_IP} 'cd ${REMOTE_DIR}/rk3576 && ./stop_inference.sh'"
echo "========================================="
