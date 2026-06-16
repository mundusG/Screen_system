# Screen Inference System (大屏推理显示系统)

基于 Qt5 + OpenCV DNN 的多路摄像头 YOLO 实时推理大屏显示系统，采用深色赛博朋克风格专业监控界面。

## 界面预览

```
┌────┬──────────────────────────────────────┬──────────┐
│    │  ┌────────┬────────┬────────┬──────┐ │ 设备列表 │
│ 导 │  │ CAM1   │ CAM2   │ CAM3   │ CAM4 │ │ ● 产线1前│
│ 航 │  │ ●LIVE  │ ●LIVE  │ ●LIVE  │ ●LIVE│ │ ● 产线1后│
│ 栏 │  ├────────┼────────┼────────┼──────┤ │ ○ 产线1左│
│    │  │ CAM5   │ CAM6   │ CAM7   │ CAM8 │ │   ...    │
│ SI │  │ ●LIVE  │ ●LIVE  │ ●LIVE  │ ●LIVE│ ├──────────┤
│    │  └────────┴────────┴────────┴──────┘ │ 实时告警 │
│ ▶  │  [1×1][2×2][2×4][3×3] Snapshot FS ⚙ │ ▌CAM1 95%│
└────┴──────────────────────────────────────┴──────────┘
```

## 架构

```
CameraCapture (独立线程)
  → InferenceEngine (独立线程, YOLO ONNX)
    → SmoothingFilter (主线程, EMA+IoU)
      → VideoWidget (Qt QPainter 渲染)
        → AlertPanel (检测告警推送)
```

8 路摄像头，支持 1×1 / 2×2 / 2×4 / 3×3 网格切换，每路独立模型权重。

## 开发环境

| 项目 | 详情 |
|------|------|
| 系统 | **WSL2 + Ubuntu 24.04** |
| 编译器 | GCC 13, CMake 3.28, C++17 |
| Qt | 5.15.13 (Widgets, Core, Gui) |
| OpenCV | 4.6.0 (core, imgproc, videoio, dnn) |
| 中文支持 | WenQuanYi Micro Hei (文泉驿微米黑) |

## 开发工作流

源码放在 Windows 下编辑，WSL 下编译运行。每次修改后执行同步脚本即可：

```bash
cd ~/screen_system && ./sync.sh
```

`sync.sh` 内容（放在 `~/screen_system/sync.sh`，只需创建一次）：

```bash
#!/bin/bash
set -e
SRC=/mnt/d/\!code/screen_system
DST=~/screen_system

# 增量同步源码（跳过 build 目录）
rsync -av --delete \
    --exclude build/ \
    --exclude .git/ \
    --exclude .vscode/ \
    "$SRC"/ "$DST"/

# 编译
cd "$DST"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
echo "Done. Run: cd ~/screen_system/build && ./ScreenInferenceSystem"
```

首次使用前确保 `rsync` 已安装（`sudo apt install rsync`），并且脚本有执行权限（`chmod +x sync.sh`）。

## 路径解析

配置和模型文件的搜索优先级：

- **配置文件**: 命令行 `-c` → `SCREEN_SYSTEM_CONFIG` 环境变量 → XDG config → exe 旁 → install share
- **模型文件**: 绝对路径 → `SCREEN_SYSTEM_MODEL_DIR` 环境变量 → exe 旁 `models/` → install share → CWD

