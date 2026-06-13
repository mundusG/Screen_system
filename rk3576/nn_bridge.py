#!/usr/bin/env python3
"""
nn_bridge.py - 桥接 nn_server 推理输出到显示系统

订阅 nn_server 的本地 MQTT 输出 (/dposter/{geid}/cmd)，
将检测结果转换为显示系统的归一化坐标格式，
发布到 inference/camera/{camera_id}/detections。

支持多通道: 单 nn_server 实例通过 param.chid 区分不同摄像头通道，
bridge 按 chid 路由到不同 camera_id。

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
        "password": "",
        "qos": 0
    },
    "client_id": "nn_bridge_rk3576",
    "stats_interval": 30,
    "rate_limit": 0,
    "channels": [
        {
            "camera_id": 0,
            "chid": 0,
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
import signal
import threading
import paho.mqtt.client as mqtt
from loguru import logger


class ChannelStats:
    """Per-channel statistics tracker."""

    def __init__(self):
        self.msg_count = 0
        self.last_reset_time = time.time()
        self.total_latency_ms = 0.0
        self.latency_samples = 0

    def record(self, latency_ms=0.0):
        self.msg_count += 1
        if latency_ms > 0:
            self.total_latency_ms += latency_ms
            self.latency_samples += 1

    def report_and_reset(self):
        now = time.time()
        elapsed = now - self.last_reset_time
        rate = self.msg_count / elapsed if elapsed > 0 else 0
        avg_lat = (self.total_latency_ms / self.latency_samples
                   if self.latency_samples > 0 else 0)
        result = {
            'count': self.msg_count,
            'rate': round(rate, 2),
            'avg_latency_ms': round(avg_lat, 1),
            'elapsed_s': round(elapsed, 1),
        }
        self.msg_count = 0
        self.last_reset_time = now
        self.total_latency_ms = 0.0
        self.latency_samples = 0
        return result


class NNBridge:
    def __init__(self, config_path):
        with open(config_path, 'r') as f:
            self.config = json.load(f)

        self.local_client = None
        self.remote_client = None
        self.running = True

        # topic-based routing (legacy compat)
        self.topic_channels = {}  # subscribe_topic -> [channel configs]
        # chid-based routing (primary)
        self.chid_map = {}  # chid -> channel config
        self.frame_counters = {}  # camera_id -> frame count
        self.channel_stats = {}  # camera_id -> ChannelStats
        # deduplicate subscriptions
        self.subscribe_topics = set()

        self.qos = self.config.get('mqtt_remote', {}).get('qos', 0)
        self.stats_interval = self.config.get('stats_interval', 30)
        self.rate_limit = self.config.get('rate_limit', 0)
        self.last_publish_time = {}

        for ch in self.config['channels']:
            cam_id = ch['camera_id']
            self.frame_counters[cam_id] = 0
            self.channel_stats[cam_id] = ChannelStats()
            self.subscribe_topics.add(ch['subscribe_topic'])

            if 'chid' in ch:
                self.chid_map[ch['chid']] = ch

            topic = ch['subscribe_topic']
            if topic not in self.topic_channels:
                self.topic_channels[topic] = []
            self.topic_channels[topic].append(ch)

    def start(self):
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)

        local_cfg = self.config['mqtt_local']
        remote_cfg = self.config['mqtt_remote']

        # Setup local MQTT client
        self.local_client = mqtt.Client(f"nn_bridge_sub_{int(time.time())}")
        self.local_client.on_connect = self._on_local_connect
        self.local_client.on_message = self._on_local_message
        self.local_client.on_disconnect = self._on_local_disconnect

        # Setup remote MQTT client with LWT
        client_id = self.config.get('client_id', f"nn_bridge_pub_{int(time.time())}")
        self.remote_client = mqtt.Client(client_id)
        self.remote_client.on_connect = self._on_remote_connect
        self.remote_client.on_disconnect = self._on_remote_disconnect

        # LWT: notify display system when bridge goes offline
        lwt_topic = "inference/bridge/status"
        lwt_payload = json.dumps({"status": "offline", "client_id": client_id})
        self.remote_client.will_set(lwt_topic, lwt_payload, qos=1, retain=True)

        if remote_cfg.get('username'):
            self.remote_client.username_pw_set(
                remote_cfg['username'], remote_cfg.get('password', ''))

        # Connect with retry
        self._connect_with_retry(
            self.local_client, local_cfg['host'], local_cfg['port'], "local")
        self._connect_with_retry(
            self.remote_client, remote_cfg['host'], remote_cfg['port'], "remote")

        # Publish online status
        online_payload = json.dumps({
            "status": "online",
            "client_id": client_id,
            "channels": len(self.config['channels']),
        })
        self.remote_client.publish(lwt_topic, online_payload, qos=1, retain=True)

        # Start MQTT loops
        self.local_client.loop_start()
        self.remote_client.loop_start()

        logger.info("NN Bridge started, {} channel(s), stats every {}s",
                     len(self.config['channels']), self.stats_interval)

        # Main loop with periodic stats
        try:
            while self.running:
                time.sleep(self.stats_interval)
                if self.running:
                    self._print_stats()
        except KeyboardInterrupt:
            pass

        self._shutdown()

    def _connect_with_retry(self, client, host, port, label):
        delay = 1
        max_delay = 30
        while self.running:
            try:
                logger.info("Connecting to {} broker: {}:{}", label, host, port)
                client.connect(host, port, 60)
                return
            except (ConnectionRefusedError, OSError) as e:
                logger.warning("{} broker connect failed: {}, retry in {}s",
                               label, e, delay)
                time.sleep(delay)
                delay = min(delay * 2, max_delay)

    def _signal_handler(self, signum, frame):
        logger.info("Received signal {}, shutting down...", signum)
        self.running = False

    def _shutdown(self):
        logger.info("Shutting down...")
        self._print_stats()

        if self.remote_client:
            client_id = self.config.get('client_id', 'nn_bridge')
            offline_payload = json.dumps({"status": "offline", "client_id": client_id})
            try:
                self.remote_client.publish(
                    "inference/bridge/status", offline_payload, qos=1, retain=True)
                time.sleep(0.2)
            except Exception:
                pass

        if self.local_client:
            self.local_client.loop_stop()
            self.local_client.disconnect()
        if self.remote_client:
            self.remote_client.loop_stop()
            self.remote_client.disconnect()
        logger.info("Shutdown complete")

    def _on_local_connect(self, client, userdata, flags, rc):
        if rc != 0:
            logger.error("Local broker connect failed, rc={} ({})",
                         rc, mqtt.connack_string(rc))
            return
        logger.info("Connected to local broker")
        for topic in self.subscribe_topics:
            client.subscribe(topic)
            logger.info("Subscribed to: {}", topic)

    def _on_local_disconnect(self, client, userdata, rc):
        if rc != 0:
            logger.warning("Local broker disconnected unexpectedly, rc={}", rc)

    def _on_remote_connect(self, client, userdata, flags, rc):
        if rc != 0:
            logger.error("Remote broker connect failed, rc={} ({})",
                         rc, mqtt.connack_string(rc))
            return
        logger.info("Connected to remote broker")

    def _on_remote_disconnect(self, client, userdata, rc):
        if rc != 0:
            logger.warning("Remote broker disconnected unexpectedly, rc={}", rc)

    def _on_local_message(self, client, userdata, msg):
        """Process nn_server detection message and republish in display format."""
        try:
            payload = json.loads(msg.payload.decode('utf-8'))
        except (json.JSONDecodeError, UnicodeDecodeError) as e:
            logger.error("Failed to decode message: {}", e)
            return

        if payload.get('cmd') != 'ch_detect_rsp':
            return

        param = payload.get('param', {})
        nn_output = param.get('nn_output', [])
        chid = param.get('chid', -1)

        # Route by chid (primary) or by topic (fallback)
        channel = self.chid_map.get(chid)
        if channel is None:
            topic_channels = self.topic_channels.get(msg.topic, [])
            if len(topic_channels) == 1:
                channel = topic_channels[0]
            elif len(topic_channels) > 1:
                logger.warning("chid {} not in chid_map, topic {} has {} channels, skipping",
                               chid, msg.topic, len(topic_channels))
                return
            else:
                return

        camera_id = channel['camera_id']

        # Rate limiting
        if self.rate_limit > 0:
            now = time.time()
            min_interval = 1.0 / self.rate_limit
            last_time = self.last_publish_time.get(camera_id, 0)
            if now - last_time < min_interval:
                return
            self.last_publish_time[camera_id] = now

        self.frame_counters[camera_id] += 1

        # Convert nn_output to normalized format
        detections = []
        for det in nn_output:
            x1 = det.get('x1', 0)
            y1 = det.get('y1', 0)
            x2 = det.get('x2', 0)
            y2 = det.get('y2', 0)
            conf = det.get('conf', 0)
            class_id = det.get('cid', 0)
            class_name = det.get('class_name', '')

            cx = (x1 + x2) / 2.0
            cy = (y1 + y2) / 2.0
            w = x2 - x1
            h = y2 - y1

            if w <= 0 or h <= 0:
                continue

            det_out = {
                'class_id': class_id,
                'confidence': conf,
                'bbox': {
                    'cx': round(cx, 4),
                    'cy': round(cy, 4),
                    'w': round(w, 4),
                    'h': round(h, 4),
                },
            }
            if class_name:
                det_out['class_name'] = class_name
            detections.append(det_out)

        # Build output message
        now_ms = int(time.time() * 1000)
        output_msg = {
            'camera_id': camera_id,
            'timestamp': now_ms,
            'frame_index': self.frame_counters[camera_id],
            'normalized': True,
            'detections': detections,
            'inference_time_ms': 0,
        }

        # Publish
        pub_topic = channel['publish_topic']
        self.remote_client.publish(pub_topic, json.dumps(output_msg), qos=self.qos)

        # Record stats
        src_ts = param.get('timestamp', 0)
        latency = (now_ms - src_ts) if src_ts > 0 else 0
        self.channel_stats[camera_id].record(latency)

        if self.frame_counters[camera_id] % 100 == 1:
            logger.info("cam[{}] chid={} frame={} dets={} topic={}",
                        camera_id, chid, self.frame_counters[camera_id],
                        len(detections), pub_topic)

    def _print_stats(self):
        parts = []
        for cam_id in sorted(self.channel_stats):
            s = self.channel_stats[cam_id].report_and_reset()
            if s['count'] > 0:
                parts.append(f"cam[{cam_id}]: {s['count']}msg {s['rate']}msg/s "
                             f"lat={s['avg_latency_ms']}ms")
        if parts:
            logger.info("Stats | {}", " | ".join(parts))
        else:
            logger.info("Stats | no messages in last interval")


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <config.json>")
        print("See script header for config format.")
        sys.exit(1)

    config_path = sys.argv[1]
    logger.remove()
    logger.add(sys.stderr, level="INFO",
               format="{time:HH:mm:ss} | {level:<5} | {message}")
    logger.add("/tmp/nn_bridge.log", rotation="10 MB", retention="3 days")

    bridge = NNBridge(config_path)
    bridge.start()


if __name__ == '__main__':
    main()
