# Screen Inference System - TODO

## ✅ 已完成（2026-06-11）

### MQTT 分布式架构基础
- [x] MQTTClient.h/cpp - Paho MQTT 封装，支持异步连接
- [x] InferenceSubscriber.h/cpp - 订阅 MQTT 主题并解析 JSON 推理结果
- [x] InferencePublisher.h/cpp - 发布 JSON 格式的推理结果到 MQTT
- [x] 配置文件扩展 - 添加 MQTT broker、mode、topics 配置
- [x] ConfigManager 扩展 - 解析 MQTT 配置和摄像头模式
- [x] Types.h 更新 - CameraConfig 添加 mode 和 mqttTopic 字段
- [x] MainWindow 多模式支持 - local_inference 和 mqtt_subscribe 模式
- [x] VideoWidget 检测框显示修复 - 支持在 NO SIGNAL 状态叠加检测框
- [x] 异步连接优化 - 避免 MQTT 连接阻塞主线程导致界面卡死
- [x] 测试验证 - MQTT 消息接收、JSON 解析、检测框显示、Alert 告警

### 测试通过的功能
- MQTT broker 连接（mosquitto）
- 主题订阅（inference/camera/0/detections）
- JSON 消息解码（camera_id, detections, bbox）
- 检测框叠加显示（在 NO SIGNAL 画面上）
- Alert 面板告警记录

## 🔧 待完成

### 高优先级

#### 1. 展示端添加 RTSP 视频流显示
**当前状态：** mqtt_subscribe 模式下只显示 NO SIGNAL + 检测框叠加

**需要实现：**
- 修改 `MainWindow::setupCameraPipeline()` 中 mqtt_subscribe 分支
- 创建 CameraThread 用于 RTSP 视频显示（不连接 InferenceEngine）
- 保持 MQTT 订阅接收检测结果
- 实现：RTSP 实时视频 + MQTT 检测框叠加

**文件修改：**
- `src/MainWindow.cpp` - setupCameraPipeline() mqtt_subscribe 分支

**注意事项：**
- 摄像头连接必须异步，避免阻塞主线程
- 使用 `requestStart()` 而不是 `open()` 同步调用
- 确保 RTSP 连接失败时不影响 MQTT 接收

#### 2. 推理端实现（瑞芯微设备）
**需要创建：**
- `src/RKNNInferenceEngine.h/cpp` - RKNN API 推理引擎

**核心功能：**
```cpp
class RKNNInferenceEngine : public QObject {
    // 初始化
    rknn_init(&ctx, model_data, model_size, 0, NULL);
    
    // 推理
    rknn_inputs_set(ctx, 1, inputs);
    rknn_run(ctx, nullptr);
    rknn_outputs_get(ctx, num_outputs, outputs, nullptr);
    
    // 解析输出 → Detection[]
    emit inferenceFinished(result);
};
```

**配置文件：**
```json
{
  "mode": "mqtt_publish",
  "mqtt": {
    "broker": "tcp://192.168.1.100:1883",
    "client_id": "inference_device_1"
  },
  "cameras": [
    {
      "cameraId": 0,
      "mode": "mqtt_publish",
      "source": "rtsp://...",
      "modelPath": "model.rknn"
    }
  ]
}
```

**依赖库：**
- librockchip_mpp - 硬件视频解码
- librga - 硬件图像处理
- librknn_api - RKNN 推理运行时

#### 3. 端到端联调测试
**测试环境：**
- 推理设备 1：camera 0-3 → MQTT 发布
- 推理设备 2：camera 4-7 → MQTT 发布
- 展示设备：订阅 8 路 → RTSP 视频 + 检测框叠加

**测试项：**
- [ ] 多路并发（8 路同时推理和显示）
- [ ] 网络延迟监控（MQTT 消息延迟）
- [ ] 断线重连（MQTT 自动重连）
- [ ] 消息乱序处理（时间戳校验）
- [ ] 资源占用（CPU、内存、网络带宽）
- [ ] 长时间稳定性（24 小时运行）

### 中优先级

#### 4. 性能优化
- [ ] JSON 序列化优化（考虑使用 MessagePack 或 Protocol Buffers）
- [ ] MQTT 消息压缩（降低网络带宽）
- [ ] QoS 策略调整（QoS 0 vs QoS 1）
- [ ] 检测框平滑算法优化（EMA 参数调优）
- [ ] 内存池复用（减少频繁分配）

