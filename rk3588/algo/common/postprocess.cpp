/**
 * postprocess.cpp — YOLO 后处理实现 (decode + NMS)
 *
 * YOLOv5: 参考 person_efence_wrapper_v2.cpp 的 anchor-based 三尺度解码
 * YOLOv8/v11: anchor-free, stride=8/16/32 三尺度解码
 */

#include "postprocess.h"
#include "bridge.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstring>

// ============================================================================
// bbox IoU
// ============================================================================
float bbox_iou(float x1_a, float y1_a, float x2_a, float y2_a,
               float x1_b, float y1_b, float x2_b, float y2_b) {
    float inter_x1 = std::max(x1_a, x1_b);
    float inter_y1 = std::max(y1_a, y1_b);
    float inter_x2 = std::min(x2_a, x2_b);
    float inter_y2 = std::min(y2_a, y2_b);
    if (inter_x1 >= inter_x2 || inter_y1 >= inter_y2) return 0.0f;

    float inter_area = (inter_x2 - inter_x1) * (inter_y2 - inter_y1);
    float area_a = (x2_a - x1_a) * (y2_a - y1_a);
    float area_b = (x2_b - x1_b) * (y2_b - y1_b);
    float union_area = area_a + area_b - inter_area;
    if (union_area <= 0.0f) return 0.0f;
    return inter_area / union_area;
}

// ============================================================================
// NMS — 按 class 独立抑制, 原地操作
// ============================================================================
int nms(AlgoDetection* dets, int n, float iou_thres) {
    // 按置信度降序排列
    std::sort(dets, dets + n, [](const AlgoDetection& a, const AlgoDetection& b) {
        return a.conf > b.conf;
    });

    bool suppressed[256] = {false};
    int kept = 0;

    for (int i = 0; i < n && i < 256; i++) {
        if (dets[i].conf <= 0.0f) continue;
        bool skip = false;
        for (int j = 0; j < i; j++) {
            if (suppressed[j]) continue;
            if (dets[i].class_id != dets[j].class_id) continue;
            float iou = bbox_iou(dets[i].x1, dets[i].y1, dets[i].x2, dets[i].y2,
                                 dets[j].x1, dets[j].y1, dets[j].x2, dets[j].y2);
            if (iou > iou_thres) {
                skip = true;
                break;
            }
        }
        if (!skip) {
            suppressed[i] = false;  // keep
            kept++;
        } else {
            dets[i].conf = 0.0f;  // 标记为抑制
        }
    }

    // 压缩: 移除被抑制的 (conf==0)
    int out_idx = 0;
    for (int i = 0; i < n; i++) {
        if (dets[i].conf > 0.0f) {
            if (out_idx != i) dets[out_idx] = dets[i];
            out_idx++;
        }
    }
    return out_idx;
}

// ============================================================================
// 将模型坐标 (letterbox 域) 映射回原始图像坐标, 归一化到 [0,1]
// ============================================================================
static void rescale_bbox(float& x1, float& y1, float& x2, float& y2,
                         float pad_left, float pad_top, float scale,
                         int img_w, int img_h) {
    // 从 letterbox 坐标 → 原始图像坐标
    x1 = (x1 - pad_left) / scale;
    y1 = (y1 - pad_top)  / scale;
    x2 = (x2 - pad_left) / scale;
    y2 = (y2 - pad_top)  / scale;

    // 裁剪到图像范围内
    x1 = std::max(0.0f, std::min((float)img_w, x1));
    y1 = std::max(0.0f, std::min((float)img_h, y1));
    x2 = std::max(0.0f, std::min((float)img_w, x2));
    y2 = std::max(0.0f, std::min((float)img_h, y2));

    // 归一化到 [0, 1]
    x1 /= (float)img_w;
    y1 /= (float)img_h;
    x2 /= (float)img_w;
    y2 /= (float)img_h;
}

