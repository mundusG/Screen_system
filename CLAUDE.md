# Screen Inference System (大屏推理显示系统)

基于 Qt5 + OpenCV DNN 的多路摄像头 YOLO 实时推理大屏显示系统。

## 架构

```
CameraCapture (独立线程) → InferenceEngine (独立线程, YOLO ONNX) → SmoothingFilter (主线程, EMA+IoU) → VideoWidget (Qt 显示)
```

8 路摄像头，2×4 网格布局，每路独立模型权重。

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
# ⚠ 在 WSL 中执行（不是 Windows）
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
├── main.cpp              # 入口，命令行解析，字体初始化
├── CMakeLists.txt         # CMake 构建文件
├── config/
│   └── default_config.json   # 8路摄像头配置（设置对话框保存到此文件）
├── models/                # YOLO ONNX 模型文件 (待添加)
└── src/
    ├── Types.h            # BoundingBox, Detection, FrameData 等数据结构
    ├── MainWindow.h/cpp   # 主窗口，2×4 网格，工具栏（启动/停止/设置）
    ├── VideoWidget.h/cpp  # Qt 控件，视频绘制 + 检测框叠加，底部状态面板
    ├── SettingsDialog.h/cpp  # 设置对话框，8 tab 编辑每路摄像头参数，保存到 JSON
    ├── CameraCapture.h/cpp   # 多源采集 (USB/RTSP/视频文件)，独立线程
    ├── InferenceEngine.h/cpp # YOLOv8 ONNX 推理，NMS，letterbox 预处理
    ├── SmoothingFilter.h/cpp # IoU 追踪 + EMA 平滑
    └── ConfigManager.h/cpp   # JSON 配置读写 + 路径解析
```

## UI 操作说明

- **▶ 启动全部** — 启动所有摄像头采集和推理
- **⏹ 停止全部** — 停止所有摄像头
- **⚙ 设置** — 打开设置对话框，编辑 8 路摄像头参数后点"保存"立即生效并写入 `default_config.json`

### 设置对话框参数说明

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
7. **设置保存后摄像头名称不刷新**：`VideoWidget` 新增 `setTitle()` 方法，`MainWindow::openSettings()` 回调在管线重建后同步调用，保存设置后左上角标题立即更新

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

## 待完成

- [ ] 添加 YOLO ONNX 模型文件到 `models/`
- [ ] 测试摄像头接入（需要 USB 摄像头或 RTSP 流）
