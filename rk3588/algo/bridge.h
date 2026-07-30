/**
 * bridge.h — Go ↔ .so 检测数据接口
 *
 * 统一定义 AlgoDetection 结构体和能力位图,
 * 供 person_efence / yolo11_pose 等算法包使用。
 *
 * Go 侧通过 cgo 读取此头文件, 自动生成对应的 Go struct。
 */

#ifndef BRIDGE_H
#define BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 常量
// ============================================================================
#define ALGO_MAX_DETECTIONS 64
#define ALGO_MAX_KEYPOINTS   17

// ============================================================================
// 能力位图 — Go 根据此位图决定绘制内容
// ============================================================================
#define ALGO_CAP_BOXES       (1<<0)  // 检测框
#define ALGO_CAP_KEYPOINTS   (1<<1)  // 关键点 + 骨架连线
#define ALGO_CAP_ZONES       (1<<2)  // 区域叠加（zone_name）
#define ALGO_CAP_POSTURE     (1<<3)  // 姿态标签（posture_label）
#define ALGO_CAP_OVERLAP_PCT (1<<4)  // 重叠百分比（overlap_ratio）
#define ALGO_CAP_VIOLATION   (1<<5)  // 违规标记（is_violation）

// ============================================================================
// 检测结果结构体 — 所有坐标归一化 [0, 1]
// ============================================================================
typedef struct AlgoDetection {
    // ── v1 基础字段 ──
    float x1, y1, x2, y2;       // 归一化坐标 [0, 1]
    float conf;                  // 置信度 [0, 1]
    int   class_id;              // 类别 ID
    char  class_name[32];        // 类别名（如 "face" / "person"）

    // ── v2 可视化能力 ──
    int   capabilities;          // 能力位图（0 = 只画框，向后兼容 v1）

    // 关键点 + 骨架 (ALGO_CAP_KEYPOINTS)
    int   n_keypoints;
    float keypoints[ALGO_MAX_KEYPOINTS][3];       // [i][0]=x, [i][1]=y, [i][2]=conf (归一化)
    int   skeleton_edges[ALGO_MAX_KEYPOINTS][2];  // [i][0..1]=连线索引对
    int   n_skeleton_edges;

    // 违规标记 (ALGO_CAP_VIOLATION)
    int   is_violation;

    // 区域/禁区 (ALGO_CAP_ZONES)
    char  zone_name[64];

    // 重叠百分比 (ALGO_CAP_OVERLAP_PCT)
    float overlap_ratio;         // [0, 1]

    // 姿态标签 (ALGO_CAP_POSTURE)
    char  posture_label[16];     // 如 "SIT" / "STAND"
} AlgoDetection;

#ifdef __cplusplus
}
#endif

#endif // BRIDGE_H