### 安装部署 (make install)

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc) && sudo make install
/usr/local/bin/ScreenInferenceSystem
# 自动在 /usr/local/share/ScreenInferenceSystem/ 查找数据文件
```

## 目录结构

```
screen_system/
├── main.cpp                  # 入口，命令行解析，CJK 字体初始化
├── CMakeLists.txt            # CMake 构建文件
├── resources.qrc             # Qt 资源文件
├── config/
│   └── default_config.json   # 8路摄像头配置（设置对话框保存到此文件）
├── models/                   # YOLO ONNX 模型文件 (待添加)
└── src/
    ├── Types.h               # BoundingBox, Detection, FrameData 等核心数据结构
    ├── Theme.h               # 全局主题常量 — 深色海军蓝配色/尺寸/间距
    │
    │  ── UI 层 ──
    ├── MainWindow.h/cpp      # 主窗口，三栏布局：Sidebar | Grid+BottomBar | AlertPanel
    ├── VideoWidget.h/cpp     # 视频窗格 — QPainter 自绘：视频帧 + 检测框 + LIVE灯 + 时间戳 + FPS
    ├── SidebarWidget.h/cpp   # 左侧导航栏 — 几何图标 (Dashboard/LiveView/Playback/Settings) + 启停按钮
    ├── BottomControlBar.h/cpp # 底部控制栏 — 网格切换 (1×1/2×2/2×4/3×3) + 快照 + 全屏 + 时钟
    ├── AlertPanel.h/cpp      # 右侧面板 — 设备在线列表 + 实时检测告警流
    ├── StatsPanel.h/cpp      # (旧版统计面板，已被 AlertPanel 替代，保留编译兼容)
    ├── SettingsDialog.h/cpp  # 设置对话框 — 8 tab 编辑每路摄像头参数，深色主题
    │
    │  ── 管线层 ──
    ├── CameraCapture.h/cpp   # 多源采集 (USB/RTSP/视频文件)，独立线程，自动重连
    ├── InferenceEngine.h/cpp # YOLOv8 ONNX 推理 (OpenCV DNN)，NMS，letterbox 预处理
    ├── SmoothingFilter.h/cpp # 贪心 IoU 追踪 + EMA 边框平滑
    └── ConfigManager.h/cpp   # JSON 配置读写 + 路径解析（线程安全）
```

## UI 操作说明

### 整体布局

```
左侧导航栏 (56px) │ 中央视频网格 + 底部控制栏 │ 右侧面板 (260px)
```

所有 UI 面板均采用 QPainter 自绘（零子控件），渲染开销极低。

### 主题配色

| 元素 | 色值 | 说明 |
|------|------|------|
| 背景 | `#0a0e1a` | 深空海军蓝 |
| 面板 | `rgba(12,20,40,220)` | 半透明侧栏 |
| 强调色 | `#00d4ff` | 青色荧光（选中/激活态） |
| LIVE 指示灯 | `#00e676` | 绿色脉冲 |
| 告警 | `#ff5252` | 红色高优先级 |
| 文字主色 | `#e0f0ff` | 冷白色 |

### 左侧导航栏

| 图标 | 功能 | 状态 |
|------|------|------|
| 网格 | Dashboard 仪表盘 | 待实现 |
| 眼睛 | Live View 实时监控 | **当前激活** |
| 播放 | Playback 回放 | 待实现 |
| 齿轮 | Settings 设置 | 打开设置对话框 |
| ▶/⏹ | 底部按钮 | 启动/停止所有摄像头 |

### 底部控制栏

| 按钮 | 功能 | 快捷键 |
|------|------|--------|
| **1×1** | 单摄像头放大（显示当前选中的摄像头） | `1` |
| **2×2** | 4路摄像头 2×2 布局 | `2` |
| **2×4** | 8路摄像头 2×4 布局（默认） | `3` |
| **3×3** | 8路摄像头 3×3 布局 | `4` |
| Snapshot | 截图 | — |
| Fullscreen | 全屏切换 | `F11` / `F` |
| Settings | 打开设置 | — |

右侧显示系统时钟 `yyyy-MM-dd HH:mm:ss`。

### 右侧面板

**设备列表**（上半部分）：
- 8 路摄像头状态行：绿点 = 在线，灰点 = 离线
- 显示摄像头名称和实时 FPS
- 右上角在线数量徽章

**实时告警**（下半部分）：
- 最近 20 条检测告警（置信度 ≥ 60% 自动推送）
- 每条显示：严重程度色条 + 类别色点 + 摄像头名 + 置信度 + 时间
- 点击告警可跳转到对应摄像头

### 视频窗格详细

每个视频窗格包含以下元素（全部 QPainter 自绘）：

