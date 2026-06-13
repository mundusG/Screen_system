#!/bin/bash
# setup_device.sh - rk3576 大屏推理显示系统 一键部署+自启动配置
#
# 在设备上执行，自动完成:
#   1. 安装依赖 (mosquitto, python3, paho-mqtt, Qt5)
#   2. 配置 MQTT broker
#   3. 部署 nn_bridge 服务
#   4. 部署显示系统自启动
#
# 用法:
#   scp rk3576/setup_device.sh root@<设备IP>:/models/screen_system/
#   ssh root@<设备IP> "bash /models/screen_system/setup_device.sh"

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="/models/screen_system"
BRIDGE_DIR="${PROJECT_DIR}/rk3576"
BUILD_DIR="${PROJECT_DIR}/build"

echo "========================================="
echo " rk3576 大屏推理显示系统 - 环境部署"
echo " 项目目录: ${PROJECT_DIR}"
echo "========================================="

# ─── 1. 安装系统依赖 ───
echo ""
echo "[1/6] 检查并安装系统依赖..."

install_if_missing() {
    if ! dpkg -l "$1" 2>/dev/null | grep -q "^ii"; then
        echo "  安装 $1 ..."
        apt-get install -y "$1"
    else
        echo "  $1 已安装"
    fi
}

apt-get update -qq

install_if_missing mosquitto
install_if_missing mosquitto-clients
install_if_missing python3
install_if_missing python3-pip

# Qt5 依赖 (编译用, 如果需要在设备上编译)
for pkg in qtbase5-dev libqt5widgets5 cmake g++ libopencv-dev; do
    install_if_missing "$pkg" 2>/dev/null || echo "  跳过 $pkg (可选)"
done

# Python 依赖
pip3 install paho-mqtt loguru 2>/dev/null || pip install paho-mqtt loguru 2>/dev/null || echo "  请手动安装: pip3 install paho-mqtt loguru"

# ─── 2. 配置 mosquitto ───
echo ""
echo "[2/6] 配置 MQTT broker (mosquitto)..."

MOSQUITTO_CONF="/etc/mosquitto/conf.d/screen_system.conf"
if [ ! -f "$MOSQUITTO_CONF" ]; then
    cat > "$MOSQUITTO_CONF" << 'EOF'
listener 1883 0.0.0.0
allow_anonymous true
EOF
    echo "  已创建 ${MOSQUITTO_CONF}"
else
    echo "  ${MOSQUITTO_CONF} 已存在"
fi

systemctl enable mosquitto
systemctl restart mosquitto
echo "  mosquitto 已启动并设为开机自启"

# ─── 3. 编译显示系统 (如果有源码) ───
echo ""
echo "[3/6] 编译显示系统..."

if [ -f "${PROJECT_DIR}/CMakeLists.txt" ]; then
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    echo "  编译完成: ${BUILD_DIR}/ScreenInferenceSystem"
else
    echo "  跳过编译 (未找到 CMakeLists.txt, 请手动拷贝编译好的二进制)"
fi

# ─── 4. 创建系统启动脚本 ───
echo ""
echo "[4/6] 创建启动脚本..."

cat > "${PROJECT_DIR}/start_system.sh" << 'SCRIPT_EOF'
#!/bin/bash
# start_system.sh - 启动大屏推理显示系统
#
# 解决的已知问题:
#   - Mali GPU EGL 与 X11 GLX 不兼容 → QT_XCB_GL_INTEGRATION=none
#   - SSH X11 转发不支持 → 使用本地 DISPLAY=:0
#   - eglfs 需要独占 DRM → 使用 xcb (X11) 模式
#   - /tmp/kms.json 重启丢失 → 不再使用 eglfs

PROJECT_DIR="/models/screen_system"

# 等待 X11 就绪 (lightdm/Xorg 启动需要时间)
wait_for_x11() {
    local timeout=30
    local waited=0
    while [ $waited -lt $timeout ]; do
        if DISPLAY=:0 xdpyinfo >/dev/null 2>&1; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    echo "X11 未就绪, 超时退出"
    return 1
}

# 启动 nn_bridge (如果未运行)
start_bridge() {
    if ! pgrep -f "nn_bridge.py" >/dev/null; then
        echo "启动 nn_bridge..."
        cd "${PROJECT_DIR}/rk3576"
        python3 nn_bridge.py bridge_config.json &
        echo "  nn_bridge PID: $!"
    else
        echo "nn_bridge 已在运行"
    fi
}

# 启动显示系统
start_display() {
    echo "启动显示系统..."
    export DISPLAY=:0
    export QT_XCB_GL_INTEGRATION=none

    cd "${PROJECT_DIR}/build"
    exec ./ScreenInferenceSystem "$@"
}

echo "==============================="
echo " 大屏推理显示系统启动"
echo " $(date '+%Y-%m-%d %H:%M:%S')"
echo "==============================="

wait_for_x11 || exit 1
start_bridge
start_display "$@"
SCRIPT_EOF

chmod +x "${PROJECT_DIR}/start_system.sh"
echo "  已创建 ${PROJECT_DIR}/start_system.sh"

# ─── 5. 创建 systemd 服务 ───
echo ""
echo "[5/6] 创建 systemd 服务..."

# nn_bridge 服务
cat > /etc/systemd/system/nn_bridge.service << EOF
[Unit]
Description=NN Bridge - RKNN to Display System
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

# 显示系统服务 (依赖 X11, 在 lightdm 之后启动)
cat > /etc/systemd/system/screen_display.service << EOF
[Unit]
Description=Screen Inference Display System
After=display-manager.service mosquitto.service nn_bridge.service
Wants=mosquitto.service nn_bridge.service

[Service]
Type=simple
Environment=DISPLAY=:0
Environment=QT_XCB_GL_INTEGRATION=none
WorkingDirectory=${BUILD_DIR}
ExecStartPre=/bin/bash -c 'for i in \$(seq 1 30); do DISPLAY=:0 xdpyinfo >/dev/null 2>&1 && exit 0; sleep 1; done; exit 1'
ExecStart=${BUILD_DIR}/ScreenInferenceSystem
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=graphical.target
EOF

systemctl daemon-reload
systemctl enable nn_bridge.service
systemctl enable screen_display.service
echo "  nn_bridge.service      → 已创建并启用"
echo "  screen_display.service → 已创建并启用"

# ─── 6. 完成 ───
echo ""
echo "========================================="
echo " 部署完成!"
echo ""
echo " 服务管理:"
echo "   systemctl start|stop|restart nn_bridge"
echo "   systemctl start|stop|restart screen_display"
echo "   journalctl -u nn_bridge -f"
echo "   journalctl -u screen_display -f"
echo ""
echo " 手动启动 (调试用):"
echo "   bash ${PROJECT_DIR}/start_system.sh"
echo ""
echo " 重启设备后系统将自动启动"
echo "   reboot"
echo ""
echo " 已解决的已知问题:"
echo "   ✓ Mali GPU EGL/GLX 不兼容"
echo "   ✓ X11 显示环境变量"
echo "   ✓ MQTT broker 自启动"
echo "   ✓ nn_bridge 自启动"
echo "   ✓ 显示系统等待 X11 就绪后启动"
echo "========================================="
