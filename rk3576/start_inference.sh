#!/bin/bash
# start_inference.sh - 启动推理桥接服务
#
# 职责：启动 dposter → 部署算法包 → 启动 nn_bridge
# dposter 负责 dmg→nn_server 数据管线，nn_bridge 负责格式转换→展示端
#
# 数据流: dmg(RTSP采集) → nn_server_200(RKNN推理) → MQTT → dposter(处理) → nn_bridge(格式转换) → 显示端
#
# 用法:
#   ./start_inference.sh                 # 完整启动: dposter + 算法包 + nn_bridge
#   ./start_inference.sh --no-dposter    # 跳过 dposter 启动
#   ./start_inference.sh --deploy-only   # 只部署算法包，不启动任何服务
#   ./start_inference.sh --no-deploy     # 只启动服务，不部署算法包
#   ./start_inference.sh --test          # 用 test_bridge.py 模拟推理输出

set +e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

SMART_GW="/oem/smart-gw"
MODEL_DIR="${SMART_GW}/models/m200"
BRIDGE_SCRIPT="${SCRIPT_DIR}/nn_bridge.py"
BRIDGE_CONF="${SCRIPT_DIR}/bridge_config.json"

# dposter 路径自动检测（优先设备原有实例，回退到本地副本）
for _dir in "/models/m200/dposter" "${SCRIPT_DIR}/dposter"; do
    if [ -f "${_dir}/main.py" ]; then
        DPOSTER_DIR="$_dir"
        break
    fi
done
DPOSTER_MAIN="${DPOSTER_DIR}/main.py"
DPOSTER_CONF="${DPOSTER_DIR}/args.json"
LOG_DIR="${PROJECT_DIR}/log"

DEPLOY_ONLY=false
NO_DEPLOY=false
TEST_MODE=false
NO_DPOSTER=false

for arg in "$@"; do
    case "$arg" in
        --deploy-only) DEPLOY_ONLY=true ;;
        --no-deploy)   NO_DEPLOY=true ;;
        --test)        TEST_MODE=true ;;
        --no-dposter)  NO_DPOSTER=true ;;
    esac
done

echo "========================================="
echo " 推理桥接启动"
echo " $(date '+%Y-%m-%d %H:%M:%S')"
echo "========================================="

# ─── 自修复：清理所有文本文件的 CRLF ───
if [ -d "${SCRIPT_DIR}" ]; then
    find "${SCRIPT_DIR}" -type f \( -name '*.sh' -o -name '*.py' -o -name '*.json' -o -name '*.yaml' -o -name '*.yml' -o -name '*.conf' -o -name '*.example' -o -name '*.txt' \) \
        -exec sh -c 'tr -d "\r" < "$1" > /tmp/crlf_fix && mv /tmp/crlf_fix "$1"' _ {} \; 2>/dev/null
fi

# ─── 1. 清理旧进程 ───
echo ""
echo "[1] 清理旧进程..."
pkill -9 -f "test_bridge.py" 2>/dev/null && echo "  已停止 test_bridge"
pkill -9 -f "nn_bridge.py" 2>/dev/null && echo "  已停止 nn_bridge"
if [ "$NO_DPOSTER" = false ]; then
    pkill -9 -f "dposter/main.py" 2>/dev/null && echo "  已停止 dposter"
    pkill -9 -f "dposter/process.py" 2>/dev/null && true  # 确保子线程也清理
fi
sleep 0.5
echo "  清理完成"

# ─── 2. 确保 mosquitto 运行 ───
echo ""
echo "[2] 检查 MQTT broker..."
if pgrep -x mosquitto >/dev/null; then
    echo "  mosquitto 已在运行"
else
    echo "  启动 mosquitto..."
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

    echo "  算法包部署完成（设备重启后 nnmgd 自动加载）"
else
    echo ""
    echo "[3] 跳过算法包部署 (--no-deploy)"
fi

[ "$DEPLOY_ONLY" = true ] && echo "" && echo "仅部署模式，完成。" && exit 0

# ─── 4. 启动 dposter ───
if [ "$NO_DPOSTER" = false ]; then
    echo ""
    echo "[4] 启动 dposter..."
    if [ ! -f "$DPOSTER_MAIN" ]; then
        echo "  [错误] dposter 入口未找到: ${DPOSTER_MAIN}"
    elif [ ! -f "$DPOSTER_CONF" ]; then
        echo "  [错误] dposter 配置未找到: ${DPOSTER_CONF}"
    else
        mkdir -p "$LOG_DIR"
        cd "$DPOSTER_DIR"
        python3 main.py args.json >>"$LOG_DIR/dposter.log" 2>&1 &
        DPOSTER_PID=$!
        sleep 1
        if kill -0 $DPOSTER_PID 2>/dev/null; then
            echo "  dposter 已启动 (PID: $DPOSTER_PID)"
            echo "  日志: ${LOG_DIR}/dposter.log"
        else
            echo "  [错误] dposter 启动失败，查看: ${LOG_DIR}/dposter.log"
        fi
    fi
else
    echo ""
    echo "[4] 跳过 dposter 启动 (--no-dposter)"
fi

# ─── 5. 启动 nn_bridge ───
STEP_NUM=5
[ "$NO_DPOSTER" = true ] && STEP_NUM=5  # 始终显示步骤5
echo ""
echo "[${STEP_NUM}] 启动 nn_bridge..."
cd "${SCRIPT_DIR}"

if [ "$TEST_MODE" = true ]; then
    echo "  测试模式: 启动模拟推理..."
    python3 test_bridge.py --channels 4 --interval 0.5 &
    echo "  test_bridge (PID: $!)"
fi

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

# ─── 完成 ───
echo ""
echo "========================================="
echo " 启动完成!"
echo ""
echo " 进程状态:"
pgrep -a "dposter/main" | sed 's/^/   /'
pgrep -a "nn_bridge" | sed 's/^/   /'
[ "$TEST_MODE" = true ] && pgrep -a "test_bridge" | sed 's/^/   /'
echo ""
echo " 设备服务状态 (本脚本不管理):"
pgrep -a "nn_server_200" | sed 's/^/   /' || echo "   nn_server_200 未运行"
pgrep -a "^dmg" | sed 's/^/   /' || echo "   dmg 未运行"
echo ""
echo " 停止: ${SCRIPT_DIR}/stop_inference.sh"
echo "========================================="
