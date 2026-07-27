# RK3588 移植测试流程

## 总览

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ 阶段1: 编译   │ ──→ │ 阶段2: 板载   │ ──→ │ 阶段3: 联调   │
│ WSL 交叉编译  │     │ 单元+集成     │     │ 端到端        │
│ ~0.5h        │     │ ~1d          │     │ ~2d          │
└──────────────┘     └──────────────┘     └──────────────┘
```

---

## 阶段 1：编译验证

### 1.1 环境检查

```bash
# 确认交叉编译工具链已安装
aarch64-linux-gnu-g++ --version
# 预期: aarch64-linux-gnu-g++ (GCC) 9.x 或更高

# 确认 sysroot 存在
ls $HOME/rk3588_sysroot/usr/include/rknn/rknn_api.h
ls $HOME/rk3588_sysroot/usr/include/rga/RgaApi.h
# 预期: 两个文件都存在
```

### 1.2 源码编译

```bash
cd /mnt/d/\!code/screen_system/rk3588
mkdir -p build && cd build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../toolchain/rk3588_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

**检查点**:
- [ ] 编译无错误 (exit code = 0)
- [ ] 产物存在: `build/algo/libscreen_detect_rknn.so`
- [ ] 产物架构正确: `file build/algo/libscreen_detect_rknn.so` 应显示 `ELF 64-bit LSB shared object, ARM aarch64`

### 1.3 符号导出检查

```bash
# 确认标准 algo 接口已导出
aarch64-linux-gnu-nm -D build/algo/libscreen_detect_rknn.so | grep -E "algo_init|algo_process_shm_detect|algo_destroy"

# 预期三行输出:
#   T algo_init
#   T algo_process_shm_detect
#   T algo_destroy
```

---

## 阶段 2：板载单元测试 (RK3588 设备)

### 2.1 部署

```bash
cd /mnt/d/\!code/screen_system
./rk3588/deploy/deploy.sh <设备IP> root /models/screen_system <SSH端口>

# 例:
./rk3588/deploy/deploy.sh 192.168.1.200
```

**检查点**:
- [ ] .so 文件已推送到 `/models/screen_system/rk3588/algo/`
- [ ] 配置模板已推送到 `/models/screen_system/rk3588/config/`
- [ ] 模型文件已在 `/models/screen_system/model/` 下（如未推送，需手动 scp）

### 2.2 配置准备

在设备上编辑 `/models/screen_system/rk3588/config/bridge_config.json`:

```json
{
    "shm_list": [
        "/tmp/video_shm_CH01.dat"
    ],
    "channels": [
        {"chid": 1, "camera_id": 0, "name": "test_cam_1"}
    ],
    "model_type": 0,
    "conf_thres": 0.3,
    "iou_thres": 0.45,
    "class_names": ["smoking", "phone", "person"],
    "mqtt_host": "127.0.0.1",
    "mqtt_port": 1883,
    "client_id": "test_rk3588",
    "preview_host": "<设备IP>",
    "preview_port": 554,
    "rate_limit": 10
}
```

**关键验证**: `shm_list` 中的路径必须与 edge_server 实际创建的 SHM 路径一致。

### 2.3 SHM 帧读取测试 (无模型)

> 测试 SHM 读写、格式转换、RGA resize 路径。

修改代码临时跳过 NPU 推理，只验证前处理管道：

```cpp
// 在 algo_process_shm_detect 中，rknn_run 调用后添加日志
fprintf(stderr, "[test] frame_id=%llu fmt=%d %dx%d -> model %dx%d\n",
        (unsigned long long)frame->frame_id, frame->format,
        frame->width, frame->height, model_w, model_h);
```

**检查点**:
- [ ] SHM 打开成功 (open + mmap 不返回错误)
- [ ] frame_id 正确递增
- [ ] 帧格式正确 (0=BGR)
- [ ] RGA resize 成功 (或 CPU 回退路径生效)

### 2.4 NPU 推理测试

edge_server 加载 .so 后查看日志:

```bash
journalctl -u edge_server -f | grep -E "algo_init|algo_process|screen_detect"

# 预期看到:
#   [algo_init] model loaded, input: 1x640x640x3
#   [algo_init] outputs: 3 tensors (YOLOv5) / 1 tensor (YOLOv8)
#   [algo_init] MQTT discovery published
#   [algo_process_shm_detect] frame 1: 3 dets, 15.2ms
```

