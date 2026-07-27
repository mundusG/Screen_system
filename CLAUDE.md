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
│   ├── default_config.json          # (gitignored) 运行时配置 — 从 .example 复制后修改
│   └── default_config.json.example  # 配置模板，含 localhost 占位符，可直接入库
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

## rk3576 推理端部署

### 系统架构

```
┌──────────────────────────────────────────────┐
│  rk3576 (推理端)                              │
│                                              │
│  摄像头 → dmg(RTSP采集) → nn_server(RKNN推理)  │
│                         → nn_bridge(格式转换)  │
│                                              │
│  mosquitto (127.0.0.1:1883，内部管道)         │
│  preview: rtsp://<设备IP>:5544/              │
└──────────────┬───────────────────────────────┘
               │ MQTT (推理结果 + discovery)
               ↓
┌──────────────────────────────────────────────┐
│  展示端 (rk3578 / WSL)                        │
│  ScreenInferenceSystem (Qt5)                 │
│    ├─ MQTT 订阅: 推理数据 + 通道发现           │
│    └─ RTSP 拉流: rtsp://<推理端IP>:5544/...   │
└──────────────────────────────────────────────┘
```

**数据流**：推理端内部 mosquitto 做 dmg→nn_server→nn_bridge 的管道；nn_bridge 把最终结果通过 `mqtt_remote` 发布到展示端可见的 broker。展示端 Qt 程序连同一个 broker 订阅推理结果和通道发现消息。

---

### A. 推理端部署（rk3576）

> **端口说明**：以下 `<设备IP>` 根据实际情况替换。如果是公网端口转发，则 `<设备IP>` 填公网 IP，`<SSH端口>` 填转发端口（如 `61837`）；局域网设备则端口填 `22` 或不传。

#### A.1 从开发机一键推送配置

```bash
cd /mnt/d/\!code/screen_system
./rk3576/deploy.sh <设备IP> root /models/screen_system <SSH端口>

# 例——局域网:
./rk3576/deploy.sh 192.168.77.145

# 例——公网端口转发:
./rk3576/deploy.sh 42.193.140.103 root /models/screen_system 61837
```

将配置推送到设备 `/models/screen_system/rk3576/`。

#### A.2 首次部署：运行安装脚本

```bash
ssh -p <SSH端口> root@<设备IP> "bash /models/screen_system/rk3576/setup_device.sh"
```

自动完成：安装 mosquitto/python3/paho-mqtt/loguru → 配置 mosquitto 监听 0.0.0.0:1883 → 创建 nn_bridge systemd 服务 → 开机自启。

#### A.3 配置 mosquitto

在设备上执行：

```bash
echo -e "listener 1883 0.0.0.0\nallow_anonymous true" | sudo tee /etc/mosquitto/conf.d/screen_system.conf
sudo systemctl restart mosquitto

# 验证
ss -tlnp | grep 1883
# 应显示: 0.0.0.0:1883
```

#### A.4 推送模型文件

```bash
scp -P <SSH端口> your_model.rknn root@<设备IP>:/models/screen_system/model/
```

#### A.5 启动 / 停止

```bash
ssh -p <SSH端口> root@<设备IP>

cd /models/screen_system/rk3576
./start_inference.sh              # 部署算法包 + 启动 nn_bridge
./start_inference.sh --no-deploy  # 只启动 bridge（不重部署算法包）
./stop_inference.sh               # 停止
tail -f /models/screen_system/log/nn_bridge.log  # 查看日志
```

#### A.6 需要修改的配置：`rk3576/bridge_config.json`

