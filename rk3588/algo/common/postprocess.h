/**
 * postprocess.h — YOLO 后处理 (decode + NMS)
 *
 * 支持三种模型格式:
 *   model_type=0: YOLOv5  (3尺度 anchor-based)
 *   model_type=1: YOLOv8  (anchor-free, 1张量 [1,4+cls,8400])
 *   model_type=2: YOLOv11 (同 YOLOv8 anchor-free)
 *
 * 所有输出坐标归一化到 [0,1]
 */

#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include <vector>
#include <cstdint>

// 前向声明 (实际定义在 bridge.h)
struct AlgoDetection;

/** 单尺度 feature map 属性 */
struct YOLOScale {
    int   stride;       // 下采样倍率 (8/16/32)
    int   grid_w;       // feature map 宽度 (如 80)
    int   grid_h;       // feature map 高度
    int   n_anchors;    // 每 grid cell 的 anchor 数 (YOLOv5=3)
    float anchors[9];   // 该尺度的 anchor 宽高 (如 [10,13, 16,30, 33,23])
    int   n_outputs;    // 该尺度 tensor 中的元素数 (grid_w * grid_h * n_anchors * (5+nc))
};

/**
 * YOLOv5 anchor-based 后处理
 * @param outputs        3 个尺度的输出 tensor 数据
 * @param n_anchors      每尺度 anchor 数 (通常 3)
 * @param nc             类别数
 * @param model_w/h      模型输入宽高 (如 640x640)
 * @param img_w/h        原始图像宽高
 * @param pad_left/top   letterbox padding 偏移
 * @param scale          resize 缩放比例
 * @param conf_thres     置信度阈值
 * @param iou_thres      NMS IoU 阈值
 * @param dets           输出检测结果数组
 * @param max_dets       最大检测数
 * @return 实际检测数
 */
int yolo5_postprocess(const float* outputs[3],
                      const YOLOScale scales[3],
                      int nc,
                      int model_w, int model_h,
                      int img_w, int img_h,
                      float pad_left, float pad_top, float scale,
                      float conf_thres, float iou_thres,
                      AlgoDetection* dets, int max_dets);

/**
 * YOLOv8/YOLOv11 anchor-free 后处理
 * @param output         单个 tensor [1, 4+nc, N]
 * @param n_elements     tensor 总元素数 (4+nc)*N
 * @param nc             类别数
 * @param ...            其余参数同 yolo5_postprocess
 */
int yolo8_postprocess(const float* output,
                      int n_elements, int nc,
                      int model_w, int model_h,
                      int img_w, int img_h,
                      float pad_left, float pad_top, float scale,
                      float conf_thres, float iou_thres,
                      AlgoDetection* dets, int max_dets);

/**
 * NMS — 贪婪非极大值抑制 (按 class 独立执行)
 * @param dets    输入/输出检测结果 (原地排序, 被抑制的设 conf=0)
 * @param n       检测数量
 * @param iou_thres IoU 阈值
 * @return 保留的检测数量
 */
int nms(AlgoDetection* dets, int n, float iou_thres);

/**
 * 检测框 IoU 计算
 */
float bbox_iou(float x1_a, float y1_a, float x2_a, float y2_a,
               float x1_b, float y1_b, float x2_b, float y2_b);

/**
 * Sigmoid 激活函数
 */
inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

#endif // POSTPROCESS_H