**检查点**:
- [ ] `algo_init` 成功 (rknn_init 无错误)
- [ ] 模型输入输出 query 正确
- [ ] `algo_process_shm_detect` 每帧被调用
- [ ] 推理耗时合理 (< 50ms/frame for YOLOv5s)
- [ ] `AlgoDetection` 数组正确填充 (x1,y1,x2,y2 归一化 [0,1])

### 2.5 后处理正确性

```bash
# 在设备上添加调试日志，输出前 3 个检测结果
# 预期: conf > conf_thres, class_id 在 [0, nc) 内
#       bbox 坐标在 [0, 1] 范围内

# 可以在 .so 的 algo_process_shm_detect 中添加:
for (int i = 0; i < *n_dets && i < 5; i++) {
    fprintf(stderr, "[det] cls=%d(%s) conf=%.3f bbox=[%.3f,%.3f,%.3f,%.3f]\n",
            dets[i].class_id, dets[i].class_name, dets[i].conf,
            dets[i].x1, dets[i].y1, dets[i].x2, dets[i].y2);
}
```

**检查点**:
- [ ] 检测数量 > 0 (模型已训练且有目标时)
- [ ] conf 均在 [conf_thres, 1.0] 区间
- [ ] class_id 有效 (在 class_names 范围内)
- [ ] class_name 正确 (从 config_json 载入)
- [ ] bbox 坐标归一化正确 (均在 [0,1])
- [ ] NMS 不产生重复框

### 2.6 MQTT 输出验证

```bash
# 设备上订阅 MQTT topic
mosquitto_sub -t 'inference/bridge/+/channels' -v -C 1
mosquitto_sub -t 'inference/bridge/+/class_manifest' -v -C 1
mosquitto_sub -t 'inference/camera/+/detections' -v -C 5
```

**通道发现** (`inference/bridge/{client_id}/channels`):
- [ ] 消息为 retained (重新订阅立即收到)
- [ ] `channels` 数组长度 = 配置的通道数
- [ ] 每个通道含 `camera_id`, `chid`, `name`, `preview_url`, `inference_topic`
- [ ] `preview_url` 格式: `rtsp://<preview_host>:<preview_port>/preview/{chid}`

**类别清单** (`inference/bridge/{client_id}/class_manifest`):
- [ ] 消息为 retained
- [ ] `channels` 数组每个元素含 `camera_id`, `model_type`, `classes`
- [ ] `classes` 中 id 和 name 与配置的 `class_names` 一致
- [ ] 支持多通道各自不同类别 (如果多通道配置了不同 class_names)

**检测结果** (`inference/camera/{camera_id}/detections`):
- [ ] QoS 0, NOT retained
- [ ] `camera_id` 正确
- [ ] `timestamp` 为有效毫秒时间戳
- [ ] `frame_index` 递增
- [ ] `normalized` = true
- [ ] `detections` 数组非空时: `class_id`, `class_name`, `confidence` 正确
- [ ] `bbox` 使用 center 格式: `{cx, cy, w, h}` (不是 corner 格式!)
- [ ] bbox 值在 [0, 1] 范围

**LWT 状态** (`inference/bridge/status`):
- [ ] edge_server 启动时: `{"status":"online",...}`
- [ ] edge_server 停止时: `{"status":"offline",...}`

### 2.7 MQTT 异常路径

```bash
# 1. MQTT broker 不可用 — .so 应优雅降级，不影响推理
sudo systemctl stop mosquitto
# 观察: 推理继续 (AlgoDetection 仍填充)，MQTT publish 失败但无 crash
sudo systemctl start mosquitto
# 观察: 自动重连，恢复发布

# 2. MQTT broker 中途重启
sudo systemctl restart mosquitto
# 观察: .so 内重连逻辑生效，retained 消息重新发布
```

**检查点**:
- [ ] broker 不可用时 algo_process_shm_detect 不阻塞、不 crash
- [ ] broker 恢复后自动重连
- [ ] retained 消息 (channels/class_manifest) 重新发布

---

