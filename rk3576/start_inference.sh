#!/bin/bash
# start_inference.sh - 启动推理桥接服务
#
# 职责：部署算法包 + 启动 nn_bridge
# 不重启 nnmgd/dmg/nn_server — 这些由设备 dposter 生态自管理
#
# 数据流: dmg(RTSP采集) → nn_server_200(RKNN推理) → MQTT → nn_bridge(格式转换) → 显示端
#
# 用法:
#   ./start_inference.sh              # 部署算法包 + 启动 nn_bridge
#   ./start_inference.sh --deploy-only # 只部署算法包，不启动 nn_bridge
#   ./start_inference.sh --no-deploy   # 只启动 nn_bridge，不部署算法包
#   ./start_inference.sh --test        # 用 test_bridge.py 模拟推理输出

set +e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

SMART_GW="/oem/smart-gw"
MODEL_DIR="${SMART_GW}/models/m200"
BRIDGE_SCRIPT="${SCRIPT_DIR}/nn_bridge.py"
BRIDGE_CONF="${SCRIPT_DIR}/bridge_config.json"

DEPLOY_ONLY=false
NO_DEPLOY=false
TEST_MODE=false

for arg in "$@"; do
    case "$arg" in
        --deploy-only) DEPLOY_ONLY=true ;;
        --no-deploy)   NO_DEPLOY=true ;;
        --test)        TEST_MODE=true ;;
    esac
done

echo "========================================="
echo " 推理桥接启动"
echo " $(date '+%Y-%m-%d %H:%M:%S')"
echo "========================================="

# ─── 1. 清理旧 bridge 进程 ───
echo ""
echo "[1] 清理旧进程..."
pkill -9 -f "test_bridge.py" 2>/dev/null && echo "  已停止 test_bridge"
pkill -9 -f "nn_bridge.py" 2>/dev/null && echo "  已停止 nn_bridge"
pkill -9 -f "dposter/main.py" 2>/dev/null && echo "  已停止 dposter"
sleep 0.5
echo "  清理完成"

# ─── 2. 确保 mosquitto 运行 ───
echo ""
echo "[2] 检查 MQTT broker..."
if pgrep -x mosquitto >/dev/null; then
    echo "  mosquitto 已在运行"
else
    echo "  启动 mosquitto..."
    # 清理残留 pid 文件
    rm -f /run/mosquitto/mosquitto.pid /var/run/mosquitto.pid 2>/dev/null || true
    mkdir -p /run/mosquitto 2>/dev/null || true
    mosquitto -d -c /etc/mosquitto/mosquitto.conf 2>/dev/null || mosquitto -d 2>/dev/null || true
    sleep 1
    if pgrep -x mosquitto >/dev/null; then
        echo "  mosquitto 已启动"
    else
        echo "  [错误] mosquitto 启动失败"
    fi
fi

# ─── 3. 部署算法包 ───
if [ "$NO_DEPLOY" = false ]; then
    echo ""
    echo "[3] 部署算法包到 ${MODEL_DIR}..."
    mkdir -p "${MODEL_DIR}/nn_server"

    cp -f "${SCRIPT_DIR}/m200/nn_server.yaml" "${MODEL_DIR}/"
    cp -f "${SCRIPT_DIR}/m200/base.json" "${MODEL_DIR}/"
    cp -f "${SCRIPT_DIR}/m200/nn.json" "${MODEL_DIR}/"
    cp -f "${SCRIPT_DIR}/m200/nn.extend.json" "${MODEL_DIR}/"
    cp -f "${SCRIPT_DIR}/m200/nn_server/nn_server.conf" "${MODEL_DIR}/nn_server/"

    # 模型文件
    if ls "${PROJECT_DIR}/model/"*.rknn >/dev/null 2>&1; then
        cp -f "${PROJECT_DIR}/model/"*.rknn "${MODEL_DIR}/"
        echo "  模型: $(ls ${MODEL_DIR}/*.rknn 2>/dev/null | xargs -n1 basename)"
    elif ls "${MODEL_DIR}/"*.rknn >/dev/null 2>&1; then
        echo "  使用已有模型: $(ls ${MODEL_DIR}/*.rknn | xargs -n1 basename)"
    else
        echo "  [警告] 无模型文件"
    fi

    # 注意: 不部署 chma/ 和 db/mpp/ — 通道配置由设备服务自管理
    # nn_bridge 会从 MQTT 消息流中自动发现活跃通道

    echo "  算法包部署完成（设备重启后 nnmgd 自动加载）"
else
    echo ""
    echo "[3] 跳过算法包部署 (--no-deploy)"
fi

[ "$DEPLOY_ONLY" = true ] && echo "" && echo "仅部署模式，完成。" && exit 0

# ─── 4. 启动 nn_bridge ───
echo ""
echo "[4] 启动 nn_bridge..."
cd "${SCRIPT_DIR}"

if [ "$TEST_MODE" = true ]; then
    echo "  测试模式: 启动模拟推理..."
    python3 test_bridge.py --channels 4 --interval 0.5 &
    echo "  test_bridge (PID: $!)"
fi

LOG_DIR="$(dirname "$SCRIPT_DIR")/log"
mkdir -p "$LOG_DIR"
python3 "$BRIDGE_SCRIPT" "$BRIDGE_CONF" >"$LOG_DIR/nn_bridge.log" 2>&1 &
BRIDGE_PID=$!
sleep 1
if kill -0 $BRIDGE_PID 2>/dev/null; then
    echo "  nn_bridge 已启动 (PID: $BRIDGE_PID)"
    echo "  日志: $LOG_DIR/nn_bridge.log"
else
    echo "  [错误] nn_bridge 启动失败"
    exit 1
fi

# ─── 5. 启动 dposter (告警处理/截图保存) ───
DPOSTER_DIR="/models/m200/dposter"
echo ""
echo "[5] 启动 dposter (告警/截图)..."
if [ -f "${DPOSTER_DIR}/main.py" ]; then
    python3 "${DPOSTER_DIR}/main.py" "${DPOSTER_DIR}/args.json" >"$LOG_DIR/dposter.log" 2>&1 &
    DPOSTER_PID=$!
    sleep 1
    if kill -0 $DPOSTER_PID 2>/dev/null; then
        echo "  dposter 已启动 (PID: $DPOSTER_PID)"
    else
        echo "  [警告] dposter 启动失败，告警截图功能不可用"
    fi
else
    echo "  [警告] dposter 未找到: ${DPOSTER_DIR}"
fi

# ─── 完成 ───
echo ""
echo "========================================="
echo " 启动完成!"
echo ""
echo " 进程状态:"
pgrep -a "nn_bridge" | sed 's/^/   /'
pgrep -a "dposter" | sed 's/^/   /'
[ "$TEST_MODE" = true ] && pgrep -a "test_bridge" | sed 's/^/   /'
echo ""
echo " 设备服务状态 (本脚本不管理):"
pgrep -a "nn_server_200" | sed 's/^/   /' || echo "   nn_server_200 未运行"
pgrep -a "^dmg" | sed 's/^/   /' || echo "   dmg 未运行"
echo ""
echo " 停止: ${SCRIPT_DIR}/stop_inference.sh"
echo "========================================="