```json
{
    "mqtt_local":  { "host": "127.0.0.1", "port": 1883 },
    "mqtt_remote": { "host": "127.0.0.1", "port": 1883 },
    "client_id": "nn_bridge_rk3576",
    "geid": 200,
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
| `preview.host` | 展示端拉 RTSP 流用的 IP | **必改**：填推理端局域网 IP |
| `mqtt_remote.host` | 发布推理结果的目标 broker | 单机部署保持 `127.0.0.1`；多机方案见下文 D 节 |
| `mqtt_local.host` | 本机内部管道 broker | 保持 `127.0.0.1` |
| `geid` | 算法组 ID | 对应 nn_server 的 geid，默认 200 |
| `discovery_topic` | 通道发现 topic 前缀 | 保持 `inference/bridge`，nn_bridge 会拼成 `/channels` |

#### A.7 验证

```bash
pgrep -a nn_bridge                                          # 进程是否在跑
mosquitto_sub -t 'inference/bridge/channels' -v -C 1       # discovery 消息
mosquitto_sub -t 'inference/camera/+/detections' -v -C 1   # 推理结果
ffprobe rtsp://127.0.0.1:5544/preview/5 2>&1 | grep Stream  # RTSP 流
```

---

### B. 展示端部署（rk3578 / 其他 Linux 设备）

展示端需要完整的 C++ 项目源码来编译 Qt 程序。推理端只需 `rk3576/` 目录（Python 脚本），两者独立。

#### B.1 安装依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake g++ \
    qtbase5-dev libqt5widgets5 libopencv-dev \
    libpaho-mqttpp-dev libpaho-mqtt-dev \
    fonts-wqy-microhei mosquitto-clients
```

#### B.2 拷贝源码 & 编译

```bash
# 从开发机推送源码（排除非必要文件）
cd /mnt/d/\!code/screen_system && \
  tar -czf - \
      --exclude=build \
      --exclude=.git \
      --exclude=.vscode \
      --exclude=.claude \
      --exclude=rk3576 \
      --exclude='*.md' \
      --exclude='*.jsonl' \
      --exclude='*.pyc' \
      --exclude=__pycache__ \
      --exclude=.DS_Store \
      --exclude=.gitignore \
      . | \
  ssh -p <SSH端口> root@<展示端IP> "mkdir -p /models/screen_system && cd /models/screen_system && tar -xzf -"

# 在展示设备上编译
ssh -p <SSH端口> root@<展示端IP> << 'ENDSSH'
cd /models/screen_system
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
ENDSSH
```

#### B.3 创建并修改配置文件

`config/default_config.json` 不在 git 中（gitignored），首次使用需从模板复制：

```bash
cd /models/screen_system
cp config/default_config.json.example config/default_config.json
```

**必须修改的字段**：

```json
{
    "mqtt_sources": [
        {
            "id": "main",
            "enabled": true,
            "broker": "tcp://<推理端IP>:1883",
            "client_id": "display_main",
            "discovery_topic": "inference/bridge/+/channels",
            "camera_id_min": 0,
            "camera_id_max": 7
        }
    ],
    "services": {
        "check_mosquitto": false
    }
}
```

| 字段 | 值 | 说明 |
|------|-----|------|
| `mqtt_sources[0].broker` | `tcp://<推理端IP>:1883` | **必改**：指向推理端 mosquitto |
| `services.check_mosquitto` | `false` | 展示端不检查本地 mosquitto |

> **⚠️ 修改配置后必须重新编译**（`cd build && cmake .. && make -j$(nproc)`），否则 build 目录里的旧配置不会更新。

cameras 数组不需要手动配置 — 系统通过 MQTT discovery 自动发现通道。

#### B.4 运行

```bash
# rk3578 等 ARM Linux 设备（需要 X11）
export DISPLAY=:0
export QT_XCB_GL_INTEGRATION=none
cd /models/screen_system/build
./ScreenInferenceSystem          # 纯 MQTT 订阅模式
./ScreenInferenceSystem -i       # image_stream 模式（MQTT + 图片帧）
./ScreenInferenceSystem -f       # 全屏
```

#### B.5 验证