## 阶段 3：端到端联调

### 3.1 展示端配置

在展示端创建/修改 `config/default_config.json`:

```json
{
    "system": {
        "mode": "mqtt_subscribe"
    },
    "mqtt_sources": [
        {
            "id": "main",
            "enabled": true,
            "broker": "tcp://<RK3588_IP>:1883",
            "client_id": "display_test",
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

### 3.2 单通道基础联调

```
RK3588 (1路摄像头) → .so 推理 → MQTT → 展示端 (1个 VideoWidget)
                                  RTSP ← 展示端拉流
```

**操作步骤**:
1. RK3588 启动 edge_server (加载 .so)
2. 展示端启动 `./ScreenInferenceSystem`
3. 按 `Space` 启动所有摄像头

**检查点**:
- [ ] 左侧面板显示通道名称 (从 discovery 获取)
- [ ] 通道状态指示灯为绿色 (在线)
- [ ] 视频画面正常显示 (RTSP 拉流成功)
- [ ] 检测框位置与目标匹配 (无偏移)
- [ ] 检测框颜色与 class_id 一致 (从 class_manifest 自动分配)
- [ ] 标签显示 `class_name confidence%` 格式
- [ ] FPS 和推理耗时正常显示
- [ ] LIVE 指示灯正常脉冲动画

### 3.3 class_manifest 验证

**操作步骤**:
1. 修改 `bridge_config.json` 的 `class_names` 为 `["smoking", "phone", "person", "helmet", "vest"]`
2. 重启 edge_server 加载新配置
3. 展示端重新订阅 class_manifest (retained 消息自动送达)

**检查点**:
- [ ] 5 个类别在检测时各显示不同颜色
- [ ] 颜色稳定 (同一 class_id 始终同色)
- [ ] 类别名称为中文或英文均可正确显示
- [ ] 类别名超过 8 字符时标签不溢出

### 3.4 多通道测试

```
RK3588 (N路摄像头) → .so 推理 → MQTT → 展示端 (N个 VideoWidget)
                                     RTSP ← 展示端拉流 (N路)
