#!/bin/bash
# deploy.sh — 从开发机一键部署 libscreen_detect_rknn.so 到 RK3588 推理端
#
# 用法:
#   ./deploy.sh <设备IP> [用户名] [远程目录] [SSH端口]
#
# 示例:
#   ./deploy.sh 192.168.1.200                              # 局域网, root
#   ./deploy.sh 42.193.140.103 root /models/screen_system 61837  # 公网端口转发
#
# 需要:
#   交叉编译工具链 (aarch64-linux-gnu) 已安装
#   RK3588 sysroot 已设置 (环境变量 RK3588_SYSROOT 或 ~/rk3588_sysroot)
#
# edge_server 会在 algo_init 时接收 config/bridge_config.json 内容作为 config_json 参数

set -e

DEVICE_IP="${1:?用法: $0 <设备IP> [用户名] [远程目录] [SSH端口]}"
DEVICE_USER="${2:-root}"
REMOTE_DIR="${3:-/models/screen_system}"
SSH_PORT="${4:-22}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RK3588_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_DIR="$(dirname "$RK3588_DIR")"

SSH_OPTS="-p ${SSH_PORT}"
SCP_OPTS="-P ${SSH_PORT}"

echo "========================================="
echo " RK3588 .so 算法包部署"
echo " 目标: ${DEVICE_USER}@${DEVICE_IP}:${REMOTE_DIR}  (端口: ${SSH_PORT})"
echo "========================================="

# 1. 交叉编译
echo ""
echo "[1/4] 交叉编译 libscreen_detect_rknn.so..."

BUILD_DIR="${RK3588_DIR}/build"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# 检测 toolchain 文件
TOOLCHAIN_FILE="${RK3588_DIR}/toolchain/rk3588_toolchain.cmake"
if [ ! -f "${TOOLCHAIN_FILE}" ]; then
    echo "ERROR: toolchain 文件未找到: ${TOOLCHAIN_FILE}"
    exit 1
fi

cmake "${RK3588_DIR}" -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

SO_FILE="${BUILD_DIR}/algo/libscreen_detect_rknn.so"
if [ ! -f "${SO_FILE}" ]; then
    echo "ERROR: 编译产物未找到: ${SO_FILE}"
    exit 1
fi

echo "  编译完成: ${SO_FILE}"
file "${SO_FILE}"

# 2. 创建远程目录
echo ""
echo "[2/4] 创建远程目录..."
ssh ${SSH_OPTS} "${DEVICE_USER}@${DEVICE_IP}" "mkdir -p \
    ${REMOTE_DIR}/rk3588/algo \
    ${REMOTE_DIR}/rk3588/config \
    ${REMOTE_DIR}/model \
    ${REMOTE_DIR}/log"

# 3. 推送文件
echo ""
echo "[3/4] 推送文件..."

# 推送 .so
scp ${SCP_OPTS} "${SO_FILE}" \
    "${DEVICE_USER}@${DEVICE_IP}:${REMOTE_DIR}/rk3588/algo/"

# 推送配置模板
cp "${RK3588_DIR}/config/bridge_config.json.example" /tmp/bridge_config.json.example
tr -d '\r' < /tmp/bridge_config.json.example > /tmp/bridge_config.json.clean
scp ${SCP_OPTS} /tmp/bridge_config.json.clean \
    "${DEVICE_USER}@${DEVICE_IP}:${REMOTE_DIR}/rk3588/config/bridge_config.json.example"
rm -f /tmp/bridge_config.json.example /tmp/bridge_config.json.clean

echo "  推送完成"

# 4. 远端初始化
echo ""
echo "[4/4] 远端初始化..."
ssh ${SSH_OPTS} "${DEVICE_USER}@${DEVICE_IP}" "
cd ${REMOTE_DIR}/rk3588/config && \
    if [ ! -f bridge_config.json ]; then
        cp bridge_config.json.example bridge_config.json
        echo '  Created bridge_config.json from template'
    fi && \
    echo '  .so deployed to ${REMOTE_DIR}/rk3588/algo/libscreen_detect_rknn.so'
"

echo ""
echo "========================================="
echo " 部署完成!"
echo ""
echo " 下一步:"
echo "   1. 推送模型文件:"
echo "      scp -P ${SSH_PORT} your_model.rknn ${DEVICE_USER}@${DEVICE_IP}:${REMOTE_DIR}/model/"
echo "   2. 编辑 bridge_config.json:"
echo "      ssh ${SSH_OPTS} ${DEVICE_USER}@${DEVICE_IP} 'vi ${REMOTE_DIR}/rk3588/config/bridge_config.json'"
echo "      改 class_names = 实际类别列表"
echo "      改 mqtt_host / preview_host = 实际IP"
echo "      改 shm_list = edge_server 使用的 SHM 路径"
echo "   3. 配置 edge_server 加载此 .so:"
echo "      model_path = ${REMOTE_DIR}/model/your_model.rknn"
echo "      config_json = 读取 ${REMOTE_DIR}/rk3588/config/bridge_config.json 内容传入"
echo "   4. 重启 edge_server 使 .so 生效"
echo ""
echo " 验证 MQTT 输出:"
echo "   mosquitto_sub -h ${DEVICE_IP} -t 'inference/bridge/+/channels' -v -C 1"
echo "   mosquitto_sub -h ${DEVICE_IP} -t 'inference/camera/+/detections' -v -C 5"
echo "========================================="
