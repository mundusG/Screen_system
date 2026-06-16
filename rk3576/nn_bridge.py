#!/usr/bin/env python3
"""
nn_bridge.py - 桥接 nn_server 推理输出到显示系统

订阅 nn_server 的本地 MQTT 输出 (/dposter/{geid}/cmd)，
将检测结果转换为显示系统的归一化坐标格式，
发布到 inference/camera/{chid}/detections。

自动发现模式: 无需预配置通道列表，从 MQTT 消息流中自动发现活跃通道，
通道超时无数据自动标记离线。

用法:
    python3 nn_bridge.py config.json

配置文件示例 (config.json):
{
    "mqtt_local": {"host": "127.0.0.1", "port": 1883},
    "mqtt_remote": {"host": "192.168.1.100", "port": 1883},
    "client_id": "nn_bridge_rk3576",
    "geid": 200,
    "stats_interval": 30,
    "rate_limit": 0,
    "channel_timeout": 30,
    "preview": {"host": "192.168.1.100", "port": 5544},
    "discovery_topic": "inference/bridge/channels"
}
"""

import json
import sys
import time
import signal
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

        self.geid = self.config.get('geid', 200)
        self.subscribe_topic = f"/dposter/{self.geid}/cmd"

        self.qos = self.config.get('mqtt_remote', {}).get('qos', 0)
        self.stats_interval = self.config.get('stats_interval', 30)
        self.rate_limit = self.config.get('rate_limit', 0)
        self.channel_timeout = self.config.get('channel_timeout', 30)
        self.last_publish_time = {}

        preview_cfg = self.config.get('preview', {})
        self.preview_host = preview_cfg.get('host', '127.0.0.1')
        self.preview_port = preview_cfg.get('port', 5544)
        self.discovery_topic = self.config.get('discovery_topic', 'inference/bridge/channels')

        # Auto-discovered channels: chid -> channel state
        self.active_channels = {}
        self.discovery_changed = True

    def _register_channel(self, chid):
        self.active_channels[chid] = {
            'camera_id': chid,
            'publish_topic': f'inference/camera/{chid}/detections',
            'frame_count': 0,
            'last_seen': time.time(),
            'stats': ChannelStats(),
        }
        self.discovery_changed = True
        logger.info("Auto-discovered channel chid={}", chid)

    def _check_channel_timeout(self):
        now = time.time()
        timed_out = [chid for chid, ch in self.active_channels.items()
                     if now - ch['last_seen'] > self.channel_timeout]
        for chid in timed_out:
            logger.info("Channel chid={} timed out, removing", chid)
            del self.active_channels[chid]
            self.discovery_changed = True

    def start(self):
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)

        local_cfg = self.config['mqtt_local']
        remote_cfg = self.config['mqtt_remote']

        self.local_client = mqtt.Client(f"nn_bridge_sub_{int(time.time())}")
        self.local_client.on_connect = self._on_local_connect
        self.local_client.on_message = self._on_local_message
        self.local_client.on_disconnect = self._on_local_disconnect

        client_id = self.config.get('client_id', f"nn_bridge_pub_{int(time.time())}")
        self.remote_client = mqtt.Client(client_id)
        self.remote_client.on_connect = self._on_remote_connect
        self.remote_client.on_disconnect = self._on_remote_disconnect

        lwt_topic = "inference/bridge/status"
        lwt_payload = json.dumps({"status": "offline", "client_id": client_id})
        self.remote_client.will_set(lwt_topic, lwt_payload, qos=1, retain=True)

        if remote_cfg.get('username'):
            self.remote_client.username_pw_set(
                remote_cfg['username'], remote_cfg.get('password', ''))

        self._connect_with_retry(
            self.local_client, local_cfg['host'], local_cfg['port'], "local")
        self._connect_with_retry(
            self.remote_client, remote_cfg['host'], remote_cfg['port'], "remote")

        online_payload = json.dumps({
            "status": "online",
            "client_id": client_id,
        })
        self.remote_client.publish(lwt_topic, online_payload, qos=1, retain=True)

        self.local_client.loop_start()
        self.remote_client.loop_start()

        logger.info("NN Bridge started (auto-discovery), geid={}, topic={}, stats every {}s",
                     self.geid, self.subscribe_topic, self.stats_interval)

        try:
            while self.running:
                time.sleep(self.stats_interval)
                if not self.running:
                    break
                self._check_channel_timeout()
                self._print_stats()
                if self.discovery_changed:
                    self._publish_discovery()
                    self.discovery_changed = False
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

    def _build_discovery_message(self):
        channels = []
        for chid in sorted(self.active_channels):
            ch = self.active_channels[chid]
            preview_url = f"rtsp://{self.preview_host}:{self.preview_port}/preview/{chid}"
            channels.append({
                'camera_id': ch['camera_id'],
                'chid': chid,
                'name': f"camera_{chid}",
                'preview_url': preview_url,
                'inference_topic': ch['publish_topic'],
            })
        return json.dumps({'channels': channels})

    def _publish_discovery(self):
        if not self.remote_client:
            return
        payload = self._build_discovery_message()
        self.remote_client.publish(self.discovery_topic, payload, qos=1, retain=True)
        logger.info("Published discovery: {} active channel(s)", len(self.active_channels))

    def _shutdown(self):
        logger.info("Shutting down...")
        self._print_stats()

        if self.remote_client:
            client_id = self.config.get('client_id', 'nn_bridge')
            offline_payload = json.dumps({"status": "offline", "client_id": client_id})
            try:
                self.remote_client.publish(
                    "inference/bridge/status", offline_payload, qos=1, retain=True)
                # Clear discovery on shutdown
                self.remote_client.publish(
                    self.discovery_topic, json.dumps({'channels': []}), qos=1, retain=True)
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
        client.subscribe(self.subscribe_topic)
        logger.info("Subscribed to: {}", self.subscribe_topic)

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
        if chid < 0:
            return

        # Auto-register new channel
        if chid not in self.active_channels:
            self._register_channel(chid)

        ch = self.active_channels[chid]
        ch['last_seen'] = time.time()
        camera_id = ch['camera_id']

        # Rate limiting
        if self.rate_limit > 0:
            now = time.time()
            min_interval = 1.0 / self.rate_limit
            last_time = self.last_publish_time.get(camera_id, 0)
            if now - last_time < min_interval:
                return
            self.last_publish_time[camera_id] = now

        ch['frame_count'] += 1

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
            'frame_index': ch['frame_count'],
            'normalized': True,
            'detections': detections,
            'inference_time_ms': 0,
        }

        pub_topic = ch['publish_topic']
        self.remote_client.publish(pub_topic, json.dumps(output_msg), qos=self.qos)

        # Record stats
        src_ts = param.get('timestamp', 0)
        latency = (now_ms - src_ts) if src_ts > 0 else 0
        ch['stats'].record(latency)

        if ch['frame_count'] % 100 == 1:
            logger.info("cam[{}] chid={} frame={} dets={} topic={}",
                        camera_id, chid, ch['frame_count'],
                        len(detections), pub_topic)

    def _print_stats(self):
        parts = []
        for chid in sorted(self.active_channels):
            ch = self.active_channels[chid]
            s = ch['stats'].report_and_reset()
            if s['count'] > 0:
                parts.append(f"ch[{chid}]: {s['count']}msg {s['rate']}msg/s "
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
