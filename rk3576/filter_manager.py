#!/usr/bin/env python3
"""
filter_manager.py - 检测结果连续帧过滤器

对指定标签要求在连续 N 帧中出现后才转发到展示端，
过滤掉偶发/误检的单帧结果。不含过滤标签的消息正常通过。

配置文件格式 (filter_config.json):
{
    "filters": [
        {"label": "defect", "consecutive_count": 3}
    ]
}
"""

import json
import threading
from loguru import logger


class DetectionFilter:
    """基于连续帧计数的检测结果过滤器。"""

    def __init__(self, config_path=None):
        """
        Args:
            config_path: 过滤配置文件路径，None 表示不过滤
        """
        self._filters = {}   # label -> consecutive_count
        self._lock = threading.Lock()
        # per-channel per-label counter: {chid: {label: count}}
        self._counters = {}

        if config_path:
            self._load_config(config_path)

    @property
    def enabled(self):
        return len(self._filters) > 0

    def _load_config(self, config_path):
        try:
            with open(config_path, 'r') as f:
                config = json.load(f)
            for item in config.get('filters', []):
                label = item.get('label', '').strip()
                count = item.get('consecutive_count', 1)
                if label and count > 1:
                    self._filters[label] = count
                    logger.info(
                        "Filter enabled: label='{}' requires {} consecutive frames",
                        label, count)
            if not self._filters:
                logger.info("No valid filters in config, filtering disabled")
        except FileNotFoundError:
            logger.warning("Filter config not found: {}, filtering disabled", config_path)
        except (json.JSONDecodeError, KeyError) as e:
            logger.error("Invalid filter config: {}", e)

    def apply(self, detections, chid):
        """
        对检测结果应用连续帧过滤。

        对于 self._filters 中配置的标签：
        - 当前帧包含该标签 → 计数器+1
        - 当前帧不含该标签 → 计数器归零
        - 计数器 < 阈值 → 移除此标签的检测结果
        - 计数器 >= 阈值 → 正常通过

        不在 self._filters 中的标签始终直接通过。

        Args:
            detections: list of dict, 每个 dict 含 class_name 字段
            chid: 通道 ID

        Returns:
            过滤后的检测列表（可能为空）
        """
        if not self._filters:
            return detections

        with self._lock:
            if chid not in self._counters:
                self._counters[chid] = {}
            counters = self._counters[chid]

            # 找出当前帧中出现的被监控标签
            present_labels = set()
            for det in detections:
                label = det.get('class_name', '')
                if label in self._filters:
                    present_labels.add(label)

            # 更新计数器：出现则+1，不出现则归零
            for label in self._filters:
                if label in present_labels:
                    counters[label] = counters.get(label, 0) + 1
                else:
                    if counters.get(label, 0) > 0:
                        counters[label] = 0

            # 过滤：移除计数器未达阈值的标签对应的检测
            filtered = []
            filtered_count = 0
            for det in detections:
                label = det.get('class_name', '')
                if label in self._filters and counters.get(label, 0) < self._filters[label]:
                    filtered_count += 1
                    continue
                filtered.append(det)

            if filtered_count > 0:
                active = {k: v for k, v in counters.items() if v > 0}
                logger.debug("chid={}: filtered {} detection(s), counters={}",
                             chid, filtered_count, active)

            return filtered
