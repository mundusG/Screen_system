# 连续检测出目标的次数
CONTINUE_DETECT_NUM = 3
MAX_SEQ = 200

ALGO_IDCT = {
    # 区域入侵算法（m4）
    "m4": {
        "geid": 36,
        "gcid":	1024,
        "class_name":	"person",
        "engine_name": "区域入侵"
    },
    "m36": {
        "det_thres": 0.6,
        "geid": 36,
        "gcid":	9216,
        "class_name":	"人员脱岗",
        "engine_name": "脱岗串岗报警"
    }
}