#### 5. 功能增强
- [ ] 时间戳同步和延迟监控
- [ ] 推理端性能统计（推理耗时、FPS、丢帧率）
- [ ] 展示端性能统计（接收延迟、渲染 FPS）
- [ ] 告警阈值配置（不同类别不同置信度）
- [ ] 历史告警查询和导出
- [ ] 检测结果录像保存

#### 6. 健壮性提升
- [ ] MQTT 连接重试策略
- [ ] 消息队列溢出处理
- [ ] 异常检测框过滤（超出画面范围）
- [ ] JSON 解析错误容错
- [ ] 网络抖动缓冲

### 低优先级

#### 7. 部署和运维
- [ ] Docker 容器化部署
- [ ] systemd 服务配置
- [ ] 日志轮转和归档
- [ ] 配置热重载
- [ ] 远程配置更新
- [ ] Prometheus 指标导出

#### 8. 文档完善
- [ ] RKNN 模型导出教程
- [ ] 瑞芯微设备部署文档
- [ ] MQTT broker 配置指南
- [ ] 性能调优指南
- [ ] 故障排查手册

## 📊 MQTT 消息格式（已定义）

**Topic:** `inference/camera/{camera_id}/detections`

**Payload:**
```json
{
  "camera_id": 0,
  "timestamp": 1735123456789,
  "frame_index": 12345,
  "detections": [
    {
      "class_id": 0,
      "confidence": 0.87,
      "bbox": {
        "cx": 320.5,
        "cy": 240.3,
        "w": 150.0,
        "h": 200.0
      }
    }
  ],
  "inference_time_ms": 15.2
}
```

## 🐛 已知问题

1. **NO SIGNAL 状态检测框缩放** - 当前假设 640x480 分辨率，实际分辨率可能不同
2. **MQTT 断线无提示** - 断线后界面无明显提示
3. **检测框闪烁** - 快速更新时可能出现闪烁（需要双缓冲优化）

## 🔬 测试命令

### 单次测试
```bash
mosquitto_pub -h localhost -t "inference/camera/0/detections" -m '{"camera_id":0,"timestamp":1735123456789,"frame_index":100,"detections":[{"class_id":0,"confidence":0.85,"bbox":{"cx":320,"cy":240,"w":100,"h":150}},{"class_id":1,"confidence":0.92,"bbox":{"cx":500,"cy":300,"w":80,"h":120}}],"inference_time_ms":15.2}'
```

### 持续测试（模拟实时推理）
```bash
for i in {1..100}; do
  X=$((RANDOM % 500 + 50))
  Y=$((RANDOM % 300 + 50))
  mosquitto_pub -h localhost -t "inference/camera/0/detections" -m "{\"camera_id\":0,\"timestamp\":$(date +%s)000,\"frame_index\":$i,\"detections\":[{\"class_id\":0,\"confidence\":0.85,\"bbox\":{\"cx\":$X,\"cy\":$Y,\"w\":100,\"h\":150}}],\"inference_time_ms\":15.2}"
  sleep 0.5
done
```

### 监听所有消息
```bash
mosquitto_sub -h localhost -t "inference/camera/#" -v
```

## 📝 开发笔记

### 关键修复
1. **MQTT 同步连接阻塞** - 改为完全异步连接（去掉 `tok->wait()`）
2. **订阅时机错误** - 必须等连接成功后再订阅（通过 `onMqttConnected` 回调）
3. **检测框不显示** - paintEvent 只在有视频帧时绘制，修改为无论有无视频都绘制
4. **SmoothingFilter 缺失** - mqtt_subscribe 模式也需要创建 SmoothingFilter 处理检测结果

### 架构设计原则
- 模块化：推理端和展示端完全解耦
- 资源高效：只传输 JSON 检测结果，不传输视频流
- 灵活配置：通过配置文件切换模式，无需修改代码
- 异步非阻塞：所有网络操作异步执行，保证 UI 流畅

## 🔧 常见问题和解决方案

### 问题 1: 界面黑屏卡死
**现象：** 程序启动后窗口显示但完全黑屏，无法交互