// ============================================================================
// YOLOv5 anchor-based 三尺度后处理
//
// 每个尺度 (stride=8/16/32):
//   tensor shape: [1, 3*(5+nc), grid_h, grid_w]
//   每 grid cell 有 n_anchors(3) 个预测:
//     raw[0..3] = tx, ty, tw, th
//     raw[4]    = obj (objectness)
//     raw[5..]  = class scores
//
// 解码公式 (参考 person_efence_wrapper_v2.cpp):
//   cx = (sigmoid(tx)*2 - 0.5 + grid_x) * stride
//   cy = (sigmoid(ty)*2 - 0.5 + grid_y) * stride
//   w  = pow(sigmoid(tw)*2, 2) * anchor_w
//   h  = pow(sigmoid(th)*2, 2) * anchor_h
//   conf = sigmoid(obj) * sigmoid(max_cls_score)
// ============================================================================
int yolo5_postprocess(const float* outputs[3],
                      const YOLOScale scales[3],
                      int nc,
                      int model_w, int model_h,
                      int img_w, int img_h,
                      float pad_left, float pad_top, float scale,
                      float conf_thres, float iou_thres,
                      AlgoDetection* dets, int max_dets) {
    int num_dets = 0;

    for (int s = 0; s < 3; s++) {
        const YOLOScale& sc = scales[s];
        int stride      = sc.stride;
        int grid_w      = sc.grid_w;
        int grid_h      = sc.grid_h;
        int n_anchors   = sc.n_anchors;
        int row_size    = 5 + nc;  // tx,ty,tw,th,obj + class_scores
        const float* data = outputs[s];

        for (int ay = 0; ay < grid_h; ay++) {
            for (int ax = 0; ax < grid_w; ax++) {
                for (int a = 0; a < n_anchors; a++) {
                    int offset = ((ay * grid_w + ax) * n_anchors + a) * row_size;
                    const float* raw = data + offset;

                    float tx   = raw[0];
                    float ty   = raw[1];
                    float tw   = raw[2];
                    float th   = raw[3];
                    float obj  = raw[4];

                    // 类别置信度
                    float max_cls = 0.0f;
                    int   cls_id  = 0;
                    for (int c = 0; c < nc; c++) {
                        float cls_score = raw[5 + c];
                        if (cls_score > max_cls) {
                            max_cls = cls_score;
                            cls_id  = c;
                        }
                    }

                    float conf = sigmoid(obj) * sigmoid(max_cls);
                    if (conf < conf_thres) continue;

                    // decode box (letterbox 坐标)
                    float anchor_w = sc.anchors[a * 2];
                    float anchor_h = sc.anchors[a * 2 + 1];

                    float cx = (sigmoid(tx) * 2.0f - 0.5f + (float)ax) * (float)stride;
                    float cy = (sigmoid(ty) * 2.0f - 0.5f + (float)ay) * (float)stride;
                    float w  = powf(sigmoid(tw) * 2.0f, 2.0f) * anchor_w;
                    float h  = powf(sigmoid(th) * 2.0f, 2.0f) * anchor_h;

                    float x1 = cx - w * 0.5f;
                    float y1 = cy - h * 0.5f;
                    float x2 = cx + w * 0.5f;
                    float y2 = cy + h * 0.5f;

                    // 映射回原图坐标 + 归一化
                    rescale_bbox(x1, y1, x2, y2, pad_left, pad_top, scale, img_w, img_h);

                    if (num_dets >= max_dets) break;
                    AlgoDetection& det = dets[num_dets];
                    memset(&det, 0, sizeof(det));
                    det.x1       = x1;
                    det.y1       = y1;
                    det.x2       = x2;
                    det.y2       = y2;
                    det.conf     = conf;
                    det.class_id = cls_id;
                    num_dets++;
                }
            }
        }
    }

    // NMS
    num_dets = nms(dets, num_dets, iou_thres);
    return num_dets;
}

// ============================================================================
// YOLOv8/YOLOv11 anchor-free 后处理 (decoded 输出)
//
// 假设模型已经在导出时做完 bbox decode:
//   - cx, cy, w, h 是模型输入分辨率下的像素值 (0 ~ model_w/h)
//   - class scores 已经经过 sigmoid (0 ~ 1), 直接就是概率
//
// 支持两种内存布局:
//   channel_first=1 (RKNN 常见): tensor [1, 4+nc, N]
//     row[c] = output[c * N + i]   (访问同一 anchor 的第 c 个通道)
//   channel_first=0 (Ultralytics ONNX 常见): tensor [1, N, 4+nc]
//     row[c] = output[i * (4+nc) + c]
// ============================================================================
int yolo8_postprocess(const float* output,
                      int n_anchors, int nc, int channel_first,
                      int model_w, int model_h,
                      int img_w, int img_h,
                      float pad_left, float pad_top, float scale,
                      float conf_thres, float iou_thres,
                      AlgoDetection* dets, int max_dets) {
    if (n_anchors <= 0 || nc <= 0) {
        fprintf(stderr, "[postprocess] yolo8: invalid shape n_anchors=%d nc=%d\n",
                n_anchors, nc);
        return 0;
    }

    int C = 4 + nc;
    int num_dets = 0;

    // 抽出 (i, c) 索引访问函数
    auto get = [&](int i, int c) -> float {
        return channel_first ? output[c * n_anchors + i] : output[i * C + c];
    };

    for (int i = 0; i < n_anchors && num_dets < max_dets; i++) {
        // 类别分数已 sigmoid, 直接取 max
        float max_cls = 0.0f;
        int   cls_id  = 0;
        for (int c = 0; c < nc; c++) {
            float s = get(i, 4 + c);
            if (s > max_cls) { max_cls = s; cls_id = c; }
        }
        if (max_cls < conf_thres) continue;

        // bbox 已 decode, 是模型输入分辨率下的像素坐标
        float cx = get(i, 0);
        float cy = get(i, 1);
        float w  = get(i, 2);
        float h  = get(i, 3);

        float x1 = cx - w * 0.5f;
        float y1 = cy - h * 0.5f;
        float x2 = cx + w * 0.5f;
        float y2 = cy + h * 0.5f;

        // letterbox → 原图 → 归一化
        rescale_bbox(x1, y1, x2, y2, pad_left, pad_top, scale, img_w, img_h);

        AlgoDetection& det = dets[num_dets];
        memset(&det, 0, sizeof(det));
        det.x1       = x1;
        det.y1       = y1;
        det.x2       = x2;
        det.y2       = y2;
        det.conf     = max_cls;
        det.class_id = cls_id;
        num_dets++;
    }

    num_dets = nms(dets, num_dets, iou_thres);
    return num_dets;
}
