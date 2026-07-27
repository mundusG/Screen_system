/**
 * screen_detect_wrapper.cpp — 屏幕检测系统算法包 (RK3588 NPU)
 *
 * 标准算法接口 (符合 efence_C++ bridge.h 规范):
 *   int  algo_init(const char* model_path, const char* config_json)
 *   int  algo_process_shm_detect(const char* output_img, AlgoDetection* dets,
 *                                  int max_dets, int* n_dets)
 *   void algo_destroy()
 *
 * 数据流:
 *   SHM 读帧 (BGR/NV12) → BGR→RGB → RGA resize+letterbox (CPU 回退)
 *     → RKNN NPU 推理 → YOLO 后处理 (v5/v8/v11) → 填充 AlgoDetection[]
 *     → MQTT 发布 (inference/camera/{id}/detections, 展示端格式)
 *
 * 编译 (RK3588 aarch64):
 *   g++ -fPIC -shared -o libscreen_detect_rknn.so screen_detect_wrapper.cpp
 *       common/json_parser.cpp common/postprocess.cpp
 *       -std=c++14 -O2
 *       -I. -I/usr/include -I/usr/include/rga -I/usr/include/rknn
 *       -L/usr/lib -L/usr/lib/aarch64-linux-gnu
 *       -lrknnrt -lrga -lmosquitto -lpthread -ldl -ljpeg -lcurl -lstdc++
 *
 * 部署: libscreen_detect_rknn.so + *.rknn + bridge_config.json
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <errno.h>
#include <ctime>
#include <sys/time.h>
#include <map>
#include <string>

#include <rga.h>
#include <RgaUtils.h>
#include "im2d.h"
#include <rknn_api.h>
#include <jpeglib.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <pthread.h>
#include <stdarg.h>

#include "bridge.h"
#include "json_parser.h"
#include "postprocess.h"

// MQTT 手动声明 (动态链接 libmosquitto)
#define MOSQ_ERR_SUCCESS 0
struct mosquitto;
extern "C" {
    int mosquitto_lib_init(void);
    struct mosquitto* mosquitto_new(const char* id, bool clean_session, void* obj);
    void mosquitto_destroy(struct mosquitto* mosq);
    int mosquitto_username_pw_set(struct mosquitto* mosq, const char* username, const char* password);
    int mosquitto_connect(struct mosquitto* mosq, const char* host, int port, int keepalive);
    int mosquitto_publish(struct mosquitto* mosq, int* mid, const char* topic, int payloadlen,
                          const void* payload, int qos, bool retain);
    int mosquitto_disconnect(struct mosquitto* mosq);
    const char* mosquitto_strerror(int mosq_errno);
    int mosquitto_reconnect(struct mosquitto* mosq);
    int mosquitto_loop_read(struct mosquitto* mosq, int max_packets);
    int mosquitto_loop_write(struct mosquitto* mosq, int max_packets);
    int mosquitto_loop_misc(struct mosquitto* mosq);
    int mosquitto_socket(struct mosquitto* mosq);
}

// ============================================================================
// 常量
// ============================================================================
#define MAX_SHARED_WIDTH  1920
#define MAX_SHARED_HEIGHT 1080
#define MAX_SHARED_CHANNELS 3
#define ALIGN_16(x) (((x) + 15) & ~15)
#define MAX_DETECTIONS 128
#define MAX_CHANNELS    16
#define MAX_CLASSES     80

// ============================================================================
// 共享内存帧结构 (必须与 edge_server 侧一致)
// ============================================================================
typedef struct {
    pthread_mutex_t lock;
    int            width;
    int            height;
    int            channels;
    int            format;       // 0=BGR, 1=RGB, 2=RGBA, 3=NV12
    uint64_t       frame_id;
    uint64_t       timestamp_ns;
    unsigned char  data[MAX_SHARED_WIDTH * MAX_SHARED_HEIGHT * MAX_SHARED_CHANNELS];
} SharedFrame;

// ============================================================================
// 通道配置
// ============================================================================
struct ChannelCfg {
    int    chid;
    int    camera_id;
    char   name[64];
};

// ============================================================================
// 全局状态
// ============================================================================
static rknn_context g_ctx = 0;
static void*        g_model_data = nullptr;
static int          g_initialized = 0;

static int g_model_w = 640, g_model_h = 640;
static int g_n_outputs = 1;
static int g_model_type = 0;          // 0=v5, 1=v8, 2=v11
static int g_num_classes = 80;

// SHM 通道列表
static std::vector<std::string> g_shm_list;
static std::vector<ChannelCfg>  g_channels;

// 推理参数
static float g_conf_thres = 0.5f;
static float g_iou_thres  = 0.45f;

// MQTT 配置
static char g_mqtt_host[128] = "127.0.0.1";
static int  g_mqtt_port = 1883;
static char g_mqtt_topic_prefix[128] = "inference/camera";
static char g_client_id[64] = "rk3588_screen";
static char g_discovery_base[128] = "inference/bridge";
static char g_preview_host[128] = "127.0.0.1";
static int  g_preview_port = 5544;
static int  g_rate_limit = 10;

// MQTT 持久连接
static mosquitto* g_mosq = nullptr;
static int        g_mosquitto_inited = 0;

// 每通道 frame_id 追踪 (检测新帧)
static std::map<std::string, uint64_t> g_last_frame_ids;
// 每通道帧计数
static std::map<int, int> g_frame_counts;
// 每通道上次 publish 时间 (rate limit)
static std::map<int, double> g_last_publish_time;

// 类别名列表 (从 config 读入)
static char g_class_names[MAX_CLASSES][DETECT_CLASS_NAME_LEN];

// YOLOv5 配置
static YOLOScale g_yolo5_scales[3];

// ============================================================================
// 日志
// ============================================================================
static void log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

// ============================================================================
// 通道号提取: "/tmp/video_shm_CH01.dat" → "CH01"
// ============================================================================
static void extract_ch_no(const char* shm_path, char* out, size_t out_size) {
    out[0] = '\0';
    if (!shm_path) return;
    const char* prefix = "video_shm_";
    const char* start = strstr(shm_path, prefix);
    if (!start) {
        const char* ch_tag = strstr(shm_path, "_CH");
        if (ch_tag) start = ch_tag + 1;
        else return;
    } else {
        start += strlen(prefix);
    }
    const char* end = strstr(start, ".dat");
    if (!end) end = start + strlen(start);
    size_t len = (size_t)(end - start);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
}

static int extract_chid(const char* ch_no) {
    if (!ch_no) return -1;
    const char* p = ch_no;
    while (*p && !(*p >= '0' && *p <= '9')) p++;
    return atoi(p);
}

// ============================================================================
// MQTT 连接管理
// ============================================================================
static int mqtt_ensure_connected() {
    if (!g_mosquitto_inited) {
        mosquitto_lib_init();
        g_mosquitto_inited = 1;
    }
    if (g_mosq) {
        // 检查是否仍然连接
        int sock = mosquitto_socket(g_mosq);
        if (sock >= 0) return 0;  // 已连接
        // 尝试重连
        int ret = mosquitto_reconnect(g_mosq);
        if (ret == MOSQ_ERR_SUCCESS) return 0;
        // 重连失败, 销毁重建
        mosquitto_destroy(g_mosq);
        g_mosq = nullptr;
    }

    char client_id[96];
    snprintf(client_id, sizeof(client_id), "%s_%d", g_client_id, (int)time(nullptr));
    g_mosq = mosquitto_new(client_id, true, nullptr);
    if (!g_mosq) {
        log_info("[screen-detect] mqtt: mosquitto_new failed\n");
        return -1;
    }
    int ret = mosquitto_connect(g_mosq, g_mqtt_host, g_mqtt_port, 5);
    if (ret != MOSQ_ERR_SUCCESS) {
        log_info("[screen-detect] mqtt: connect failed %s:%d (%s)\n",
                g_mqtt_host, g_mqtt_port, mosquitto_strerror(ret));
        mosquitto_destroy(g_mosq);
        g_mosq = nullptr;
        return -1;
    }
    return 0;
}

// ============================================================================
// MQTT 发布 JSON (保持连接, publish 后不 disconnect)
// ============================================================================
static void mqtt_publish(const char* topic, const char* payload, int qos, bool retain) {
    if (mqtt_ensure_connected() != 0) return;
    int ret = mosquitto_publish(g_mosq, nullptr, topic, (int)strlen(payload), payload, qos, retain);
    if (ret != MOSQ_ERR_SUCCESS) {
        log_info("[screen-detect] mqtt: publish failed topic=%s (%s)\n",
                topic, mosquitto_strerror(ret));
    }
    // 驱动 I/O (非阻塞)
    mosquitto_loop_write(g_mosq, 1);
    mosquitto_loop_misc(g_mosq);
}

// ============================================================================
// 发布通道发现消息 (retained)
// ============================================================================
static void publish_discovery() {
    char topic[256];
    snprintf(topic, sizeof(topic), "%s/%s/channels", g_discovery_base, g_client_id);

    char payload[8192];
    int off = snprintf(payload, sizeof(payload), "{\"channels\":[");
    bool first = true;
    for (const auto& ch : g_channels) {
        if (!first) off += snprintf(payload + off, sizeof(payload) - off, ",");
        first = false;
        off += snprintf(payload + off, sizeof(payload) - off,
            "{\"camera_id\":%d,\"chid\":%d,\"name\":\"%s\","
            "\"preview_url\":\"rtsp://%s:%d/preview/%d\","
            "\"inference_topic\":\"%s/%d/detections\"}",
            ch.camera_id, ch.chid, ch.name,
            g_preview_host, g_preview_port, ch.chid,
            g_mqtt_topic_prefix, ch.camera_id);
    }
    off += snprintf(payload + off, sizeof(payload) - off, "]}");

    mqtt_publish(topic, payload, 1, true);
    log_info("[screen-detect] discovery: published %zu channels\n", g_channels.size());
}

// ============================================================================
// 发布类别清单 (retained)
// ============================================================================
static void publish_class_manifest() {
    char topic[256];
    snprintf(topic, sizeof(topic), "%s/%s/class_manifest", g_discovery_base, g_client_id);

    char payload[8192];
    int off = snprintf(payload, sizeof(payload),
        "{\"client_id\":\"%s\",\"channels\":[", g_client_id);
    bool first_ch = true;
    for (const auto& ch : g_channels) {
        if (!first_ch) off += snprintf(payload + off, sizeof(payload) - off, ",");
        first_ch = false;
        off += snprintf(payload + off, sizeof(payload) - off,
            "{\"camera_id\":%d,\"model_type\":\"yolov%d\",\"classes\":[",
            ch.camera_id, g_model_type == 0 ? 5 : (g_model_type == 1 ? 8 : 11));
        for (int c = 0; c < g_num_classes; c++) {
            off += snprintf(payload + off, sizeof(payload) - off,
                "%s{\"id\":%d,\"name\":\"%s\"}",
                c > 0 ? "," : "", c, g_class_names[c]);
        }
        off += snprintf(payload + off, sizeof(payload) - off, "]}");
    }
    off += snprintf(payload + off, sizeof(payload) - off, "]}");

    mqtt_publish(topic, payload, 1, true);
    log_info("[screen-detect] class_manifest: published %d classes\n", g_num_classes);
}

// ============================================================================
// 发布 LWT 状态
// ============================================================================
static void publish_status(const char* status) {
    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"status\":\"%s\",\"client_id\":\"%s\"}", status, g_client_id);
    mqtt_publish("inference/bridge/status", payload, 1, true);
}

// ============================================================================
// 发布单帧检测结果 (展示端格式)
// ============================================================================
static void publish_detections(int camera_id, const AlgoDetection* dets, int n_dets,
                                float infer_ms, int frame_count) {
    // rate limit
    double now_sec = (double)time(nullptr);
    int effective_rate = g_rate_limit > 0 ? g_rate_limit : 10;
    double min_interval = 1.0 / effective_rate;
    auto it = g_last_publish_time.find(camera_id);
    if (it != g_last_publish_time.end() && (now_sec - it->second) < min_interval) {
        return;
    }
    g_last_publish_time[camera_id] = now_sec;

    char topic[256];
    snprintf(topic, sizeof(topic), "%s/%d/detections", g_mqtt_topic_prefix, camera_id);

    int64_t ts_ms = 0;
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        ts_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }

    char payload[16384];
    int off = snprintf(payload, sizeof(payload),
        "{\"camera_id\":%d,\"timestamp\":%lld,\"frame_index\":%d,"
        "\"normalized\":true,\"inference_time_ms\":%.1f,\"detections\":[",
        camera_id, (long long)ts_ms, frame_count, infer_ms);

    for (int i = 0; i < n_dets; i++) {
        const AlgoDetection& d = dets[i];
        if (d.conf <= 0.0f) continue;
        // corner [x1,y1,x2,y2] → center {cx,cy,w,h}
        float cx = (d.x1 + d.x2) / 2.0f;
        float cy = (d.y1 + d.y2) / 2.0f;
        float w  = d.x2 - d.x1;
        float h  = d.y2 - d.y1;
        if (off > (int)sizeof(payload) - 256) break;  // 安全截断

        off += snprintf(payload + off, sizeof(payload) - off,
            "%s{\"class_id\":%d,\"class_name\":\"%s\",\"confidence\":%.2f,"
            "\"bbox\":{\"cx\":%.4f,\"cy\":%.4f,\"w\":%.4f,\"h\":%.4f}}",
            i > 0 ? "," : "",
            d.class_id,
            d.class_name[0] ? d.class_name : (d.class_id < g_num_classes ? g_class_names[d.class_id] : "unknown"),
            d.conf, cx, cy, w, h);
    }
    off += snprintf(payload + off, sizeof(payload) - off, "]}");

    mqtt_publish(topic, payload, 0, false);
}

// ============================================================================
// 解析 channels 数组: [{"chid":1,"camera_id":0,"name":"cam1"},...]
// ============================================================================
static void parse_channels(const char* json) {
    g_channels.clear();
    const char* key = "\"channels\"";
    const char* p = strstr(json, key);
    if (!p) return;
    p = strchr(p + strlen(key), '[');
    if (!p) return;
    p++;

    while (p && *p) {
        while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',')) p++;
        if (*p == '\0' || *p == ']') break;
        if (*p != '{') { p++; continue; }

        const char* end = strchr(p, '}');
        if (!end) break;
        size_t blen = (size_t)(end - p) + 1;
        char* block = (char*)malloc(blen + 1);
        memcpy(block, p, blen);
        block[blen] = '\0';

        ChannelCfg ch;
        memset(&ch, 0, sizeof(ch));
        ch.chid      = json_get_int(block, "chid", 0);
        ch.camera_id = json_get_int(block, "camera_id", 0);
        char* name   = json_get_string(block, "name");
        if (name) {
            strncpy(ch.name, name, sizeof(ch.name) - 1);
            free(name);
        } else {
            snprintf(ch.name, sizeof(ch.name), "camera_%d", ch.chid);
        }
        g_channels.push_back(ch);
        free(block);
        p = end + 1;
    }
}

// ============================================================================
// 解析 YOLOv5 anchor 配置
// ============================================================================
static void init_yolo5_scales(int model_w, int model_h) {
    // COCO 默认 anchors (YOLOv5s)
    // stride=8 (80×80): small objects
    g_yolo5_scales[0].stride = 8;
    g_yolo5_scales[0].grid_w = model_w / 8;
    g_yolo5_scales[0].grid_h = model_h / 8;
    g_yolo5_scales[0].n_anchors = 3;
    g_yolo5_scales[0].anchors[0] = 10;  g_yolo5_scales[0].anchors[1] = 13;
    g_yolo5_scales[0].anchors[2] = 16;  g_yolo5_scales[0].anchors[3] = 30;
    g_yolo5_scales[0].anchors[4] = 33;  g_yolo5_scales[0].anchors[5] = 23;

    // stride=16 (40×40): medium objects
    g_yolo5_scales[1].stride = 16;
    g_yolo5_scales[1].grid_w = model_w / 16;
    g_yolo5_scales[1].grid_h = model_h / 16;
    g_yolo5_scales[1].n_anchors = 3;
    g_yolo5_scales[1].anchors[0] = 30;  g_yolo5_scales[1].anchors[1] = 61;
    g_yolo5_scales[1].anchors[2] = 62;  g_yolo5_scales[1].anchors[3] = 45;
    g_yolo5_scales[1].anchors[4] = 59;  g_yolo5_scales[1].anchors[5] = 119;

    // stride=32 (20×20): large objects
    g_yolo5_scales[2].stride = 32;
    g_yolo5_scales[2].grid_w = model_w / 32;
    g_yolo5_scales[2].grid_h = model_h / 32;
    g_yolo5_scales[2].n_anchors = 3;
    g_yolo5_scales[2].anchors[0] = 116; g_yolo5_scales[2].anchors[1] = 90;
    g_yolo5_scales[2].anchors[2] = 156; g_yolo5_scales[2].anchors[3] = 198;
    g_yolo5_scales[2].anchors[4] = 373; g_yolo5_scales[2].anchors[5] = 326;
}

// ============================================================================
// algo_init — 加载模型 + 解析配置 + 初始化 MQTT
// ============================================================================
extern "C" int algo_init(const char* model_path, const char* config_json) {
    log_info("[screen-detect] init: model=%s\n", model_path ? model_path : "(null)");

    if (g_initialized) {
        log_info("[screen-detect] init: already initialized\n");
        return 0;
    }
    if (!model_path || model_path[0] == '\0') {
        log_info("[screen-detect] init: model_path is empty\n");
        return -1;
    }

    // ── 1. 加载 RKNN 模型 ──
    FILE* fp = fopen(model_path, "rb");
    if (!fp) {
        log_info("[screen-detect] init: cannot open model %s\n", model_path);
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    size_t model_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    log_info("[screen-detect] init: model size %.2f MB\n", model_size / 1048576.0);

    g_model_data = malloc(model_size);
    if (!g_model_data) { fclose(fp); return -1; }
    size_t read_n = fread(g_model_data, 1, model_size, fp);
    fclose(fp);
    if (read_n != model_size) {
        log_info("[screen-detect] init: read incomplete (%zu/%zu)\n", read_n, model_size);
        free(g_model_data); g_model_data = nullptr;
        return -1;
    }

    int ret = rknn_init(&g_ctx, g_model_data, model_size, 0, nullptr);
    log_info("[screen-detect] init: rknn_init=%d\n", ret);
    if (ret < 0) {
        free(g_model_data); g_model_data = nullptr;
        return -1;
    }
    rknn_set_core_mask(g_ctx, RKNN_NPU_CORE_0_1_2);

    // 获取输入/输出属性
    rknn_input_output_num io_num;
    rknn_query(g_ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    g_n_outputs = io_num.n_output;
    log_info("[screen-detect] init: n_inputs=%d n_outputs=%d\n", io_num.n_input, g_n_outputs);

    rknn_tensor_attr input_attr;
    memset(&input_attr, 0, sizeof(input_attr));
    input_attr.index = 0;
    rknn_query(g_ctx, RKNN_QUERY_INPUT_ATTR, &input_attr, sizeof(input_attr));
    g_model_w = input_attr.dims[2];
    g_model_h = input_attr.dims[1];
    log_info("[screen-detect] init: model input %dx%d\n", g_model_w, g_model_h);

    // 打印输出 tensor 属性 (调试)
    for (int i = 0; i < g_n_outputs; i++) {
        rknn_tensor_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.index = i;
        rknn_query(g_ctx, RKNN_QUERY_OUTPUT_ATTR, &attr, sizeof(attr));
        log_info("[screen-detect] init: output[%d] dims=[%d,%d,%d,%d] type=%d size=%d\n",
                i, attr.dims[0], attr.dims[1], attr.dims[2], attr.dims[3],
                attr.type, attr.size);
    }

    // ── 2. 解析 config_json ──
    if (!config_json || config_json[0] == '\0') {
        log_info("[screen-detect] init: WARNING — no config_json provided, using defaults\n");
    } else {
        // 尝试作为文件路径读取
        char config_buf[32768] = {0};
        const char* source = config_json;
        FILE* cf = fopen(config_json, "r");
        if (cf) {
            size_t n = fread(config_buf, 1, sizeof(config_buf) - 1, cf);
            fclose(cf);
            if (n > 0) {
                config_buf[n] = '\0';
                source = config_buf;
                log_info("[screen-detect] init: read config file %s (%zu bytes)\n", config_json, n);
            }
        }

        // shm_list
        {
            auto parsed = json_get_str_array(source, "shm_list");
            if (!parsed.empty()) {
                g_shm_list = parsed;
                log_info("[screen-detect] init: shm_list=%zu channels\n", g_shm_list.size());
            }
        }
        // shm_name (fallback)
        if (g_shm_list.empty()) {
            char* shm = json_get_string(source, "shm_name");
            if (shm) {
                g_shm_list.push_back(std::string(shm));
                free(shm);
                log_info("[screen-detect] init: shm_name=%s\n", g_shm_list[0].c_str());
            }
        }

        // channels
        parse_channels(source);
        log_info("[screen-detect] init: channels=%zu parsed\n", g_channels.size());

        // 模型参数
        g_model_type = json_get_int(source, "model_type", 0);
        g_conf_thres = json_get_float(source, "conf_thres", 0.5f);
        g_iou_thres  = json_get_float(source, "iou_thres", 0.45f);

        // class_names
        {
            auto names = json_get_str_array(source, "class_names");
            if (!names.empty()) {
                g_num_classes = (int)names.size();
                if (g_num_classes > MAX_CLASSES) g_num_classes = MAX_CLASSES;
                for (int i = 0; i < g_num_classes; i++) {
                    strncpy(g_class_names[i], names[i].c_str(), DETECT_CLASS_NAME_LEN - 1);
                    g_class_names[i][DETECT_CLASS_NAME_LEN - 1] = '\0';
                }
                log_info("[screen-detect] init: %d classes loaded\n", g_num_classes);
            } else {
                // fallback: COCO 80
                g_num_classes = 80;
                log_info("[screen-detect] init: using default COCO 80 classes\n");
            }
        }

        // MQTT 配置
        {
            char* host = json_get_string(source, "mqtt_host");
            if (host) { strncpy(g_mqtt_host, host, sizeof(g_mqtt_host) - 1); free(host); }
        }
        g_mqtt_port = json_get_int(source, "mqtt_port", 1883);
        {
            char* prefix = json_get_string(source, "mqtt_topic_prefix");
            if (prefix) { strncpy(g_mqtt_topic_prefix, prefix, sizeof(g_mqtt_topic_prefix) - 1); free(prefix); }
        }
        {
            char* cid = json_get_string(source, "client_id");
            if (cid) { strncpy(g_client_id, cid, sizeof(g_client_id) - 1); free(cid); }
        }
        {
            char* db = json_get_string(source, "discovery_topic_base");
            if (db) { strncpy(g_discovery_base, db, sizeof(g_discovery_base) - 1); free(db); }
        }
        {
            char* ph = json_get_string(source, "preview_host");
            if (ph) { strncpy(g_preview_host, ph, sizeof(g_preview_host) - 1); free(ph); }
        }
        g_preview_port = json_get_int(source, "preview_port", 5544);
        g_rate_limit   = json_get_int(source, "rate_limit", 10);

        log_info("[screen-detect] init: MQTT %s:%d prefix=%s client=%s rate_limit=%d\n",
                g_mqtt_host, g_mqtt_port, g_mqtt_topic_prefix, g_client_id, g_rate_limit);
    }  // end config parse

    // 自动补全 channels (无配置时从 SHM 推断)
    if (g_channels.empty()) {
        for (size_t i = 0; i < g_shm_list.size(); i++) {
            ChannelCfg ch;
            ch.chid = (int)i + 1;
            ch.camera_id = (int)i;
            snprintf(ch.name, sizeof(ch.name), "camera_%d", ch.chid);
            g_channels.push_back(ch);
        }
    }

    // 初始化 YOLOv5 尺度配置
    if (g_model_type == 0) {
        init_yolo5_scales(g_model_w, g_model_h);
    }

    // ── 3. 初始化 MQTT 连接 ──
    if (g_mqtt_host[0] && g_mqtt_port > 0) {
        if (mqtt_ensure_connected() == 0) {
            publish_status("online");
            publish_discovery();
            publish_class_manifest();
            log_info("[screen-detect] init: MQTT connected and discovery published\n");
        } else {
            log_info("[screen-detect] init: WARNING MQTT connect failed, detections will not be published\n");
        }
    }

    g_initialized = 1;
    log_info("[screen-detect] init: SUCCESS (model=%dx%d type=%d shm=%zu classes=%d)\n",
            g_model_w, g_model_h, g_model_type, g_shm_list.size(), g_num_classes);
    return 0;
}

// ============================================================================
// algo_process_shm_detect — 多通道 SHM 轮询推理
// ============================================================================
extern "C" int algo_process_shm_detect(const char* output_img, AlgoDetection* dets,
                                        int max_dets, int* n_dets) {
    if (n_dets) *n_dets = 0;
    if (!dets || max_dets <= 0 || !n_dets) {
        log_info("[screen-detect] process: invalid args\n");
        return -1;
    }
    if (!g_initialized) {
        log_info("[screen-detect] process: not initialized\n");
        return -1;
    }
    if (g_shm_names.empty()) {
        log_info("[screen-detect] process: no shm_names configured\n");
        return 0;
    }

    int total = 0;

    for (const auto& shm_name : g_shm_names) {
        // ── 步骤 1: 打开共享内存读帧 ──
        int shm_fd = open(shm_name.c_str(), O_RDWR);
        if (shm_fd < 0) continue;

        size_t shm_size = sizeof(SharedFrame);
        void* shm_ptr = mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED) { close(shm_fd); continue; }

        SharedFrame* shm = static_cast<SharedFrame*>(shm_ptr);
        if (pthread_mutex_lock(&shm->lock) != 0) {
            pthread_mutex_consistent(&shm->lock);
            pthread_mutex_lock(&shm->lock);
        }

        int src_w = shm->width, src_h = shm->height;
        uint64_t frame_id = shm->frame_id;
        int frame_format = shm->format;

        // frame_id 无变化则跳过
        if (frame_id == g_last_frame_ids[shm_name] || src_w <= 0 || src_h <= 0) {
            pthread_mutex_unlock(&shm->lock);
            munmap(shm_ptr, shm_size);
            close(shm_fd);
            continue;
        }
        g_last_frame_ids[shm_name] = frame_id;

        // 拷贝帧数据
        size_t data_size = (size_t)src_w * src_h * MAX_SHARED_CHANNELS;
        uint8_t* bmp_data = (uint8_t*)malloc(data_size);
        if (!bmp_data) {
            pthread_mutex_unlock(&shm->lock);
            munmap(shm_ptr, shm_size);
            close(shm_fd);
            continue;
        }
        memcpy(bmp_data, shm->data, data_size);
        pthread_mutex_unlock(&shm->lock);
        munmap(shm_ptr, shm_size);
        close(shm_fd);

        // 提取通道号
        char ch_no[64] = "CH_UNKNOWN";
        extract_ch_no(shm_name.c_str(), ch_no, sizeof(ch_no));
        int chid = extract_chid(ch_no);

        // 查找通道配置
        int camera_id = chid - 1;  // fallback
        for (const auto& ch : g_channels) {
            if (ch.chid == chid) { camera_id = ch.camera_id; break; }
        }

        // ── 步骤 2: BGR → RGB ──
        if (frame_format == 0) {  // BGR
            for (int i = 0; i < src_w * src_h; i++) {
                uint8_t tmp = bmp_data[i * 3];
                bmp_data[i * 3]     = bmp_data[i * 3 + 2];
                bmp_data[i * 3 + 2] = tmp;
            }
        }
        // TODO: NV12 → RGB 转换 (如果 frame_format==3)

        // ── 步骤 3: RGA 预处理 (resize + letterbox) ──
        int dst_w = g_model_w, dst_h = g_model_h;
        int src_bpp = 3, dst_bpp = 3;
        int src_wstride = ALIGN_16(src_w * src_bpp);
        uint8_t* src_buf = (uint8_t*)malloc(src_wstride * src_h);
        uint8_t* dst_buf = (uint8_t*)malloc(dst_w * dst_h * dst_bpp);
        if (!src_buf || !dst_buf) {
            free(bmp_data);
            if (src_buf) free(src_buf);
            if (dst_buf) free(dst_buf);
            continue;
        }
        for (int i = 0; i < src_h; i++)
            memcpy(src_buf + i * src_wstride, bmp_data + i * src_w * src_bpp, src_w * src_bpp);
        memset(dst_buf, 0x72, dst_w * dst_h * dst_bpp);  // 灰色 padding

        float r_ratio = std::min((float)dst_h / src_h, (float)dst_w / src_w);
        int new_w = (int)(src_w * r_ratio), new_h = (int)(src_h * r_ratio);
        int pad_left = (dst_w - new_w) / 2, pad_top = (dst_h - new_h) / 2;

        rga_buffer_t src_img = wrapbuffer_virtualaddr(src_buf, src_w, src_h, RK_FORMAT_RGB_888);
        rga_buffer_t dst_img = wrapbuffer_virtualaddr(dst_buf, dst_w, dst_h, RK_FORMAT_RGB_888);
        im_rect src_rect = {0, 0, src_w, src_h};
        im_rect dst_rect = {pad_left, pad_top, new_w, new_h};
        rga_buffer_t pat_img; im_rect pat_rect;
        memset(&pat_img, 0, sizeof(pat_img)); memset(&pat_rect, 0, sizeof(pat_rect));

        IM_STATUS rga_ret = improcess(src_img, dst_img, pat_img, src_rect, dst_rect, pat_rect, IM_SYNC);
        if (rga_ret != IM_STATUS_SUCCESS) {
            // CPU 回退
            log_info("[screen-detect] process: RGA failed, CPU fallback ch=%s\n", ch_no);
            float scale = r_ratio;
            int sw = (int)(src_w * scale), sh = (int)(src_h * scale);
            int px = (dst_w - sw) / 2, py = (dst_h - sh) / 2;
            memset(dst_buf, 0x72, dst_w * dst_h * dst_bpp);
            for (int y = 0; y < sh; y++)
                for (int x = 0; x < sw; x++) {
                    int sx = std::min((int)(x / scale), src_w - 1);
                    int sy = std::min((int)(y / scale), src_h - 1);
                    uint8_t* d = dst_buf + ((py + y) * dst_w + (px + x)) * 3;
                    uint8_t* s = bmp_data + (sy * src_w + sx) * 3;
                    d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
                }
        }
        free(src_buf);

        // ── 步骤 4: NPU 推理 ──
        rknn_input inputs[1];
        memset(inputs, 0, sizeof(inputs));
        inputs[0].index = 0;
        inputs[0].type  = RKNN_TENSOR_UINT8;
        inputs[0].fmt   = RKNN_TENSOR_NHWC;
        inputs[0].buf   = dst_buf;
        inputs[0].size  = (uint32_t)(dst_w * dst_h * dst_bpp);

        int ret = rknn_inputs_set(g_ctx, 1, inputs);
        if (ret < 0) {
            log_info("[screen-detect] process: rknn_inputs_set failed\n");
            free(bmp_data); free(dst_buf); continue;
        }

        auto t0 = std::chrono::steady_clock::now();
        ret = rknn_run(g_ctx, nullptr);
        auto t1 = std::chrono::steady_clock::now();
        double infer_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ret < 0) {
            log_info("[screen-detect] process: rknn_run failed\n");
            free(bmp_data); free(dst_buf); continue;
        }

        rknn_output* outputs = (rknn_output*)calloc((size_t)g_n_outputs, sizeof(rknn_output));
        if (!outputs) {
            free(bmp_data); free(dst_buf); continue;
        }
        for (int i = 0; i < g_n_outputs; i++) outputs[i].want_float = 1;
        rknn_outputs_get(g_ctx, (uint32_t)g_n_outputs, outputs, nullptr);

        // ── 步骤 5: 后处理 ──
        int ch_dets = 0;
        int remaining = max_dets - total;

        if (g_model_type == 0) {
            // YOLOv5: 需要 3 个输出 tensor
            if (g_n_outputs >= 3) {
                const float* out_ptrs[3] = {
                    (const float*)outputs[0].buf,
                    (const float*)outputs[1].buf,
                    (const float*)outputs[2].buf
                };
                ch_dets = yolo5_postprocess(out_ptrs, g_yolo5_scales, g_num_classes,
                    g_model_w, g_model_h, src_w, src_h,
                    (float)pad_left, (float)pad_top, r_ratio,
                    g_conf_thres, g_iou_thres,
                    dets + total, remaining);
            } else {
                log_info("[screen-detect] process: YOLOv5 needs 3 outputs, got %d\n", g_n_outputs);
            }
        } else {
            // YOLOv8/v11: 单 tensor
            rknn_tensor_attr out_attr;
            memset(&out_attr, 0, sizeof(out_attr));
            out_attr.index = 0;
            rknn_query(g_ctx, RKNN_QUERY_OUTPUT_ATTR, &out_attr, sizeof(out_attr));
            int n_elems = 1;
            for (int d = 0; d < 4; d++) if (out_attr.dims[d] > 0) n_elems *= out_attr.dims[d];

            ch_dets = yolo8_postprocess((const float*)outputs[0].buf,
                n_elems, g_num_classes,
                g_model_w, g_model_h, src_w, src_h,
                (float)pad_left, (float)pad_top, r_ratio,
                g_conf_thres, g_iou_thres,
                dets + total, remaining);
        }

        // 填充 class_name
        for (int i = 0; i < ch_dets; i++) {
            AlgoDetection& det = dets[total + i];
            int cid = det.class_id;
            if (cid >= 0 && cid < g_num_classes) {
                strncpy(det.class_name, g_class_names[cid], DETECT_CLASS_NAME_LEN - 1);
                det.class_name[DETECT_CLASS_NAME_LEN - 1] = '\0';
            }
        }

        log_info("[screen-detect] process: ch=%s chid=%d frame=%lu infer=%.1fms dets=%d\n",
                ch_no, chid, (unsigned long)frame_id, infer_ms, ch_dets);

        // 释放 RKNN outputs
        rknn_outputs_release(g_ctx, (uint32_t)g_n_outputs, outputs);
        free(outputs);
        free(dst_buf);

        // ── 步骤 6: MQTT 发布 ──
        if (g_mqtt_host[0] && ch_dets > 0) {
            g_frame_counts[camera_id]++;
            publish_detections(camera_id, dets + total, ch_dets,
                               (float)infer_ms, g_frame_counts[camera_id]);
        }

        total += ch_dets;
        free(bmp_data);
    }

    *n_dets = total;
    return 0;
}

// ============================================================================
// algo_destroy — 释放资源
// ============================================================================
extern "C" void algo_destroy() {
    log_info("[screen-detect] destroy\n");

    if (g_mosq) {
        publish_status("offline");
        mosquitto_disconnect(g_mosq);
        mosquitto_destroy(g_mosq);
        g_mosq = nullptr;
    }

    if (g_ctx) {
        rknn_destroy(g_ctx);
        g_ctx = 0;
    }
    if (g_model_data) {
        free(g_model_data);
        g_model_data = nullptr;
    }

    g_shm_names.clear();
    g_channels.clear();
    g_last_frame_ids.clear();
    g_frame_counts.clear();
    g_last_publish_time.clear();
    g_initialized = 0;
    log_info("[screen-detect] destroy: done\n");
}