```

| 通道数 | 测试重点 |
|--------|---------|
| 2路 | 多 topic MQTT 路由正确 |
| 4路 | 2×2 网格正常 |
| 8路 | 2×4 / 3×3 网格正常，性能不降级 |

**检查点 (每通道)**:
- [ ] discovery 发现所有通道
- [ ] 每个通道对应正确的 `camera_id` 和 `inference_topic`
- [ ] RTSP 地址从 `preview_url` 正确解析
- [ ] 不同通道可显示不同类别 (如果各通道 class_names 不同)
- [ ] 网格切换 (1×1/2×2/2×4/3×3) 均正常

### 3.5 网格切换测试

| 操作 | 预期 |
|------|------|
| 按 `1` | 切换到 1×1，只显示选中摄像头 |
| 按 `2` | 切换到 2×2，显示 4 路 |
| 按 `3` | 切换到 2×4，显示 8 路 (默认) |
| 按 `4` | 切换到 3×3，显示 8 路 + 1 空位 |
| 鼠标点击某个 VideoWidget | 选中高亮，1×1 模式自动切换 |

**检查点**:
- [ ] 切换流畅无闪烁
- [ ] 视频比例保持 16:9
- [ ] 检测框在缩放后位置正确

### 3.6 告警面板测试

**检查点**:
- [ ] 设备列表显示在线状态 (绿点/灰点)
- [ ] 在线数量徽章计数正确
- [ ] 检测到目标时实时告警流推送
- [ ] 告警显示: 色条 + 类别色点 + 摄像头名 + 置信度 + 时间
- [ ] 点击告警可跳转到对应摄像头

### 3.7 压力测试

```bash
# 连续运行 2 小时
# 监控: 内存泄漏 / 帧率下降 / MQTT 断连
```

**检查点**:
- [ ] 内存稳定 (RSS 增长 < 50MB/h)
- [ ] 推理帧率稳定 (波动 < 20%)
- [ ] MQTT 无消息丢失
- [ ] 无 crash 或异常日志
- [ ] SHM 帧无泄漏 (frame_id 连续)

### 3.8 长时间运行

```bash
# 连续运行 24 小时
journalctl -u edge_server | grep -c "error\|crash\|OOM"
```

**检查点**:
- [ ] 无崩溃
- [ ] 无 OOM
- [ ] 无文件描述符泄漏 (lsof 数量稳定)

---

## 阶段 4：边界情况

### 4.1 推理端

| 场景 | 预期行为 |
|------|---------|
| 摄像头断线 | SHM 无新帧，.so 返回 n_dets=0，不 crash |
| 摄像头恢复 | 自动检测新帧，恢复推理 |
| 模型文件不存在 | algo_init 返回 -1，日志输出错误 |
| 模型与 config 不匹配 (nc 不对) | algo_init 应检测并报错 |
| config_json 格式错误 | algo_init 返回 -1，使用默认值 |
| config_json 缺少可选字段 | 使用 fallback 默认值 |
| SHM 不存在 | 跳过该 SHM，处理下一个 |
| SHM 权限不足 | 日志报错，跳过 |
| 帧格式为 NV12 | .so 内转换或跳过 |
| class_names 为空 | 使用 "unknown_0", "unknown_1"... |

### 4.2 展示端

| 场景 | 预期行为 |
|------|---------|
| MQTT broker 不可达 | 显示 "NO SIGNAL"，自动重连 |
| discovery 消息延迟到达 | 通道在收到后自动创建 |
| class_manifest 未收到 | 使用默认绿色/红色配色 |
| RTSP 流不可达 | 显示 "NO SIGNAL" + 扫描线动画 |
| RTSP 流卡顿/断流 | CameraThread 自动重连 |
| 多 MQTT source 同 camera_id | 第二个 source 的检测被过滤 |
| 切换到后台/最小化 | 不崩溃，恢复后正常 |
| 分辨率变更 | 布局自适应 |

### 4.3 模型类型切换

| model_type | 差异 | 验证 |
|-----------|------|------|
| 0 (YOLOv5) | 3 输出 tensor，anchor-based decode | class_names 不同、anchor 不同时正确 |
| 1 (YOLOv8) | 1 输出 tensor，anchor-free decode | stride 映射正确 (80×80/40×40/20×20) |
| 2 (YOLOv11) | 同 YOLOv8 | 同 YOLOv8 验证项 |

**切换测试**:
1. 修改 `bridge_config.json` 的 `model_type`
2. 更新模型文件路径
3. 重启 edge_server
4. 验证检测结果格式正确

---

## 阶段 5：回归检查

### 5.1 RK3576 旧链路不受影响

```bash
# 在 RK3576 设备上照常启动 nn_bridge
# 展示端使用 mqtt_sources 指向 RK3576 的 broker
# 验证: 旧设备仍正常工作
```

**检查点**:
- [ ] RK3576 Python bridge 正常运行
- [ ] 展示端同时连接 RK3576 和 RK3588 两个 source
- [ ] 两套 source 的 camera_id 范围不重叠
- [ ] 同一展示端可混合显示来自不同推理端的通道

### 5.2 WSL 本地推理模式

```bash
cd ~/screen_system/build && ./ScreenInferenceSystem
# 使用 local_inference 模式，USB 摄像头 + ONNX 模型
```

**检查点**:
- [ ] `system.mode` = `local_inference` 时正常运行
- [ ] 不尝试连接 MQTT
- [ ] ServiceLauncher 不报错 (无 nn_bridge)

---

## 快速冒烟测试 (每次部署后执行)

```bash
# === 推理端 ===
# 1. 检查进程
pgrep -a edge_server

# 2. 检查 .so 加载
journalctl -u edge_server --since "1 min ago" | grep "algo_init"

# 3. 检查 MQTT 输出 (取 3 条)
timeout 15 mosquitto_sub -t 'inference/camera/0/detections' -v -C 3

# 4. 检查 retained 消息
mosquitto_sub -t 'inference/bridge/+/channels' -v -C 1
mosquitto_sub -t 'inference/bridge/+/class_manifest' -v -C 1

# === 展示端 ===
# 5. 启动程序
cd /models/screen_system/build && timeout 30 ./ScreenInferenceSystem

# 6. 检查日志无 crash
grep -c "SIGSEGV\|SIGABRT\|OOM" /var/log/syslog
```
