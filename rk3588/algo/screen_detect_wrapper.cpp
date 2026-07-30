/**
 * screen_detect_wrapper.cpp — 屏幕检测系统算法包 (RK3588 NPU)
 *
 * 标准算法接口 (符合 efence_C++ bridge.h 规范):
 *   int  algo_init(const char* model_path, const char* config_json)
 *   int  algo_process_shm(const char* input_img, const char* output_img)
 *        → .so 内部画框+存图+MQTT (Go 前端自动对接)
 *   int  algo_process_shm_detect(const char* output_img, AlgoDetection* dets,
 *                                  int max_dets, int* n_dets)
 *        → 返回检测数据给 Go, Go 侧处理画框/存图/前端记录
 *   void algo_destroy()
 *
 * 数据流:
 *   SHM 读帧 (BGR/NV12) → BGR→RGB → RGA resize+letterbox (CPU 回退)
 *     → RKNN NPU 推理 → YOLO 后处理 (v5/v8/v11) → 填充 AlgoDetection[]
 *     → [draw_in_so=1] 内部画框 + 存标注图 + MQTT 发布
 *     → [draw_in_so=0] 存原图 (Go 后端画框) + MQTT 发布
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
#include <dlfcn.h>

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
    int mosquitto_will_set(struct mosquitto* mosq, const char* topic, int payloadlen,
                           const void* payload, int qos, bool retain);
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
#define DETECT_CLASS_NAME_LEN 32   // 与 bridge.h AlgoDetection::class_name[32] 保持一致
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
static int g_output_n_elems = 0;       // YOLOv8 输出 tensor 元素数 (在 algo_init 中缓存)
static int g_v8_n_anchors = 0;         // YOLOv8 锚点数 N (通常 8400)
static int g_v8_channel_first = 1;     // YOLOv8 tensor 布局: 1=[1,C,N] (RKNN 常见), 0=[1,N,C]
static int g_model_type = 0;           // 0=v5, 1=v8, 2=v11
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
static char g_alarm_topic[128] = "alarm/event";  // Go 前端告警记录 topic
static int  g_draw_in_so = 1;      // 1=.so内部画框+存标注图, 0=存原图交给Go后端画
static int  g_save_original = 0;   // draw_in_so=1时是否同时存原图 (供调试)
static char g_image_save_dir[256] = "";  // 检测到目标时额外存标注图到此目录 (空=不额外存)
                                         // 支持 {camera_id} / {chid} / {ch_no} 占位符

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
// RGB24 → JPEG (libjpeg), 用于按 efence 规范存原图供 Go 后端读取
// ============================================================================
static int save_rgb_to_jpeg(const char* path, const uint8_t* rgb, int w, int h, int quality) {
    FILE* fp = fopen(path, "wb");
    if (!fp) {
        log_info("[screen-detect] jpeg: open %s failed: %s\n", path, strerror(errno));
        return -1;
    }
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width      = w;
    cinfo.image_height     = h;
    cinfo.input_components = 3;
    cinfo.in_color_space   = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer[1];
    int row_stride = w * 3;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = (JSAMPROW)(rgb + cinfo.next_scanline * row_stride);
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(fp);
    return 0;
}

// ============================================================================
// 路径占位符替换: {camera_id} → 数字, {chid} → 数字
// ============================================================================
static void expand_path_placeholders(char* out, size_t out_size,
                                      const char* tmpl, int camera_id, int chid,
                                      const char* ch_no) {
    const char* src = tmpl;
    char* dst = out;
    char* end = out + out_size - 1;
    while (*src && dst < end) {
        if (strncmp(src, "{camera_id}", 11) == 0) {
            int n = snprintf(dst, end - dst + 1, "%d", camera_id);
            dst += n; src += 11;
        } else if (strncmp(src, "{chid}", 6) == 0) {
            int n = snprintf(dst, end - dst + 1, "%d", chid);
            dst += n; src += 6;
        } else if (strncmp(src, "{ch_no}", 7) == 0) {
            int n = snprintf(dst, end - dst + 1, "%s", ch_no ? ch_no : "");
            dst += n; src += 7;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// ============================================================================
// 画检测框 — 像素级 RGB 绘制 (参考 efence draw_fence_result)
// 每类使用 HSV 色相轮换颜色, 3px 粗边框 + 四角加强 + 标签徽章
// ============================================================================
static void draw_detection_boxes(uint8_t* rgb, int width, int height,
                                  const AlgoDetection* dets, int n_dets) {
    if (!rgb || n_dets <= 0) return;

    // HSV 色相轮换: 12种颜色覆盖不同类别
    static const uint8_t CLASS_COLORS[12][3] = {
        {0,   212, 255},  // 青色
        {0,   255, 0  },  // 绿色
        {255, 128, 0  },  // 橙色
        {255, 0,   255},  // 品红
        {255, 255, 0  },  // 黄色
        {0,   128, 255},  // 天蓝
        {128, 0,   255},  // 紫色
        {255, 0,   128},  // 玫红
        {128, 255, 0  },  // 黄绿
        {0,   255, 255},  // 青绿
        {255, 128, 128},  // 浅红
        {128, 128, 255},  // 蓝紫
    };
    static const uint8_t TXT_BG[3]   = {0, 0, 0};       // 标签黑底
    static const uint8_t TXT_FG[3]   = {255, 255, 255};  // 标签白字

    // 3×5 迷你数字字体 (0-9, .)
    static const uint8_t DIGIT[12][5] = {
        {0xE,0xA,0xA,0xA,0xE},  // 0
        {0x4,0xC,0x4,0x4,0xE},  // 1
        {0xE,0x2,0xE,0x8,0xE},  // 2
        {0xE,0x2,0x6,0x2,0xE},  // 3
        {0xA,0xA,0xE,0x2,0x2},  // 4
        {0xE,0x8,0xE,0x2,0xE},  // 5
        {0xE,0x8,0xE,0xA,0xE},  // 6
        {0xE,0x2,0x2,0x2,0x2},  // 7
        {0xE,0xA,0xE,0xA,0xE},  // 8
        {0xE,0xA,0xE,0x2,0xE},  // 9
        {0x0,0x0,0x0,0x0,0x0},  // (empty)
        {0x0,0x0,0x0,0x4,0x0},  // .
    };

    for (int di = 0; di < n_dets; di++) {
        const AlgoDetection& d = dets[di];
        if (d.conf <= 0.0f) continue;

        const uint8_t* color = CLASS_COLORS[d.class_id % 12];

        // 归一化坐标 → 像素坐标
        int x1 = std::max(0, (int)(d.x1 * width));
        int y1 = std::max(0, (int)(d.y1 * height));
        int x2 = std::min(width - 1, (int)(d.x2 * width));
        int y2 = std::min(height - 1, (int)(d.y2 * height));
        int bw = x2 - x1, bh = y2 - y1;
        if (bw < 2 || bh < 2) continue;

        // 边框线宽 (缩放自适应: min 1px, max 3px)
        int lw = std::max(1, std::min(3, std::min(bw, bh) / 60));

        // 绘制矩形边框 (上下 + 左右)
        for (int dy = 0; dy < lw; dy++) {
            int top_y = y1 + dy, bot_y = y2 - dy;
            if (top_y < height) {
                for (int x = x1; x <= x2; x++) {
                    int idx = (top_y * width + x) * 3;
                    rgb[idx + 0] = color[0]; rgb[idx + 1] = color[1]; rgb[idx + 2] = color[2];
                }
            }
            if (bot_y >= 0 && bot_y != top_y) {
                for (int x = x1; x <= x2; x++) {
                    int idx = (bot_y * width + x) * 3;
                    rgb[idx + 0] = color[0]; rgb[idx + 1] = color[1]; rgb[idx + 2] = color[2];
                }
            }
        }
        for (int dx = 0; dx < lw; dx++) {
            int left_x = x1 + dx, right_x = x2 - dx;
            if (left_x < width) {
                for (int y = y1 + lw; y < y2 - lw; y++) {
                    int idx = (y * width + left_x) * 3;
                    rgb[idx + 0] = color[0]; rgb[idx + 1] = color[1]; rgb[idx + 2] = color[2];
                }
            }
            if (right_x >= 0 && right_x != left_x) {
                for (int y = y1 + lw; y < y2 - lw; y++) {
                    int idx = (y * width + right_x) * 3;
                    rgb[idx + 0] = color[0]; rgb[idx + 1] = color[1]; rgb[idx + 2] = color[2];
                }
            }
        }

        // 四角加强 (corner accent): 每个角画水平+垂直短线
        int corner_len = std::max(8, std::min(bw, bh) / 4);
        int cw = std::max(2, std::min(4, corner_len / 5));
        for (int t = 0; t < cw; t++) {
            // 左上角 — 水平 (→) + 垂直 (↓)
            for (int i = 0; i < corner_len && y1 + t < height && x1 + i < width; i++) {
                int idx = ((y1 + t) * width + (x1 + i)) * 3;
                rgb[idx+0]=color[0]; rgb[idx+1]=color[1]; rgb[idx+2]=color[2];
            }
            for (int i = 0; i < corner_len && x1 + t < width && y1 + i < height; i++) {
                int idx = ((y1 + i) * width + (x1 + t)) * 3;
                rgb[idx+0]=color[0]; rgb[idx+1]=color[1]; rgb[idx+2]=color[2];
            }
            // 右上角 — 水平 (←) + 垂直 (↓)
            for (int i = 0; i < corner_len && y1 + t < height && x2 - i >= 0; i++) {
                int idx = ((y1 + t) * width + (x2 - i)) * 3;
                rgb[idx+0]=color[0]; rgb[idx+1]=color[1]; rgb[idx+2]=color[2];
            }
            for (int i = 0; i < corner_len && x2 - t >= 0 && y1 + i < height; i++) {
                int idx = ((y1 + i) * width + (x2 - t)) * 3;
                rgb[idx+0]=color[0]; rgb[idx+1]=color[1]; rgb[idx+2]=color[2];
            }
            // 左下角 — 水平 (→) + 垂直 (↑)
            for (int i = 0; i < corner_len && y2 - t >= 0 && x1 + i < width; i++) {
                int idx = ((y2 - t) * width + (x1 + i)) * 3;
                rgb[idx+0]=color[0]; rgb[idx+1]=color[1]; rgb[idx+2]=color[2];
            }
            for (int i = 0; i < corner_len && x1 + t < width && y2 - i >= 0; i++) {
                int idx = ((y2 - i) * width + (x1 + t)) * 3;
                rgb[idx+0]=color[0]; rgb[idx+1]=color[1]; rgb[idx+2]=color[2];
            }
            // 右下角 — 水平 (←) + 垂直 (↑)
            for (int i = 0; i < corner_len && y2 - t >= 0 && x2 - i >= 0; i++) {
                int idx = ((y2 - t) * width + (x2 - i)) * 3;
                rgb[idx+0]=color[0]; rgb[idx+1]=color[1]; rgb[idx+2]=color[2];
            }
            for (int i = 0; i < corner_len && x2 - t >= 0 && y2 - i >= 0; i++) {
                int idx = ((y2 - i) * width + (x2 - t)) * 3;
                rgb[idx+0]=color[0]; rgb[idx+1]=color[1]; rgb[idx+2]=color[2];
            }
        }

        // 标签徽章: 类名 + 置信度, 贴边框顶部
        if (d.class_name[0]) {
            char label[64];
            snprintf(label, sizeof(label), "%s %.2f", d.class_name, d.conf);

            int label_w = (int)strlen(label) * 4 + 4;  // 3px字体 + 1px间距
            int label_h = 7;  // 5px 字体 + 2px padding
            int lx = std::max(0, x1);
            int ly = std::max(0, y1 - label_h - 2);
            if (ly < 0) ly = y2 + 2;  // 框太靠顶, 放框下方
            if (ly + label_h >= height) ly = y1 + 2;  // 框太靠底, 放框内顶部

            int lbl_end_x = std::min(lx + label_w, width - 1);
            int lbl_end_y = std::min(ly + label_h, height - 1);

            // 标签背景
            for (int y = ly; y < lbl_end_y; y++) {
                for (int x = lx; x < lbl_end_x; x++) {
                    int idx = (y * width + x) * 3;
                    rgb[idx + 0] = TXT_BG[0]; rgb[idx + 1] = TXT_BG[1]; rgb[idx + 2] = TXT_BG[2];
                }
            }
            // 标签文字 (3x5 像素字体)
            int cx = lx + 2, cy = ly + 1;
            for (int ci = 0; label[ci] && cx + 3 < lbl_end_x; ci++) {
                char ch = label[ci];
                int di = (ch >= '0' && ch <= '9') ? (ch - '0') :
                         (ch == '.') ? 11 : (ch == ' ') ? 10 : -1;
                if (di < 0) { cx++; continue; }
                for (int row = 0; row < 5 && cy + row < lbl_end_y; row++) {
                    uint8_t bits = DIGIT[di][row];
                    for (int col = 0; col < 4 && cx + col < lbl_end_x; col++) {
                        if (bits & (1 << (3 - col))) {
                            int idx = ((cy + row) * width + (cx + col)) * 3;
                            rgb[idx + 0] = TXT_FG[0];
                            rgb[idx + 1] = TXT_FG[1];
                            rgb[idx + 2] = TXT_FG[2];
                        }
                    }
                }
                cx += 4;
            }
        }
    }
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
    // LWT: 进程崩溃时 broker 自动发布 offline 状态
    {
        char lwt_payload[256];
        snprintf(lwt_payload, sizeof(lwt_payload),
            "{\"status\":\"offline\",\"client_id\":\"%s\"}", g_client_id);
        mosquitto_will_set(g_mosq, "inference/bridge/status",
            (int)strlen(lwt_payload), lwt_payload, 1, true);
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
    // 驱动 I/O (非阻塞) — loop_read 消费 QoS 1 PUBACK, loop_misc 处理心跳
    mosquitto_loop_write(g_mosq, 1);
    mosquitto_loop_read(g_mosq, 1);
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
    // rate limit (使用单调时钟, 精度毫秒级)
    struct timespec ts_now;
    clock_gettime(CLOCK_MONOTONIC, &ts_now);
    double now_sec = (double)ts_now.tv_sec + ts_now.tv_nsec * 1e-9;
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
// 发布告警事件到 Go 前端 (对应 person_efence send_mqtt_alarm)
// topic: alarm/event (可由 alarm_topic 配置)
// ============================================================================
static void publish_alarm_event(const char* ch_no, const char* image_path,
                                 const AlgoDetection* dets, int n_dets,
                                 int camera_id) {
    if (!ch_no || !image_path || n_dets <= 0) return;
    if (!g_alarm_topic[0]) return;

    // 时间戳
    time_t now = time(nullptr);
    struct tm tm_buf;
    char detected_at[64];
    strftime(detected_at, sizeof(detected_at), "%Y-%m-%dT%H:%M:%S+08:00",
             localtime_r(&now, &tm_buf));

    // 相对路径: 取最后两段 (如 "69/output_xxx_CH08.jpg")
    char img_rel[1024] = "";
    {
        const char* fname = strrchr(image_path, '/');
        if (fname && fname > image_path) {
            // 临时拷贝 parent 目录部分, 找倒数第二个 '/'
            char tmp[1024];
            size_t len = (size_t)(fname - image_path);
            if (len < sizeof(tmp)) {
                memcpy(tmp, image_path, len);
                tmp[len] = '\0';
                const char* parent = strrchr(tmp, '/');
                if (parent) {
                    snprintf(img_rel, sizeof(img_rel), "%s/%s", parent + 1, fname + 1);
                } else {
                    snprintf(img_rel, sizeof(img_rel), "%s", fname + 1);
                }
            }
        } else {
            snprintf(img_rel, sizeof(img_rel), "%s", image_path);
        }
    }

    // 统计
    float max_conf = 0;
    for (int i = 0; i < n_dets; i++) {
        if (dets[i].conf > max_conf) max_conf = dets[i].conf;
    }

    // 构造 detections 数组
    char dets_json[8192] = {0};
    int off = 0;
    for (int i = 0; i < n_dets && off < (int)sizeof(dets_json) - 300; i++) {
        const AlgoDetection& d = dets[i];
        float cx = (d.x1 + d.x2) / 2.0f;
        float cy = (d.y1 + d.y2) / 2.0f;
        float w  = d.x2 - d.x1;
        float h  = d.y2 - d.y1;
        off += snprintf(dets_json + off, sizeof(dets_json) - off,
            "%s{\"class_id\":%d,\"class_name\":\"%s\",\"conf\":%.2f,"
            "\"bbox\":{\"cx\":%.4f,\"cy\":%.4f,\"w\":%.4f,\"h\":%.4f}}",
            i > 0 ? "," : "",
            d.class_id,
            d.class_name[0] ? d.class_name : "unknown",
            d.conf, cx, cy, w, h);
    }

    char payload[10240];
    snprintf(payload, sizeof(payload),
        "{"
        "\"alarm_type\":\"screen_detect\","
        "\"ch_no\":\"%s\","
        "\"camera_id\":%d,"
        "\"image\":\"%s\","
        "\"image_path\":\"%s\","
        "\"confidence\":%.2f,"
        "\"count\":%d,"
        "\"detections\":[%s],"
        "\"detected_at\":\"%s\""
        "}",
        ch_no, camera_id,
        img_rel, img_rel, max_conf, n_dets,
        dets_json, detected_at);

    mqtt_publish(g_alarm_topic, payload, 0, false);
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

    // 打印输出 tensor 属性 (调试) + 缓存 output[0] 维度用于后续推断
    rknn_tensor_attr out0_attr;
    memset(&out0_attr, 0, sizeof(out0_attr));
    for (int i = 0; i < g_n_outputs; i++) {
        rknn_tensor_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.index = i;
        rknn_query(g_ctx, RKNN_QUERY_OUTPUT_ATTR, &attr, sizeof(attr));
        log_info("[screen-detect] init: output[%d] dims=[%d,%d,%d,%d] type=%d size=%d\n",
                i, attr.dims[0], attr.dims[1], attr.dims[2], attr.dims[3],
                attr.type, attr.size);
        if (i == 0) out0_attr = attr;
    }

    // ── 从 output tensor shape 自动推断 model_type 和 num_classes ──
    // 逻辑:
    //   - 3 输出 tensor  → YOLOv5 (raw features, anchor-based)
    //     nc 从最大 tensor 反推: n_elems = grid*grid*3*(5+nc) → nc = n_elems/(grid*grid*3) - 5
    //   - 1 输出 tensor  → YOLOv8/v11 (decoded, anchor-free)
    //     dims=[1, 4+nc, N] (channel-first) 或 [1, N, 4+nc] (channel-last)
    //     N 通常是 8400 (640输入) / 2100 (320输入), 4+nc 一般较小 (≤85)
    //   自动推断结果作为初值, config 里显式设 model_type / class_names 可覆盖
    if (g_n_outputs == 3) {
        // YOLOv5
        g_model_type = 0;
        // 找最大 tensor (stride=8, grid 最大) 反推 nc
        int max_n = 0;
        for (int i = 0; i < 3; i++) {
            rknn_tensor_attr a; memset(&a, 0, sizeof(a)); a.index = i;
            if (rknn_query(g_ctx, RKNN_QUERY_OUTPUT_ATTR, &a, sizeof(a)) != 0) continue;
            int n = 1;
            for (int d = 0; d < 4; d++) if (a.dims[d] > 0) n *= a.dims[d];
            if (n > max_n) max_n = n;
        }
        int grid_max = g_model_w / 8;
        int per_cell = 3 * grid_max * grid_max;
        if (per_cell > 0 && max_n > 5 * per_cell) {
            int nc_est = max_n / per_cell - 5;
            if (nc_est > 0 && nc_est <= MAX_CLASSES) g_num_classes = nc_est;
        }
        log_info("[screen-detect] init: auto-detected YOLOv5, num_classes=%d\n", g_num_classes);
    } else if (g_n_outputs == 1) {
        // YOLOv8/v11 — 计算总元素数, 推断布局 (channel-first vs channel-last) 和维度
        g_output_n_elems = 1;
        for (int d = 0; d < 4; d++)
            if (out0_attr.dims[d] > 0) g_output_n_elems *= out0_attr.dims[d];

        // 找 batch 之后的两个非零维度, 记住它们的位置和值
        int pos[2] = {-1, -1}, val[2] = {0, 0}, k = 0;
        for (int d = 1; d < 4 && k < 2; d++) {
            if (out0_attr.dims[d] > 0) { pos[k] = d; val[k] = out0_attr.dims[d]; k++; }
        }
        if (k == 2) {
            // 较小的维度是 4+nc, 较大的是 N; 若 C 在前 (pos 更小) 则 channel-first
            int c_pos, n_val, c_val;
            if (val[0] < val[1]) { c_pos = pos[0]; c_val = val[0]; n_val = val[1]; }
            else                 { c_pos = pos[1]; c_val = val[1]; n_val = val[0]; }
            g_v8_channel_first = (c_pos < pos[(val[0] < val[1]) ? 1 : 0]) ? 1 : 0;
            g_v8_n_anchors = n_val;
            if (c_val > 4 && c_val - 4 <= MAX_CLASSES) g_num_classes = c_val - 4;
        }
        g_model_type = 2;  // v8/v11 后处理相同, 默认标 v11 (更新)
        log_info("[screen-detect] init: auto-detected YOLOv8/v11, num_classes=%d, "
                "n_anchors=%d, channel_first=%d\n",
                g_num_classes, g_v8_n_anchors, g_v8_channel_first);
    } else {
        log_info("[screen-detect] init: WARNING unexpected n_outputs=%d, keeping defaults\n",
                g_n_outputs);
    }

    // ── 2. 定位持久化 sidecar 配置文件 ──
    // 分工:
    //   edge_server 传的 config_json (.runtime_init.json) → 提供 shm_name (运行时分配)
    //   .so/model 同目录的 algo_config.json                → 提供算法专属配置
    //     (class_names / client_id / mqtt / preview_host 等, 部署时固化, 不随重启变)
    // 两个文件都解析, sidecar 覆盖 runtime 里同名字段。
    char algo_cfg_path[1024] = {0};
    {
        auto try_path = [](const char* dir, const char* name, char* out, size_t out_size) -> bool {
            if (!dir || !dir[0]) return false;
            snprintf(out, out_size, "%s/%s", dir, name);
            FILE* f = fopen(out, "r");
            if (f) { fclose(f); return true; }
            out[0] = '\0';
            return false;
        };
        Dl_info dl_info;
        char so_dir[512] = {0};
        if (dladdr((void*)&algo_init, &dl_info) && dl_info.dli_fname) {
            char so_path[512] = {0};
            strncpy(so_path, dl_info.dli_fname, sizeof(so_path) - 1);
            const char* d = dirname(so_path);
            if (d) strncpy(so_dir, d, sizeof(so_dir) - 1);
        }
        char model_dir[512] = {0};
        {
            char mp[512] = {0};
            strncpy(mp, model_path, sizeof(mp) - 1);
            const char* d = dirname(mp);
            if (d) strncpy(model_dir, d, sizeof(model_dir) - 1);
        }
        if (try_path(so_dir,    "algo_config.json", algo_cfg_path, sizeof(algo_cfg_path)) ||
            try_path(model_dir, "algo_config.json", algo_cfg_path, sizeof(algo_cfg_path))) {
            log_info("[screen-detect] init: found sidecar config %s\n", algo_cfg_path);
        }
    }

    // ── 3. 解析配置 (先 runtime, 再 sidecar 覆盖) ──
    auto parse_config = [](const char* cfg_arg) {
        if (!cfg_arg || cfg_arg[0] == '\0') return;

        // 若是路径, 读文件内容; 否则当 JSON 字符串
        char config_buf[32768] = {0};
        const char* source = cfg_arg;
        FILE* cf = fopen(cfg_arg, "r");
        if (cf) {
            size_t n = fread(config_buf, 1, sizeof(config_buf) - 1, cf);
            fclose(cf);
            if (n > 0) {
                config_buf[n] = '\0';
                source = config_buf;
                log_info("[screen-detect] init: read config file %s (%zu bytes)\n", cfg_arg, n);
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
        // shm_name (fallback, 单通道)
        if (g_shm_list.empty()) {
            char* shm = json_get_string(source, "shm_name");
            if (shm) {
                g_shm_list.push_back(std::string(shm));
                free(shm);
                log_info("[screen-detect] init: shm_name=%s\n", g_shm_list.back().c_str());
            }
        }

        // channels (可选; 显式提供则替换)
        {
            std::vector<ChannelCfg> saved = g_channels;
            parse_channels(source);
            if (g_channels.empty() && !saved.empty()) g_channels = saved;
            else if (!g_channels.empty())
                log_info("[screen-detect] init: channels=%zu parsed\n", g_channels.size());
        }

        // 模型参数 — 仅当 config 里显式提供才覆盖
        int cfg_model_type = json_get_int(source, "model_type", -1);
        if (cfg_model_type >= 0) g_model_type = cfg_model_type;
        g_conf_thres = json_get_float(source, "conf_thres", g_conf_thres);
        g_iou_thres  = json_get_float(source, "iou_thres", g_iou_thres);

        // class_names — 只填显示名, 数量以模型推断的 g_num_classes 为准。
        // 不匹配时: 多的忽略, 少的用 class_N 占位, 并打警告。
        {
            auto names = json_get_str_array(source, "class_names");
            if (!names.empty()) {
                int provided = (int)names.size();
                if (provided != g_num_classes) {
                    log_info("[screen-detect] init: WARNING class_names count %d != model nc %d, "
                            "using model nc\n", provided, g_num_classes);
                }
                int fill = std::min(provided, g_num_classes);
                for (int i = 0; i < fill; i++) {
                    strncpy(g_class_names[i], names[i].c_str(), DETECT_CLASS_NAME_LEN - 1);
                    g_class_names[i][DETECT_CLASS_NAME_LEN - 1] = '\0';
                }
                for (int i = fill; i < g_num_classes; i++) {
                    snprintf(g_class_names[i], DETECT_CLASS_NAME_LEN, "class_%d", i);
                }
                log_info("[screen-detect] init: %d class names loaded (model nc=%d)\n",
                        fill, g_num_classes);
            }
        }

        // MQTT / preview
        {
            char* host = json_get_string(source, "mqtt_host");
            if (host) { strncpy(g_mqtt_host, host, sizeof(g_mqtt_host) - 1); free(host); }
        }
        {
            int p = json_get_int(source, "mqtt_port", -1);
            if (p > 0) g_mqtt_port = p;
        }
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
        {
            int p = json_get_int(source, "preview_port", -1);
            if (p > 0) g_preview_port = p;
        }
        {
            int r = json_get_int(source, "rate_limit", -1);
            if (r > 0) g_rate_limit = r;
        }
        {
            int v = json_get_int(source, "draw_in_so", -1);
            if (v >= 0) g_draw_in_so = v;
        }
        {
            int v = json_get_int(source, "save_original", -1);
            if (v >= 0) g_save_original = v;
        }
        {
            char* dir = json_get_string(source, "image_save_dir");
            if (dir) {
                strncpy(g_image_save_dir, dir, sizeof(g_image_save_dir) - 1);
                free(dir);
            }
        }
        {
            char* t = json_get_string(source, "alarm_topic");
            if (t) { strncpy(g_alarm_topic, t, sizeof(g_alarm_topic) - 1); free(t); }
        }
    };

    // 顺序: runtime → sidecar (后者覆盖前者)
    parse_config(config_json);
    if (algo_cfg_path[0]) parse_config(algo_cfg_path);

    // class_names 未设置时的兜底占位
    if (g_class_names[0][0] == '\0') {
        for (int i = 0; i < g_num_classes; i++) {
            snprintf(g_class_names[i], DETECT_CLASS_NAME_LEN, "class_%d", i);
        }
        log_info("[screen-detect] init: no class_names configured, using placeholder class_0..%d\n",
                g_num_classes - 1);
    }

    log_info("[screen-detect] init: MQTT %s:%d prefix=%s client=%s rate_limit=%d draw_in_so=%d image_save_dir=%s\n",
            g_mqtt_host, g_mqtt_port, g_mqtt_topic_prefix, g_client_id, g_rate_limit, g_draw_in_so,
            g_image_save_dir[0] ? g_image_save_dir : "(none)");

    // 自动补全 channels — 从 SHM 文件名解析 chid (如 CH06 → 6),
    // 保证 discovery 里的 chid 和 process 日志中的 chid 一致
    if (g_channels.empty()) {
        for (size_t i = 0; i < g_shm_list.size(); i++) {
            ChannelCfg ch;
            memset(&ch, 0, sizeof(ch));
            char ch_no[64] = "";
            extract_ch_no(g_shm_list[i].c_str(), ch_no, sizeof(ch_no));
            int chid = extract_chid(ch_no);
            if (chid <= 0) chid = (int)i + 1;
            ch.chid = chid;
            ch.camera_id = (int)i;
            snprintf(ch.name, sizeof(ch.name), "camera_%d", chid);
            g_channels.push_back(ch);
            log_info("[screen-detect] init: auto-added channel chid=%d camera_id=%d shm=%s\n",
                    ch.chid, ch.camera_id, g_shm_list[i].c_str());
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

// algo_process_shm_detect 前置声明 (algo_process_shm 内部调用)
extern "C" int algo_process_shm_detect(const char* output_img, AlgoDetection* dets,
                                        int max_dets, int* n_dets);

// ============================================================================
// algo_process_shm — .so 内部画框+存图+MQTT (Go edge_server 主接口)
// Go 前端通过此接口自动对接：存图到 images/ + MQTT 推送给展示端
// ============================================================================
extern "C" int algo_process_shm(const char* input_img, const char* output_img) {
    // 内部调用 algo_process_shm_detect, 走 draw_in_so=1 全流程
    // Go 只需要拿到成功/失败返回值, 检测数据由 .so 内部处理
    AlgoDetection dets[MAX_DETECTIONS];
    int n_dets = 0;
    (void)input_img;   // SHM 模式不使用 input_img
    return algo_process_shm_detect(output_img, dets, MAX_DETECTIONS, &n_dets);
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
    if (g_shm_list.empty()) {
        log_info("[screen-detect] process: no shm_list configured\n");
        return 0;
    }

    int total = 0;

    for (const auto& shm_name : g_shm_list) {
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
        ret = rknn_outputs_get(g_ctx, (uint32_t)g_n_outputs, outputs, nullptr);
        if (ret < 0) {
            log_info("[screen-detect] process: rknn_outputs_get failed ret=%d\n", ret);
            free(outputs);
            free(bmp_data); free(dst_buf); continue;
        }

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
            // YOLOv8/v11: 单 tensor decoded 输出
            ch_dets = yolo8_postprocess((const float*)outputs[0].buf,
                g_v8_n_anchors, g_num_classes, g_v8_channel_first,
                g_model_w, g_model_h, src_w, src_h,
                (float)pad_left, (float)pad_top, r_ratio,
                g_conf_thres, g_iou_thres,
                dets + total, remaining);
        }

        // 填 class_name (memset 已归零, capabilities 保持 0 → Go 走 v1 默认路径)
        for (int i = 0; i < ch_dets; i++) {
            AlgoDetection& det = dets[total + i];
            int cid = det.class_id;
            if (cid >= 0 && cid < g_num_classes) {
                strncpy(det.class_name, g_class_names[cid], DETECT_CLASS_NAME_LEN - 1);
                det.class_name[DETECT_CLASS_NAME_LEN - 1] = '\0';
            }
        }

        // 精简日志: 每个 det 一行 (class_id, conf, bbox), 用于线上排查
        for (int i = 0; i < ch_dets; i++) {
            const AlgoDetection& det = dets[total + i];
            log_info("  det[%d] cid=%d conf=%.2f bbox=(%.3f,%.3f,%.3f,%.3f)\n",
                    i, det.class_id, det.conf,
                    det.x1, det.y1, det.x2, det.y2);
        }

        log_info("[screen-detect] process: ch=%s chid=%d frame=%lu infer=%.1fms dets=%d\n",
                ch_no, chid, (unsigned long)frame_id, infer_ms, ch_dets);

        // 释放 RKNN outputs
        rknn_outputs_release(g_ctx, (uint32_t)g_n_outputs, outputs);
        free(outputs);
        free(dst_buf);

        // ── 步骤 6a: 存图 (双模式) ──
        //   output_img 来自 Go edge_server 传参, 为空时 fallback 到 /tmp/
        {
            // 诊断: 首帧打印 output_img 参数值
            static int output_img_logged = 0;
            if (!output_img_logged) {
                log_info("[screen-detect] process: output_img=%s\n",
                        output_img ? output_img : "(null)");
                output_img_logged = 1;
            }

            // 确定输出路径: 优先用 Go 传入的, 否则 fallback 到 /tmp/
            char ch_output[1024] = {0};
            bool use_fallback = (!output_img || !output_img[0]);

            if (use_fallback) {
                // Go 侧未传 output_img, 自动生成路径
                snprintf(ch_output, sizeof(ch_output),
                        "/tmp/screen_detect_%s.jpg", ch_no);
            } else {
                size_t prefix_len = 0;
                const char* dot = strrchr(output_img, '.');
                const char* slash = strrchr(output_img, '/');
                const char* base = slash ? slash + 1 : output_img;
                bool has_dot_ext = (dot && dot > base);
                if (has_dot_ext) {
                    const char* strip = nullptr;
                    for (const char* p = dot - 1; p > base; p--) {
                        if (*p == '_' && (p + 1) < dot &&
                            *(p + 1) == 'C' && *(p + 2) == 'H' &&
                            (p + 3) < dot && *(p + 3) >= '0' && *(p + 3) <= '9') {
                            strip = p; break;
                        }
                    }
                    prefix_len = strip ? (size_t)(strip - output_img)
                                       : (size_t)(dot - output_img);
                    if (prefix_len < sizeof(ch_output) - 32) {
                        memcpy(ch_output, output_img, prefix_len);
                        snprintf(ch_output + prefix_len, sizeof(ch_output) - prefix_len,
                                "_%s%s", ch_no, dot);
                    }
                }
                if (!ch_output[0]) {
                    snprintf(ch_output, sizeof(ch_output), "%s_%s.jpg", output_img, ch_no);
                }
            }

            if (g_draw_in_so) {
                // 模式 A: .so 内部画框 + 存标注图
                if (g_save_original) {
                    char orig_output[1056];
                    snprintf(orig_output, sizeof(orig_output),
                            "/tmp/screen_detect_%s_orig.jpg", ch_no);
                    save_rgb_to_jpeg(orig_output, bmp_data, src_w, src_h, 85);
                }
                draw_detection_boxes(bmp_data, src_w, src_h, dets + total, ch_dets);
                if (ch_output[0] && save_rgb_to_jpeg(ch_output, bmp_data, src_w, src_h, 85) != 0) {
                    log_info("[screen-detect] process: save annotated image failed: %s\n", ch_output);
                } else if (ch_output[0]) {
                    log_info("[screen-detect] process: saved %s\n", ch_output);

                    // 额外存一份到 images/ 目录 + 发布 alarm/event 给 Go 前端
                    if (ch_dets > 0 && !use_fallback && ch_output[0]) {
                        const char* alarm_img_path = ch_output;  // 默认用 video_frames 路径
                        const char* vf_tag = strstr(ch_output, "video_frames");
                        if (vf_tag) {
                            const char* fname = strrchr(ch_output, '/');
                            fname = fname ? fname + 1 : ch_output;
                            char img_path[1024];
                            size_t base_len = (size_t)(vf_tag - ch_output);
                            memcpy(img_path, ch_output, base_len);
                            int off = (int)base_len;
                            off += snprintf(img_path + off, sizeof(img_path) - off, "images");
                            const char* trail = vf_tag + 12;
                            snprintf(img_path + off, sizeof(img_path) - off, "%s", trail);
                            char* last_slash = strrchr(img_path, '/');
                            if (last_slash) {
                                *last_slash = '\0';
                                char mkdir_cmd[1060];
                                snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", img_path);
                                int mk_ret = system(mkdir_cmd);
                                (void)mk_ret;
                                *last_slash = '/';
                            }
                            if (save_rgb_to_jpeg(img_path, bmp_data, src_w, src_h, 85) == 0) {
                                log_info("[screen-detect] process: image copy saved %s\n", img_path);
                                alarm_img_path = img_path;
                            }
                        } else if (g_image_save_dir[0]) {
                            const char* fname = strrchr(ch_output, '/');
                            fname = fname ? fname + 1 : ch_output;
                            char img_dir[512];
                            expand_path_placeholders(img_dir, sizeof(img_dir),
                                                    g_image_save_dir, camera_id, chid, ch_no);
                            char img_path[1024];
                            int n = snprintf(img_path, sizeof(img_path), "%s/%s", img_dir, fname);
                            if (n > 0 && n < (int)sizeof(img_path)) {
                                char mkdir_cmd[1060];
                                snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", img_dir);
                                int mk_ret = system(mkdir_cmd);
                                (void)mk_ret;
                                if (save_rgb_to_jpeg(img_path, bmp_data, src_w, src_h, 85) == 0) {
                                    log_info("[screen-detect] process: image copy saved %s\n", img_path);
                                    alarm_img_path = img_path;
                                }
                            }
                        }
                        // 发布告警事件到 Go 前端 (只发一次)
                        publish_alarm_event(ch_no, alarm_img_path,
                                           dets + total, ch_dets, camera_id);
                    }
                }
            } else {
                // 模式 B: 存原图 (Go 后端画框)
                if (ch_output[0] && save_rgb_to_jpeg(ch_output, bmp_data, src_w, src_h, 85) != 0) {
                    log_info("[screen-detect] process: save original image failed: %s\n", ch_output);
                } else if (ch_output[0]) {
                    log_info("[screen-detect] process: saved original %s\n", ch_output);
                }
            }
        }

        // ── 步骤 6b: MQTT 发布 (供 Qt 展示端订阅, 与 Go 后端 sendAlarmMQTT 并行) ──
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

    g_shm_list.clear();
    g_channels.clear();
    g_last_frame_ids.clear();
    g_frame_counts.clear();
    g_last_publish_time.clear();
    g_initialized = 0;
    log_info("[screen-detect] destroy: done\n");
}