**原因：** 
- MQTT 连接使用 `tok->wait()` 同步阻塞主线程
- 摄像头 `camera->open()` 在 `initialize()` 中同步调用阻塞
- `startAll()` 在主线程直接调用

**解决方案：**
```cpp
// 1. MQTT 异步连接（MQTTClient.cpp）
auto tok = mClient->connect(connOpts);
// 删除 tok->wait();  不要同步等待

// 2. 订阅延迟到连接成功回调
void InferenceSubscriber::onMqttConnected() {
    for (const QString& topic : mSubscribedTopics) {
        mMqttClient->subscribe(topic);
    }
}

// 3. 使用 QTimer 异步调用
QTimer::singleShot(0, this, [this, topics]() {
    mInferenceSubscriber->connectAndSubscribe(...);
});

// 4. main.cpp 中注释掉自动启动
// mainWindow.startAll();  // 不自动启动
```

### 问题 2: MQTT 连接成功但订阅失败
**现象：** 日志显示 "Cannot subscribe, not connected"

**原因：** 
- `connectToBroker()` 异步返回，但立即调用 `subscribe()`
- 此时连接尚未建立完成

**解决方案：**
```cpp
// InferenceSubscriber.cpp
bool connectAndSubscribe(...) {
    mSubscribedTopics = topics;  // 先保存 topics
    mMqttClient->connectToBroker(...);
    // 不立即订阅，等待 onMqttConnected 回调
    return true;
}

void onMqttConnected() {
    // 连接成功后才订阅
    for (const QString& topic : mSubscribedTopics) {
        mMqttClient->subscribe(topic);
    }
}
```

### 问题 3: MQTT 消息接收但检测框不显示
**现象：** 
- Alert 面板有告警记录
- 控制台输出 "VideoWidget[0]: Updated detections, count: 2"
- 但界面上看不到检测框

**原因：** 
- VideoWidget 的 paintEvent 只在 `if (!mCurrentFrame.empty())` 分支绘制检测框
- NO SIGNAL 状态下 mCurrentFrame 为空，跳过了检测框绘制

**解决方案：**
```cpp
// VideoWidget.cpp paintEvent
// 将检测框绘制移到外层，无论有无视频帧都执行
{
    QMutexLocker locker(&mFrameMutex);
    QMutexLocker detLocker(&mDetectionMutex);

    float scaleX, scaleY;
    if (!mCurrentFrame.empty()) {
        // 有视频：使用实际尺寸
        scaleX = static_cast<float>(drawW) / mCurrentFrame.cols;
        scaleY = static_cast<float>(drawH) / mCurrentFrame.rows;
    } else {
        // NO SIGNAL：假设 640x480
        scaleX = static_cast<float>(drawW) / 640.0f;
        scaleY = static_cast<float>(drawH) / 480.0f;
    }

    // 绘制检测框（无论有无视频）
    for (const auto& det : mDetections) {
        if (det.filtered) continue;
        // ... 绘制代码
    }
}
```

### 问题 4: SmoothingFilter 不存在导致检测框不显示
**现象：** 控制台输出 "No SmoothingFilter for camera 0"

**原因：** 
- mqtt_subscribe 模式下，`initialize()` 跳过了 `setupCameraPipeline()`
- 没有创建 SmoothingFilter
- `onInferenceFinished` 无法处理检测结果

**解决方案：**
```cpp
// MainWindow.cpp setupCameraPipeline
else if (cameraMode == "mqtt_subscribe") {
    // mqtt_subscribe 模式也需要创建 SmoothingFilter
    auto* smoother = new SmoothingFilter(cameraId, this);
    smoother->setAlpha(config.smoothingAlpha);
    smoother->setMaxLostFrames(config.trackMaxLost);
    
    connect(smoother, &SmoothingFilter::displayResultReady,
            this, &MainWindow::onDisplayResultReady);
    
    mSmoothingFilters[cameraId] = smoother;
    
    // 不创建 CameraThread 和 InferenceEngine
    return true;
}
```

### 问题 5: 配置文件 JSON 解析错误
**现象：** "JSON parse error: garbage at the end of the document"

**原因：** 
- 手动编辑配置文件时格式错误
- 缺少逗号、括号不匹配、字段不完整