```bash
ping <推理端IP>                                                     # 网络
mosquitto_sub -h <推理端IP> -t 'inference/bridge/channels' -v -C 1  # MQTT
ffprobe rtsp://<推理端IP>:5544/preview/<chid> 2>&1 | grep Stream    # RTSP
```

启动展示端后按 `Space` 启动所有摄像头。

---

### C. 快速检查清单

- [ ] 推理端和展示端网络互通
- [ ] 推理端 mosquitto 监听 `0.0.0.0:1883`（`ss -tlnp | grep 1883`）
- [ ] `bridge_config.json` 的 `preview.host` = 推理端 IP
- [ ] `default_config.json` 的 `mqtt_sources[0].broker` = `tcp://推理端IP:1883`
- [ ] 模型文件已拷贝到推理端 `/models/screen_system/model/`
- [ ] 推理端已启动（nn_bridge 或 edge_server），discovery 消息正常
- [ ] 展示端能收到 discovery 消息且 RTSP 流可达

#### 常见问题

| 现象 | 原因 | 解决 |
|------|------|------|
| MQTT Connection refused | mosquitto 只监听 127.0.0.1 | 加 `listener 1883 0.0.0.0` 配置 |
| 有框无视频 | RTSP 不通或 `preview.host` 错误 | 检查 `bridge_config.json` 的 preview.host |
| 有视频无框 | 推理端未运行或 broker 地址错误 | 检查推理端日志和 `default_config.json` 的 broker |
| 通道未发现 | 推理端无数据或 discovery topic 不匹配 | `mosquitto_sub -t 'inference/bridge/+/channels' -v -C 1` |
| 改了配置不生效 | build 目录里的旧配置没更新 | 重新 cmake && make |
| 闪退 | OpenCV FFMPEG 异常 | 检查 RTSP 地址、设备网络稳定性 |

---

### D. 多推理端扩展（2 台 rk3576 + 1 台展示端）

推荐架构：**broker 放在展示端**（方案 B），数据流最直接。

```
rk3576-A (192.168.77.145)          rk3576-B (192.168.77.146)
  管 camera 0-3                      管 camera 4-7
  本地 mosquitto 照常运行             本地 mosquitto 照常运行
  mqtt_remote.host = 192.168.77.100  mqtt_remote.host = 192.168.77.100
  preview.host = 192.168.77.145      preview.host = 192.168.77.146
  client_id = nn_bridge_A            client_id = nn_bridge_B
          │                                    │
          └────── MQTT broker ──────────────────┘
                 展示端 (192.168.77.100)
                 mosquitto 监听 0.0.0.0:1883
```

**展示端改动**：

```bash
# 安装并配置 mosquitto 监听外网
sudo apt install -y mosquitto
echo -e "listener 1883 0.0.0.0\nallow_anonymous true" | sudo tee /etc/mosquitto/conf.d/screen_system.conf
sudo systemctl restart mosquitto
```

```json
// default_config.json
"mqtt_sources": [{ "broker": "tcp://127.0.0.1:1883", ... }],
"services": {
    "check_mosquitto": true
}
```

**推理端改动**（两台各自改 `bridge_config.json`）：

```json
  // rk3576-A 的 bridge_config.json
  "mqtt_remote": { "host": "192.168.77.100" },   // 展示端 IP
  "preview":     { "host": "192.168.77.145" }     // 自己的 IP

  // rk3576-B 的 bridge_config.json
  "mqtt_remote": { "host": "192.168.77.100" },   // 同一个展示端 IP
  "preview":     { "host": "192.168.77.146" }     // 自己的 IP
```

每台推理端的本地 mosquitto（`mqtt_local`）照常 `127.0.0.1`，不受影响——它只管本机 dmg→nn_server→nn_bridge 的内部管道。

---

## rk3588 推理端部署 (.so 算法包)

RK3588 推理端使用 Go edge_server 管理 RTSP 采集 → SHM → dlopen .so 的标准管线。不需要 nn_bridge.py —— .so 内部直接发布 MQTT 到展示端格式。

