#!/bin/bash
# setup_device.sh - rk3576 推理端一键部署 + 自启动配置
#
# 在设备上执行，自动完成:
#   1. 安装依赖 (mosquitto, python3, paho-mqtt)
#   2. 配置 MQTT broker
#   3. 部署 nn_bridge systemd 服务
#
# 用法:
#   ssh -p <端口> root@<设备IP> "bash /models/screen_system/rk3576/setup_device.sh"

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="/models/screen_system"
BRIDGE_DIR="${PROJECT_DIR}/rk3576"

echo "========================================="
echo " rk3576 推理端 - 环境部署"
echo "========================================="

# ─── 1. 安装系统依赖 ───
echo ""
echo "[1/4] 安装系统依赖..."

apt-get update -qq

# mosquitto
for pkg in mosquitto mosquitto-clients python3; do
    if dpkg -l "$pkg" 2>/dev/null | grep -q "^ii"; then
        echo "  $pkg 已安装"
    else
        echo "  安装 $pkg ..."
        apt-get install -y "$pkg"
    fi
done

# Python MQTT 库 — 优先用 apt（避免 pip 依赖冲突）
echo "  安装 Python 依赖..."
if apt-get install -y python3-paho-mqtt 2>/dev/null; then
    echo "  paho-mqtt (apt) 已安装"
else
    echo "  apt 不可用，尝试 pip..."
    # 修复 Debian 12 常见的 pkg-resources 版本冲突
    apt-get install -y python3-pkg-resources 2>/dev/null || true
    apt-get install -y python3-pip 2>/dev/null || true
    pip3 install --break-system-packages paho-mqtt 2>/dev/null || \
        pip3 install paho-mqtt 2>/dev/null || \
        echo "  [警告] paho-mqtt 安装失败，请手动安装"
fi

# loguru — 只有 pip 渠道
if python3 -c "import loguru" 2>/dev/null; then
    echo "  loguru 已安装"
else
    pip3 install --break-system-packages loguru 2>/dev/null || \
        pip3 install loguru 2>/dev/null || \
        echo "  [警告] loguru 安装失败（--test 模式需要）"
fi

# ─── 2. 配置 mosquitto ───
echo ""
echo "[2/4] 配置 MQTT broker..."

MOSQUITTO_CONF="/etc/mosquitto/conf.d/screen_system.conf"

# 写入配置（追加方式，不覆盖已有业务配置）
cat > "$MOSQUITTO_CONF" << 'EOF'
listener 1883 0.0.0.0
allow_anonymous true
EOF
echo "  已写入 ${MOSQUITTO_CONF}"

systemctl enable mosquitto 2>/dev/null || true

# 优先用 systemctl 正常重启（不影响其他业务对该 broker 的使用）
if systemctl restart mosquitto 2>/dev/null; then
    echo "  mosquitto 已重启并设为开机自启"
elif pgrep -x mosquitto >/dev/null; then
    # systemctl 不可用但进程已在跑（设备自带），发送 SIGHUP 重载配置
    echo "  systemctl 不可用，mosquitto 已在运行，重载配置..."
    kill -HUP $(pgrep -x mosquitto) 2>/dev/null || true
else
    # 都没跑起来，清理残留后直接启动
    echo "  首次启动 mosquitto..."
    rm -f /run/mosquitto/mosquitto.pid /var/run/mosquitto.pid 2>/dev/null || true
    mkdir -p /run/mosquitto 2>/dev/null || true
    mosquitto -d -c /etc/mosquitto/mosquitto.conf 2>/dev/null || mosquitto -d 2>/dev/null || true
    sleep 1
fi

# 验证
if pgrep -x mosquitto >/dev/null; then
    echo "  mosquitto 运行中"
else
    echo "  [警告] mosquitto 未运行"
    echo "  排查: mosquitto -c /etc/mosquitto/mosquitto.conf -v"
fi

if ss -tlnp 2>/dev/null | grep -q 1883 || netstat -tlnp 2>/dev/null | grep -q 1883; then
    echo "  验证通过: 端口 1883 已监听"
else
    echo "  [警告] 端口 1883 未检测到，请检查 mosquitto 配置"
fi

# ─── 3. 创建系统服务 ───
echo ""
echo "[3/4] 创建 nn_bridge systemd 服务..."

cat > /etc/systemd/system/nn_bridge.service << EOF
[Unit]
Description=NN Bridge - RKNN Inference to MQTT
After=network.target mosquitto.service
Wants=mosquitto.service

[Service]
Type=simple
WorkingDirectory=${BRIDGE_DIR}
ExecStart=/usr/bin/python3 ${BRIDGE_DIR}/nn_bridge.py ${BRIDGE_DIR}/bridge_config.json
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable nn_bridge.service
echo "  nn_bridge.service → 已创建并启用"

# ─── 4. 完成 ───
echo ""
echo "========================================="
echo " 推理端部署完成!"
echo ""
echo " 服务管理:"
echo "   systemctl start|stop|restart nn_bridge"
echo "   journalctl -u nn_bridge -f"
echo ""
echo " 手动启动:"
echo "   cd ${BRIDGE_DIR} && ./start_inference.sh"
echo ""
echo " 下一步:"
echo "   1. 推送模型到 ${PROJECT_DIR}/model/"
echo "   2. 编辑 ${BRIDGE_DIR}/bridge_config.json (preview.host)"
echo "   3. ${BRIDGE_DIR}/start_inference.sh"
echo "========================================="
