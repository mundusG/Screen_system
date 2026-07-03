#!/bin/bash
# deploy.sh - 从开发机一键部署到 rk3576 推理端
#
# 推送文件列表由 git ls-files 自动生成（与 git 保持一致，避免手工维护遗漏）
# 不推送：.rknn 模型文件（太大且不常变，需单独 scp）
#
# 用法:
#   ./deploy.sh <设备IP> [用户名] [远程目录] [SSH端口]
#
# 示例:
#   ./deploy.sh 192.168.77.145                          # 局域网，默认 root, port 22
#   ./deploy.sh 42.193.140.103 root /models/screen_system 61837  # 公网端口转发

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

# 1. 创建远程目录结构（tar 解压需要父目录存在）
echo ""
echo "[1/3] 创建远程目录..."
ssh ${SSH_OPTS} "${DEVICE_USER}@${DEVICE_IP}" "mkdir -p \
    ${REMOTE_DIR}/rk3576 \
    ${REMOTE_DIR}/model \
    ${REMOTE_DIR}/log"

# 2. 生成文件列表 & 推送
echo "[2/3] 推送推理端配置..."

# 本地剥离所有文本文件的 CRLF（防止 Windows 编辑器的 \r 污染）
cd "${SCRIPT_DIR}"
find . -type f \( -name '*.sh' -o -name '*.py' -o -name '*.json' -o -name '*.yaml' -o -name '*.yml' -o -name '*.conf' -o -name '*.example' -o -name '*.txt' \) \
    ! -path './.git/*' ! -path './build/*' \
    -exec sh -c 'tr -d "\r" < "$1" > /tmp/rk3576_crlf_fix && mv /tmp/rk3576_crlf_fix "$1"' _ {} \;

# 从 git 生成文件列表，排除模型/冗余/构建产物
cd "${PROJECT_DIR}"
git ls-files rk3576/ \
    | grep -v '\.rknn$' \
    | grep -v 'deploy.sh' \
    | grep -v 'nn_server/conf/' \
    | grep -v 'nn_server/data/' \
    | grep -v 'nn_server/nn_server.conf' \
    | grep -v '__pycache__/' \
    | grep -v '\.pyc$' \
    > /tmp/rk3576_files.txt

FILE_COUNT=$(wc -l < /tmp/rk3576_files.txt)
echo "  推送 ${FILE_COUNT} 个文件:"
sed 's/^/    /' /tmp/rk3576_files.txt

cd "${SCRIPT_DIR}"
tar -czf /tmp/rk3576_deploy.tar.gz \
    --exclude='*.rknn' \
    --exclude='*.pyc' \
    --exclude='__pycache__' \
    --exclude='.DS_Store' \
    -T /tmp/rk3576_files.txt \
    -C "${PROJECT_DIR}"

scp ${SCP_OPTS} /tmp/rk3576_deploy.tar.gz "${DEVICE_USER}@${DEVICE_IP}:/tmp/"

# 远端：解压 + 清理 CRLF + 生成 run.sh
ssh ${SSH_OPTS} "${DEVICE_USER}@${DEVICE_IP}" "
cd ${REMOTE_DIR}/rk3576 && tar -xzf /tmp/rk3576_deploy.tar.gz && rm /tmp/rk3576_deploy.tar.gz && \
    find . -type f \( -name '*.sh' -o -name '*.py' -o -name '*.json' -o -name '*.yaml' -o -name '*.yml' -o -name '*.conf' -o -name '*.example' -o -name '*.txt' \) \
        -exec sh -c 'tr -d \"\\r\" < \"\$1\" > /tmp/crlf_fix && mv /tmp/crlf_fix \"\$1\"' _ {} \; && \
    chmod +x *.sh && \
    if [ ! -f bridge_config.json ]; then cp bridge_config.json.example bridge_config.json; echo '  Created bridge_config.json from template'; fi && \
    if [ ! -f filter_config.json ]; then cp filter_config.json.example filter_config.json; echo '  Created filter_config.json from template'; fi && \
    cat > run.sh << 'RUNEOF'
#!/bin/bash
cd \"\$(dirname \"\$0\")\"
find . -maxdepth 2 -type f \( -name '*.sh' -o -name '*.py' \) \
    -exec sh -c 'tr -d \"\\r\" < \"\$1\" > /tmp/.f && mv /tmp/.f \"\$1\"' _ {} \;
chmod +x *.sh
exec bash start_inference.sh \"\$@\"
RUNEOF
    chmod +x run.sh && echo '  run.sh 已生成'
"
rm /tmp/rk3576_deploy.tar.gz /tmp/rk3576_files.txt
echo "  推送完成"

# 3. 完成
echo ""
echo "========================================="
echo " 部署完成!"
echo ""
echo " 下一步:"
echo "   1. 推送模型文件:"
echo "      scp -P ${SSH_PORT} your_model.rknn ${DEVICE_USER}@${DEVICE_IP}:${REMOTE_DIR}/model/"
echo "   2. (首次) 运行安装脚本:"
echo "      ssh ${SSH_OPTS} ${DEVICE_USER}@${DEVICE_IP} 'bash ${REMOTE_DIR}/rk3576/setup_device.sh'"
echo "   3. 编辑 bridge_config.json:"
echo "      ssh ${SSH_OPTS} ${DEVICE_USER}@${DEVICE_IP} 'vi ${REMOTE_DIR}/rk3576/bridge_config.json'"
echo "      改 preview.host = <设备局域网IP>"
echo "   4. 启动推理:"
echo "      ssh ${SSH_OPTS} ${DEVICE_USER}@${DEVICE_IP} 'cd ${REMOTE_DIR}/rk3576 && ./run.sh'"
echo ""
echo " 测试模式 (无需模型/摄像头):"
echo "      ssh ${SSH_OPTS} ${DEVICE_USER}@${DEVICE_IP} 'cd ${REMOTE_DIR}/rk3576 && ./run.sh --test'"
echo "========================================="