### 系统架构

```
┌──────────────────────────────────────────────┐
│  RK3588 (推理端)                              │
│                                              │
│  摄像头 → edge_server(RTSP→SHM→dlopen .so)    │
│         └→ algo_process_shm_detect()          │
│              ├→ return AlgoDetection[]        │
│              └→ MQTT publish (内部直连)        │
│                                              │
│  mosquitto (127.0.0.1:1883)                  │
│  preview: rtsp://<设备IP>:554/preview/{chid}  │
└──────────────┬───────────────────────────────┘
               │ MQTT
               │   inference/camera/{id}/detections
               │   inference/bridge/{client_id}/channels (retain)
               │   inference/bridge/{client_id}/class_manifest (retain)
               │   inference/bridge/status (LWT, retain)
               ↓
┌──────────────────────────────────────────────┐
│  展示端 (WSL / 其他 Linux)                     │
│  ScreenInferenceSystem (Qt5)                 │
│    ├─ MQTT 订阅: 检测数据 + 通道发现 + 类别清单 │
│    └─ RTSP 拉流: rtsp://<RK3588>:554/preview/ │
└──────────────────────────────────────────────┘
```

### MQTT 消息格式

**检测结果** → `inference/camera/{camera_id}/detections` (QoS 0):

```json
{
    "camera_id": 0,
    "timestamp": 1722001234567,
    "frame_index": 1523,
    "normalized": true,
    "inference_time_ms": 12.5,
    "detections": [
        {
            "class_id": 0,
            "class_name": "smoking",
            "confidence": 0.92,
            "bbox": {"cx": 0.37, "cy": 0.21, "w": 0.12, "h": 0.34}
        }
    ]
}
```

**通道发现** → `inference/bridge/{client_id}/channels` (QoS 1, retain):

```json
{
    "channels": [
        {
            "camera_id": 0,
            "chid": 1,
            "name": "camera_1",
            "preview_url": "rtsp://192.168.1.200:554/preview/1",
            "inference_topic": "inference/camera/0/detections"
        }
    ]
}
```

**类别清单** → `inference/bridge/{client_id}/class_manifest` (QoS 1, retain):

```json
{
    "client_id": "rk3588_screen_01",
    "channels": [{
        "camera_id": 0,
        "model_type": "yolov5",
        "classes": [
            {"id": 0, "name": "smoking"},
            {"id": 1, "name": "phone"}
        ]
    }]
}
```

### A. 编译 .so

```bash
# 在开发机上交叉编译（需要 aarch64-linux-gnu 工具链 + RK3588 sysroot）
cd /mnt/d/\!code/screen_system/rk3588
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain/rk3588_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
# 产物: algo/libscreen_detect_rknn.so
```

### B. 部署

```bash
# 一键部署（编译 + 推送 + 远端初始化）
cd /mnt/d/\!code/screen_system
./rk3588/deploy/deploy.sh <设备IP> root /models/screen_system <SSH端口>

# 例——局域网:
./rk3588/deploy/deploy.sh 192.168.1.200

# 例——公网端口转发:
./rk3588/deploy/deploy.sh 42.193.140.103 root /models/screen_system 61837
```

### C. 配置 edge_server

edge_server 传给 algo_init 的 config_json 应包含 `bridge_config.json` 的内容：

| 字段 | 说明 |
|------|------|
| `shm_list[]` | edge_server 创建的 SHM 路径列表 |
| `channels[]` | `{chid, camera_id, name}` 通道映射 |
| `model_type` | `0`=YOLOv5, `1`=YOLOv8, `2`=YOLOv11 |
| `class_names[]` | 类别名列表（动态，不限于 person） |
| `conf_thres` / `iou_thres` | 检测阈值 |
| `mqtt_host` / `mqtt_port` | MQTT broker 地址 |
| `client_id` | 用于 discovery/class_manifest/LWT topic |
| `preview_host` / `preview_port` | RTSP 预览地址 |
| `rate_limit` | MQTT 发布帧率上限 (fps) |