**解决方案：**
```bash
# 1. 使用 python json.tool 验证
python3 -m json.tool config.json > /dev/null && echo "格式正确" || echo "格式错误"

# 2. 使用 cat > file << 'EOF' 一次性写入完整内容
cat > config.json << 'EOF'
{
  "system": { "mode": "mqtt_subscribe" },
  "mqtt": { "broker": "tcp://localhost:1883" }
}
EOF

# 3. 确保所有必需字段完整
# 每个 camera 对象必须包含：
# - cameraId, name, source, enabled
# - confidenceThreshold, inputWidth, inputHeight
# - smoothingAlpha, trackMaxLost, inferenceIntervalMs
# - classColors
```

### 问题 6: WSL 环境 Qt 界面不显示
**现象：** 程序运行但看不到窗口

**原因：** 
- WSL X 服务配置问题
- DISPLAY 环境变量未设置

**解决方案：**
```bash
# 1. 检查 DISPLAY
echo $DISPLAY  # 应该显示 :0 或类似

# 2. 测试 X 服务
xclock  # 能显示说明 X 正常

# 3. 如果不行，重启 WSL
# 在 Windows PowerShell 执行：
wsl --shutdown
wsl

# 4. 或使用 VcXsrv（更稳定）
# 下载安装 VcXsrv
# 启动 XLaunch，勾选 "Disable access control"
# WSL 中设置：
export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0.0
export LIBGL_ALWAYS_INDIRECT=1
```


### 问题 7: BoundingBox 字段名不匹配
**现象：** 编译错误 "'const struct BoundingBox' has no member named 'cx'"

**原因：** 
- BoundingBox 使用 `x, y, width, height`
- JSON 使用 `cx, cy, w, h`

**解决方案：**
```cpp
// InferencePublisher.cpp
bboxObj["cx"] = static_cast<double>(det.bbox.x);       // 不是 cx
bboxObj["cy"] = static_cast<double>(det.bbox.y);       // 不是 cy
bboxObj["w"] = static_cast<double>(det.bbox.width);    // 不是 w
bboxObj["h"] = static_cast<double>(det.bbox.height);   // 不是 h

// InferenceSubscriber.cpp
float x = static_cast<float>(bboxObj["cx"].toDouble());
float y = static_cast<float>(bboxObj["cy"].toDouble());
float width = static_cast<float>(bboxObj["w"].toDouble());
float height = static_cast<float>(bboxObj["h"].toDouble());
det.bbox = BoundingBox(x, y, width, height);
```

### 问题 8: Paho MQTT 库找不到
**现象：** CMake 错误 "Could NOT find PahoMqttCpp"

**解决方案：**
```bash
# Ubuntu/WSL
sudo apt install libpaho-mqtt-dev libpaho-mqttpp-dev

# 或手动编译
wget https://github.com/eclipse/paho.mqtt.c/archive/refs/tags/v1.3.13.tar.gz
tar xzf v1.3.13.tar.gz
cd paho.mqtt.c-1.3.13
mkdir build && cd build
cmake .. -DPAHO_WITH_SSL=OFF
make -j$(nproc)
sudo make install
sudo ldconfig
```


## 🛠️ 调试技巧

### 1. 查看 MQTT 消息流
```bash
# 订阅所有主题
mosquitto_sub -h localhost -t "#" -v

# 只看推理主题
mosquitto_sub -h localhost -t "inference/camera/#" -v
```

### 2. 过滤关键日志
```bash
# 只看关键流程
./ScreenInferenceSystem 2>&1 | grep -E "(MQTT|InferenceSubscriber|onInferenceFinished|VideoWidget.*Updated)"
```

### 3. 检查配置加载
```bash
# 查看配置文件被识别的部分
./ScreenInferenceSystem 2>&1 | grep -E "(ConfigManager|System mode|Camera.*mode)"
```

### 4. 验证数据流
发送消息后应该看到完整的调用链：
```
InferenceSubscriber: Received message from "inference/camera/0/detections" size: 256
InferenceSubscriber: Decoded result for camera 0 detections: 2
MainWindow::onInferenceFinished: camera 0 detections: 2
MainWindow::onDisplayResultReady: camera 0 detections: 2
VideoWidget[0]: Updated detections, count: 2
MainWindow::onDisplayResultReady: Updated VideoWidget 0
```

如果中断在某一步，说明该步骤有问题。