| 位置 | 元素 | 说明 |
|------|------|------|
| 左上 | 摄像头名称 | 蓝色左边框 + 摄像头图标 + 名称文字 |
| 右上 | LIVE 指示灯 | 绿色圆点 + "LIVE" 文字，脉冲动画 |
| 左下 | 时间戳 | `HH:mm:ss` 格式，半透明背景 |
| 右下 | 状态信息 | `FPS | 推理耗时ms | 目标数`，青色文字 |
| 边框 | 选中状态 | 点击选中后显示青色荧光边框 |
| 无信号 | 扫描线效果 | "NO SIGNAL" + 横线扫描动画 |

检测框增强：
- 四角加粗短线（corner accent），视觉更清晰
- 标签徽章：圆角背景 + 白色文字
- 追踪 ID 显示在框内左下角

### 键盘快捷键

| 快捷键 | 功能 |
|--------|------|
| `Space` | 启动/停止所有摄像头 |
| `F11` / `F` | 全屏切换 |
| `Escape` | 退出全屏 |
| `1` | 切换到 1×1 模式 |
| `2` | 切换到 2×2 模式 |
| `3` | 切换到 2×4 模式 |
| `4` | 切换到 3×3 模式 |

### 设置对话框参数

每路摄像头（Camera 1 ~ Camera 8）各有一个 tab，可调整：

| 参数 | 说明 |
|------|------|
| 名称 | 摄像头显示名称 |
| 来源 | `0`/`1`... (USB设备号)、`/dev/video0`、`rtsp://...` |
| 启用 | 是否启用该路摄像头 |
| 模型路径 | YOLO ONNX 文件路径，支持绝对路径或相对于 models/ 的文件名 |
| 输入宽/高 | 模型输入分辨率（默认 640×640） |
| 置信度阈值 | 最低置信度，低于此值的检测框不显示 |
| NMS 阈值 | 非极大值抑制 IoU 阈值 |
| 推理间隔 | 两次推理之间的毫秒数（越小推理越频繁） |
| 平滑系数 EMA α | EMA 平滑强度，0=完全平滑，1=无平滑 |
| 最大丢失帧数 | 目标连续丢失多少帧后从追踪列表删除 |

## 已知修复

1. **UTF-8 BOM**：所有源文件已添加 BOM (`EF BB BF`)，Windows 编辑器可正确识别编码
2. **`CameraCapture.cpp` mutex 错误**：`isOpen() const` 中 `mMutex` 改为 `mutable QMutex`
3. **Qt 5.15 弃用警告**：`QAtomicInt::load()`/`store()` 改为 `loadRelaxed()`/`storeRelaxed()`
4. **中文字体**：
   - `main.cpp` 按候选列表自动选第一个可用的 CJK 字体（WenQuanYi Micro Hei → Noto Sans CJK SC → ...）
   - `VideoWidget.cpp` 改为继承 app 默认字体，不再硬编码字体名
   - fontconfig 添加了 CJK 回退：`/etc/fonts/conf.d/60-cjk-fallback.conf`
   - 中文渲染在 Qt 应用内部正常
5. **WSLg 标题栏乱码**：窗口标题、弹窗标题均改为纯英文（WSLg 合成器渲染标题栏，不受 Qt 字体设置控制）
6. **VideoWidget 滑动条**：置信度 QSlider 已移除，保留 QDoubleSpinBox 手动输入
7. **设置保存后摄像头名称不刷新**：`VideoWidget` 新增 `setTitle()` 方法，保存设置后标题立即更新
8. **UI 重构 v2**：移除菜单栏和背景图片，改用三栏布局（导航栏 + 网格 + 面板），全 QPainter 自绘面板

## 新建环境部署指南

以下命令在 WSL2 Ubuntu 24.04 中从头到尾执行一遍即可完成部署。

### 1. 安装依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake g++ \
    qtbase5-dev libqt5widgets5 libopencv-dev \
    fonts-wqy-microhei fonts-wqy-zenhei

