#!/usr/bin/env python3
"""
nn_bridge.py - 桥接 nn_server 推理输出到显示系统

订阅 nn_server 的本地 MQTT 输出 (/dposter/{geid}/cmd)，
将检测结果转换为显示系统的归一化坐标格式，
发布到 inference/camera/{camera_id}/detections。

用法:
    python3 nn_bridge.py config.json

配置文件示例 (config.json):
{
    "mqtt_local": {
        "host": "127.0.0.1",
        "port": 1883
    },
    "mqtt_remote": {
        "host": "192.168.1.100",
        "port": 1883,
        "username": "",
        "password": ""
    },
    "channels": [
        {
            "camera_id": 0,
            "geid": 58,
            "subscribe_topic": "/dposter/58/cmd",
            "publish_topic": "inference/camera/0/detections"
        }
    ]
}
"""

import json
import sys
import time
import threading
import paho.mqtt.client as mqtt
from loguru import logger


class NNBridge:
    def __init__(self, config_path):
        with open(config_path, 'r') as f:
            self.config = json.load(f)

        self.local_client = None
        self.remote_client = None
        self.channel_map = {}  # subscribe_topic -> channel config
        self.frame_counters = {}  # camera_id -> frame count

        for ch in self.config['channels']:
            self.channel_map[ch['subscribe_topic']] = ch
            self.frame_counters[ch['camera_id']] = 0

    def start(self):
        # Setup local MQTT client (subscribes to nn_server output)
        local_cfg = self.config['mqtt_local']
        self.local_client = mqtt.Client(f"nn_bridge_sub_{int(time.time())}")
        self.local_client.on_connect = self._on_local_connect
        self.local_client.on_message = self._on_local_message
        self.local_client.on_disconnect = self._on_local_disconnect

        # Setup remote MQTT client (publishes to display system)
        remote_cfg = self.config['mqtt_remote']
        client_id = self.config.get('client_id', f"nn_bridge_pub_{int(time.time())}")
        self.remote_client = mqtt.Client(client_id)
        self.remote_client.on_connect = self._on_remote_connect
        self.remote_client.on_disconnect = self._on_remote_disconnect

        if remote_cfg.get('username'):
            self.remote_client.username_pw_set(
                remote_cfg['username'], remote_cfg.get('password', ''))

        # Connect
        logger.info(f"Connecting to local broker: {local_cfg['host']}:{local_cfg['port']}")
        self.local_client.connect(local_cfg['host'], local_cfg['port'], 60)

        logger.info(f"Connecting to remote broker: {remote_cfg['host']}:{remote_cfg['port']}")
        self.remote_client.connect(remote_cfg['host'], remote_cfg['port'], 60)

        # Start loops in separate threads
        self.local_client.loop_start()
        self.remote_client.loop_start()

        logger.info("NN Bridge started. Press Ctrl+C to stop.")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            logger.info("Shutting down...")
            self.local_client.loop_stop()
            self.remote_client.loop_stop()
            self.local_client.disconnect()
            self.remote_client.disconnect()

    def _on_local_connect(self, client, userdata, flags, rc):
        logger.info(f"Connected to local broker, rc={rc}")
        for topic in self.channel_map:
            client.subscribe(topic)
            logger.info(f"Subscribed to: {topic}")

    def _on_local_disconnect(self, client, userdata, rc):
        if rc != 0:
            logger.warning(f"Local broker disconnected unexpectedly, rc={rc}")

    def _on_remote_connect(self, client, userdata, flags, rc):
        logger.info(f"Connected to remote broker, rc={rc}")

    def _on_remote_disconnect(self, client, userdata, rc):
        if rc != 0:
            logger.warning(f"Remote broker disconnected unexpectedly, rc={rc}")

    def _on_local_message(self, client, userdata, msg):
        """Process nn_server detection message and republish in display format."""
        try:
            payload = json.loads(msg.payload.decode('utf-8'))
        except (json.JSONDecodeError, UnicodeDecodeError) as e:
            logger.error(f"Failed to decode message: {e}")
            return

        # Find channel config for this topic
        channel = self.channel_map.get(msg.topic)
        if channel is None:
            return

        # Parse nn_server format
        if payload.get('cmd') != 'ch_detect_rsp':
            return

        param = payload.get('param', {})
        nn_output = param.get('nn_output', [])
        camera_id = channel['camera_id']

        self.frame_counters[camera_id] += 1

        # Convert nn_output to our normalized format
        # nn_server bbox format: x1, y1, x2, y2 (normalized 0~1)
        detections = []
        for det in nn_output:
            x1 = det.get('x1', 0)
            y1 = det.get('y1', 0)
            x2 = det.get('x2', 0)
            y2 = det.get('y2', 0)
            conf = det.get('conf', 0)
            class_id = det.get('cid', 0)

            # Convert x1,y1,x2,y2 to cx,cy,w,h (all already normalized)
            cx = (x1 + x2) / 2.0
            cy = (y1 + y2) / 2.0
            w = x2 - x1
            h = y2 - y1

            if w <= 0 or h <= 0:
                continue

            detections.append({
                'class_id': class_id,
                'confidence': conf,
                'bbox': {
                    'cx': round(cx, 4),
                    'cy': round(cy, 4),
                    'w': round(w, 4),
                    'h': round(h, 4)
                }
            })

        # Build output message in display system format
        output_msg = {
            'camera_id': camera_id,
            'timestamp': int(time.time() * 1000),
            'frame_index': self.frame_counters[camera_id],
            'normalized': True,
            'detections': detections,
            'inference_time_ms': 0  # not available from nn_server
        }

        # Publish to display system topic
        pub_topic = channel['publish_topic']
        self.remote_client.publish(pub_topic, json.dumps(output_msg), qos=0)

        if self.frame_counters[camera_id] % 50 == 1:
            logger.info(f"cam[{camera_id}] frame={self.frame_counters[camera_id]} "
                       f"detections={len(detections)} topic={pub_topic}")


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <config.json>")
        print("See script header for config format.")
        sys.exit(1)

    config_path = sys.argv[1]
    logger.remove()
    logger.add(sys.stderr, level="INFO")
    logger.add("/tmp/nn_bridge.log", rotation="10 MB", retention="3 days")

    bridge = NNBridge(config_path)
    bridge.start()


if __name__ == '__main__':
    main()