### D. 验证

```bash
ssh root@<RK3588>
# 检查 .so 是否被 edge_server 加载
journalctl -u edge_server -f | grep "algo_init"

# MQTT 验证
mosquitto_sub -t 'inference/bridge/+/channels' -v -C 1
mosquitto_sub -t 'inference/bridge/+/class_manifest' -v -C 1
mosquitto_sub -t 'inference/camera/+/detections' -v -C 5
```

### E. 展示端适配要点

- `class_manifest` topic 由 InferenceSubscriber 自动订阅（从 discovery topic 派生：`channels` → `class_manifest`）
- 解码后通过 `classManifestReceived` 信号发送到 MainWindow
- MainWindow 按 class_id → HSV 色相自动分配颜色到 VideoWidget
- ConfigManager 不再解析 `services.nn_bridge` 字段
- ServiceLauncher 不再管理 Python bridge 进程

---

## 模型导出指南

系统支持 YOLOv5 和 YOLOv8 的 **decoded 格式** ONNX 模型（输出已包含 bbox decode）。不支持 raw 特征图格式。

### YOLOv5 导出

```bash
python export.py --weights best.pt --include onnx --opset 12
# 输出 [1, N, 5+numClasses]，每行 cx/cy/w/h/obj_conf/cls...
```

### YOLOv8 导出

```bash
yolo export model=best.pt format=onnx opset=12
# 输出 [1, 4+numClasses, 8400]，无 obj_conf
```

- 输入分辨率在配置文件中设置（`inputWidth` / `inputHeight`），须与训练/导出时一致

---

## 待完成

- [x] 添加 YOLO ONNX 模型文件到 `models/`
- [x] 测试摄像头接入（需要 USB 摄像头或 RTSP 流）
- [ ] 实现 Dashboard 仪表盘视图
- [ ] 实现 Playback 回放视图
- [ ] 实现 Snapshot 截图功能
- [ ] 删除旧版 StatsPanel（确认无引用后）

---

## 附录：WSL2 开发环境

WSL2 作为展示端开发/调试环境。和普通展示端的区别是：源码放在 Windows 下编辑，WSL 下用 `sync.sh` 增量同步编译，省去 scp 步骤。

### 1. 安装依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake g++ \
    qtbase5-dev libqt5widgets5 libopencv-dev \
    libpaho-mqttpp-dev libpaho-mqtt-dev \
    fonts-wqy-microhei fonts-wqy-zenhei \
    mosquitto-clients rsync

which moc  # 必须是 /usr/lib/qt5/bin/moc，不能是 Windows 的
```

### 2. 创建 sync.sh

```bash
cat > ~/screen_system/sync.sh << 'EOF'
#!/bin/bash
set -e
SRC=/mnt/d/\!code/screen_system
DST=~/screen_system

rsync -av --delete \
    --exclude build/ \
    --exclude .git/ \
    --exclude .vscode/ \
    "$SRC"/ "$DST"/

cd "$DST"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
echo "Done. Run: cd ~/screen_system/build && ./ScreenInferenceSystem"
EOF
chmod +x ~/screen_system/sync.sh
```

### 3. 首次同步 & 配置

```bash
cd ~/screen_system
cp config/default_config.json.example config/default_config.json
# 编辑 mqtt.broker 指向推理端 IP，services 参考 B.3 节
./sync.sh
```

### 4. 运行

```bash
cd ~/screen_system/build
./ScreenInferenceSystem          # 纯 MQTT 订阅
./ScreenInferenceSystem -i       # MQTT + 图片帧
./ScreenInferenceSystem -f       # 全屏
```

### WSL DNS 失效

```bash
# /etc/resolv.conf
nameserver 223.5.5.5
nameserver 223.6.6.6

# /etc/wsl.conf
[network]
generateResolvConf = false
```