# 检查 moc 是否来自 WSL 而非 Windows（anaconda 等会污染 PATH）
which moc          # 必须是 /usr/lib/qt5/bin/moc，如果是 /mnt/c/... 则执行下一行
export PATH=$(echo "$PATH" | tr ':' '\n' | grep -v '/mnt/c' | tr '\n' ':')
```

### 2. 同步源码到 WSL

```bash
# 源码必须放在 ~/ 下（ext4），不能在 /mnt/d/ 下编译
# 原因：跨文件系统 + Windows PATH 污染 + 特殊字符路径会导致 CMake AUTOMOC 失败
mkdir -p ~/screen_system
cp -r /mnt/d/\!code/screen_system/* ~/screen_system/
```

### 3. 编译

```bash
cd ~/screen_system
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 4. 运行

```bash
./ScreenInferenceSystem

# 如果中文显示为空格，重启 WSL 让字体生效：
# 在 Windows 终端执行: wsl --shutdown
# 然后重新进入 WSL 运行

# 命令行参数
./ScreenInferenceSystem -f                    # 全屏
./ScreenInferenceSystem -c /path/to/config.json
./ScreenInferenceSystem -n 4                  # 4路摄像头
```

## 故障排查

### WSL DNS 失效

```bash
# /etc/resolv.conf 中设置可用的 DNS
nameserver 223.5.5.5
nameserver 223.6.6.6

# /etc/wsl.conf 中禁止自动生成
[network]
generateResolvConf = false
```

## 模型导出指南

系统支持 YOLOv5 和 YOLOv8 的 **decoded 格式** ONNX 模型（输出已包含 bbox decode）。不支持 raw 特征图格式。

### YOLOv5 导出

```bash
# 标准导出（输出 [1, N, 5+numClasses]，每行 cx/cy/w/h/obj_conf/cls...）
python export.py --weights best.pt --include onnx --opset 12
```

### YOLOv8 导出

```bash
# 标准导出（输出 [1, 4+numClasses, 8400]，无 obj_conf）
yolo export model=best.pt format=onnx opset=12
```

### 注意

- 输入分辨率在配置文件中设置（`inputWidth` / `inputHeight`），须与训练/导出时一致
- 系统会自动判断 YOLOv5 / YOLOv8 格式
- 如果输出是 raw 特征图（多个 4D tensor），系统会打印警告并跳过推理

## 待完成

- [x] 添加 YOLO ONNX 模型文件到 `models/`
- [x] 测试摄像头接入（需要 USB 摄像头或 RTSP 流）
- [ ] 实现 Dashboard 仪表盘视图
- [ ] 实现 Playback 回放视图
- [ ] 实现 Snapshot 截图功能
- [ ] 删除旧版 StatsPanel（确认无引用后）

 1. 查看网口名称：
  ip link show | grep -E "^[0-9]"

  2. 给连接摄像头的网口配静态 IP（假设网口是 eth0）：
  sudo ip addr add 169.254.98.100/16 dev eth0
  sudo ip link set eth0 up

  3. 测试连通：
  ping 169.254.98.43 -c 3

## 多设备部署指南（推理端 + 展示端）

### 系统架构

```
┌─────────────────────────────────┐     ┌─────────────────────────────────┐
│  rk3576-A (推理端)               │     │  rk3578 / WSL (展示端)           │
│                                 │     │                                 │
│  摄像头 → dmg(RTSP采集)          │     │  ScreenInferenceSystem (Qt5)    │
│         → nn_server(RKNN推理)    │     │    ├─ MQTT 订阅推理数据          │
│         → nn_bridge(格式转换)    │─────│    ├─ RTSP 读取视频流            │
│                                 │MQTT │    └─ 自动发现通道               │
│  preview: rtsp://IP:5544/       │     │                                 │
│  mosquitto: 0.0.0.0:1883       │     │                                 │
└─────────────────────────────────┘     └─────────────────────────────────┘
```

### 网络要求

- 推理端和展示端在同一局域网
- 推理端 mosquitto 监听 0.0.0.0:1883（非 127.0.0.1）
- 展示端能访问推理端的 MQTT(1883) 和 RTSP(5544) 端口

---

### A. 推理端部署（rk3576）

#### A.1 从开发机一键推送

```bash
# 在开发机（Windows / WSL）执行
cd /mnt/d/\!code/screen_system
./rk3576/deploy.sh <设备IP>

# 例:
./rk3576/deploy.sh 192.168.77.145
```

这会将所有配置推送到设备 `/models/screen_system/rk3576/`。

#### A.2 首次部署：设备上运行安装脚本

```bash
ssh root@<设备IP>
bash /models/screen_system/rk3576/setup_device.sh
```

此脚本自动完成：
- 安装 mosquitto、python3、paho-mqtt、loguru
- 配置 mosquitto 监听 0.0.0.0:1883
- 创建 systemd 服务（nn_bridge.service）
- 设置开机自启动

#### A.3 手动配置 mosquitto（如果 setup_device.sh 不可用）

```bash
# 确保 mosquitto 监听所有网口
echo -e "listener 1883 0.0.0.0\nallow_anonymous true" | sudo tee /etc/mosquitto/conf.d/screen_system.conf
sudo systemctl restart mosquitto

# 验证
ss -tlnp | grep 1883
# 应显示: 0.0.0.0:1883
```

#### A.4 推送模型文件

```bash
scp your_model.rk3576.rknn root@<设备IP>:/models/screen_system/model/
```

#### A.5 启动/停止推理

```bash
ssh root@<设备IP>

# 启动（部署算法包 + 启动 nn_bridge）
cd /models/screen_system/rk3576
./start_inference.sh

# 只启动 bridge（不重新部署算法包）
./start_inference.sh --no-deploy

# 停止
./stop_inference.sh

# 查看日志
tail -f /models/screen_system/log/nn_bridge.log
```

#### A.6 需要修改的配置

**`rk3576/bridge_config.json`** — 根据实际环境修改：

```json
{
    "mqtt_local": {"host": "127.0.0.1", "port": 1883},
    "mqtt_remote": {"host": "127.0.0.1", "port": 1883},
    "client_id": "nn_bridge_rk3576",
    "geid": 200,
    "stats_interval": 30,
    "rate_limit": 0,
    "channel_timeout": 30,
    "preview": {
        "host": "<推理端IP>",
        "port": 5544
    },
    "discovery_topic": "inference/bridge/channels"
}
```

| 字段 | 说明 | 改什么 |
|------|------|--------|
| `mqtt_remote.host` | 远程 MQTT broker | 如果展示端有自己的 broker 就改为展示端 IP，否则保持 127.0.0.1 |
| `preview.host` | 展示端连接 RTSP 用的 IP | **必须改为推理端的局域网 IP**（展示端能访问的地址） |
| `preview.port` | dmg preview 端口 | 默认 5544，一般不用改 |
| `geid` | 算法组 ID | 对应 nn_server 的 geid，默认 200 |
| `channel_timeout` | 通道超时秒数 | 超过此时间无数据的通道自动下线 |

#### A.7 验证推理端

```bash
# 在推理端上检查 nn_bridge 是否在运行
pgrep -a nn_bridge

# 检查 MQTT 是否有发现消息
mosquitto_sub -t 'inference/bridge/channels' -v -C 1

# 检查推理输出
mosquitto_sub -t 'inference/camera/+/detections' -v -C 1

# 检查 RTSP 流
ffprobe rtsp://127.0.0.1:5544/preview/5 2>&1 | grep Stream
```

---

### B. 展示端部署

#### B.1 WSL2 开发/测试环境

```bash
# 安装依赖
sudo apt update
sudo apt install -y build-essential cmake g++ \
    qtbase5-dev libqt5widgets5 libopencv-dev \
    libpaho-mqttpp-dev libpaho-mqtt-dev \
    fonts-wqy-microhei mosquitto-clients

# 同步编译
cd ~/screen_system && ./sync.sh
# 或手动:
mkdir -p ~/screen_system && rsync -av --delete --exclude build/ --exclude .git/ /mnt/d/\!code/screen_system/ ~/screen_system/
cd ~/screen_system && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)

# 运行
cd ~/screen_system/build
./ScreenInferenceSystem
```

#### B.2 rk3578 设备部署

```bash
# 安装依赖
sudo apt update
sudo apt install -y build-essential cmake g++ \
    qtbase5-dev libqt5widgets5 libopencv-dev \
    libpaho-mqttpp-dev libpaho-mqtt-dev \
    fonts-wqy-microhei mosquitto-clients

# 拷贝源码到设备
scp -r /mnt/d/\!code/screen_system root@<展示端IP>:/models/screen_system

# 在设备上编译
ssh root@<展示端IP>
cd /models/screen_system
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 运行（需要 X11 环境）
export DISPLAY=:0
export QT_XCB_GL_INTEGRATION=none
./ScreenInferenceSystem
```

#### B.3 需要修改的配置

**`config/default_config.json`** — 根据实际环境修改：

```json
{
    "system": {
        "mode": "mqtt_subscribe"
    },
    "mqtt": {
        "broker": "tcp://<推理端IP>:1883",
        "client_id": "display_device",
        "username": "",
        "password": ""
    }
}
```

| 字段 | 说明 | 改什么 |
|------|------|--------|
| `mqtt.broker` | MQTT broker 地址 | **改为推理端的局域网 IP**，格式 `tcp://IP:1883` |
| `mqtt.client_id` | MQTT 客户端 ID | 多台展示端时需要不同的 ID |
| `system.mode` | 系统模式 | 保持 `mqtt_subscribe` |

cameras 数组不需要手动配置 — 系统会通过 MQTT discovery 自动发现通道。

#### B.4 验证展示端

```bash
# 测试网络连通
ping <推理端IP>

# 测试 MQTT 连接
mosquitto_sub -h <推理端IP> -t 'inference/bridge/channels' -v -C 1

# 测试 RTSP 流
ffprobe rtsp://<推理端IP>:5544/preview/<chid> 2>&1 | grep Stream

# 启动展示端后按 Space 启动摄像头
```

---

### C. 快速检查清单

#### 新设备部署前确认

- [ ] 推理端和展示端网络互通
- [ ] 推理端 mosquitto 监听 0.0.0.0:1883
- [ ] `bridge_config.json` 的 `preview.host` = 推理端 IP
- [ ] `default_config.json` 的 `mqtt.broker` = `tcp://推理端IP:1883`
- [ ] 模型文件已拷贝到推理端 `/models/screen_system/model/`
- [ ] 推理端 nn_bridge 已启动且 discovery 消息正常
- [ ] 展示端能收到 discovery 消息且 RTSP 流可达

#### 常见问题

| 现象 | 原因 | 解决 |
|------|------|------|
| MQTT Connection refused | mosquitto 只监听 127.0.0.1 | 加 `listener 1883 0.0.0.0` 配置 |
| 有框无视频 | RTSP 不通或 `preview.host` 错误 | 检查 `bridge_config.json` 的 preview.host |
| 有视频无框 | nn_bridge 未运行或 MQTT broker 地址错误 | 检查 nn_bridge 日志和 `default_config.json` 的 broker |
| 通道未发现 | nn_bridge 无 MQTT 数据或 discovery topic 不匹配 | `mosquitto_sub -t '/dposter/200/cmd' -v -C 1` 看源数据 |
| FPS 低 | preview 流本身帧率低，或网络带宽不足 | `ffprobe` 检查源流帧率 |
| 闪退 | OpenCV FFMPEG 异常 | 检查 RTSP 地址是否正确，设备网络是否稳定 |

---

### D. 多推理端扩展（2台 rk3576 + 1台展示端）

架构目标：rk3576-A 管 camera 0-3，rk3576-B 管 camera 4-7。

1. 两台推理端的 `bridge_config.json` 使用**不同的 geid** 或**不同的 client_id**
2. 两台推理端的 `mqtt_remote.host` 都指向**同一个 MQTT broker**（可以是展示端或第三方）
3. `preview.host` 分别填各自的 IP
4. 展示端的 `mqtt.broker` 指向那个统一的 broker
5. 通道自动发现会合并两台推理端的 discovery 消息

```
rk3576-A (192.168.77.145)          rk3576-B (192.168.77.146)
  preview.host=192.168.77.145        preview.host=192.168.77.146
  mqtt_remote.host=192.168.77.100    mqtt_remote.host=192.168.77.100
  client_id=nn_bridge_A              client_id=nn_bridge_B
          │                                    │
          └────── MQTT broker (192.168.77.100) ────── 展示端
```

注意：当前 discovery 机制每台 bridge 发布自己的 channels（retained），展示端会收到最后一条。如需合并多台，后续可改为分 topic 发布：`inference/bridge/A/channels`。