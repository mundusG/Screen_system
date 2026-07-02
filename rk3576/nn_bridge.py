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
import os
import sys
import time
import signal
import threading
import base64
import numpy as np
import cv2
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

        # Per-bridge discovery topic: inference/bridge/{client_id}/channels
        client_id = self.config.get('client_id', 'nn_bridge')
        base_topic = self.config.get('discovery_topic', 'inference/bridge')
        self.discovery_topic = f'{base_topic}/{client_id}/channels'

        # chid → display slot mapping (optional, falls back to chid - 1)
        self.channel_map = self.config.get('channel_map', {})

        # Defect class IDs — embed a JPEG thumbnail when these classes are detected
        self.defect_class_ids = set(self.config.get('defect_class_ids', [1]))

        # Thread-safety: active_channels is read/written by both the
        # main thread and the paho MQTT callback thread.
        self._lock = threading.Lock()
        self._local_needs_reconnect = False
        self._remote_needs_reconnect = False

        # Auto-discovered channels: chid -> channel state
        self.active_channels = {}
        self.discovery_changed = True

    def _check_channel_timeout(self):
        now = time.time()
        with self._lock:
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
        # Prevent unbounded memory growth from internal message queue.
        # At ~120 msg/s, 500 slots = ~4 s buffer — tight enough to prevent
        # OOM during a prolonged broker stall, long enough for brief bursts.
        self.local_client.max_queued_messages = 500

        client_id = self.config.get('client_id', f"nn_bridge_pub_{int(time.time())}")
        self.remote_client = mqtt.Client(client_id)
        self.remote_client.on_connect = self._on_remote_connect
        self.remote_client.on_disconnect = self._on_remote_disconnect
        self.remote_client.max_queued_messages = 500
        self.remote_client.max_inflight_messages_set(50)

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

        logger.info("NN Bridge started, geid={}, topic={}, discovery={}, channel_map={}, "
                     "max_queued=500, effective_rate={}fps",
                     self.geid, self.subscribe_topic, self.discovery_topic,
                     self.channel_map if self.channel_map else "auto(chid-1)",
                     self.rate_limit if self.rate_limit > 0 else 10)

        loop_count = 0
        try:
            while self.running:
                time.sleep(5)
                if not self.running:
                    break

                loop_count += 1

                # Periodic MQTT reconnect (non-blocking — reconnect runs in
                # the paho network thread via loop_start).
                self._handle_reconnects()
                self._check_channel_timeout()

                # Print stats and publish discovery every stats_interval seconds
                if loop_count % max(1, self.stats_interval // 5) == 0:
                    self._print_stats()
                if self.discovery_changed:
                    self._publish_discovery()
                    with self._lock:
                        self.discovery_changed = False
        except KeyboardInterrupt:
            pass
        except Exception:
            logger.exception("Fatal error in main loop")
        finally:
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

    def _handle_reconnects(self):
        """Called from main loop to re-establish dropped MQTT connections."""
        if self._local_needs_reconnect and self.local_client:
            self._local_needs_reconnect = False
            logger.info("Attempting local broker reconnect...")
            try:
                self.local_client.reconnect()
            except (ConnectionRefusedError, OSError) as e:
                logger.warning("Local broker reconnect failed: {}", e)
                self._local_needs_reconnect = True

        if self._remote_needs_reconnect and self.remote_client:
            self._remote_needs_reconnect = False
            logger.info("Attempting remote broker reconnect...")
            try:
                self.remote_client.reconnect()
            except (ConnectionRefusedError, OSError) as e:
                logger.warning("Remote broker reconnect failed: {}", e)
                self._remote_needs_reconnect = True

    def _signal_handler(self, signum, frame):
        logger.info("Received signal {}, shutting down...", signum)
        self.running = False

    def _build_discovery_message(self):
        with self._lock:
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
            count = len(self.active_channels)
        return json.dumps({'channels': channels}), count

    def _publish_discovery(self):
        if not self.remote_client:
            return
        payload, count = self._build_discovery_message()
        self.remote_client.publish(self.discovery_topic, payload, qos=1, retain=True)
        logger.info("Published discovery: {} active channel(s)", count)

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
        self._local_needs_reconnect = False
        client.subscribe(self.subscribe_topic)
        logger.info("Subscribed to: {}", self.subscribe_topic)

    def _on_local_disconnect(self, client, userdata, rc):
        if rc != 0:
            logger.warning("Local broker disconnected unexpectedly, rc={}", rc)
            self._local_needs_reconnect = True
        else:
            logger.info("Local broker disconnected (clean)")

    def _on_remote_connect(self, client, userdata, flags, rc):
        if rc != 0:
            logger.error("Remote broker connect failed, rc={} ({})",
                         rc, mqtt.connack_string(rc))
            return
        logger.info("Connected to remote broker")
        self._remote_needs_reconnect = False

    def _on_remote_disconnect(self, client, userdata, rc):
        if rc != 0:
            logger.warning("Remote broker disconnected unexpectedly, rc={}", rc)
            self._remote_needs_reconnect = True
        else:
            logger.info("Remote broker disconnected (clean)")

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

        # Frame metadata for reading raw frame file (used when embedding JPEG)
        dwidth = param.get('dwidth', 0)
        dheight = param.get('dheight', 0)
        seq = param.get('seq', 0)

        now = time.time()

        # ── Single lock: register + state + rate limit + frame count ──
        effective_rate = self.rate_limit if self.rate_limit > 0 else 10
        min_interval = 1.0 / effective_rate

        with self._lock:
            if chid not in self.active_channels:
                camera_id = self.channel_map.get(str(chid), chid - 1)
                self.active_channels[chid] = {
                    'camera_id': camera_id,
                    'publish_topic': f'inference/camera/{camera_id}/detections',
                    'frame_count': 0,
                    'last_seen': now,
                    'stats': ChannelStats(),
                }
                self.discovery_changed = True
                logger.info("Auto-discovered channel chid={} -> camera_id={}", chid, camera_id)

            ch = self.active_channels[chid]
            ch['last_seen'] = now
            camera_id = ch['camera_id']
            pub_topic = ch['publish_topic']

            # Rate limit — check and set atomically
            last_time = self.last_publish_time.get(camera_id, 0)
            if now - last_time < min_interval:
                return
            self.last_publish_time[camera_id] = now

            ch['frame_count'] += 1
            frame_count = ch['frame_count']

        # ── Format conversion (no lock needed — only local vars) ──
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
        now_ms = int(now * 1000)
        output_msg = {
            'camera_id': camera_id,
            'timestamp': now_ms,
            'frame_index': frame_count,
            'normalized': True,
            'detections': detections,
            'inference_time_ms': 0,
        }

        # Embed a JPEG thumbnail when a defect class is detected. Draw
        # detection boxes on the frame so the display side can save it
        # directly without re-processing — same effect as dposter's alarm
        # snapshot but delivered inline via MQTT.
        has_defect = any(d.get('cid', 0) in self.defect_class_ids
                         for d in nn_output)
        if has_defect and dwidth > 0 and dheight > 0:
            raw_path = f"/mpp/mem/ch{chid}_{seq}.raw"
            try:
                if os.path.exists(raw_path):
                    raw_data = np.fromfile(raw_path, dtype=np.uint8)
                    raw_data = raw_data.reshape(dheight, dwidth, 3)
                    frame = cv2.cvtColor(raw_data, cv2.COLOR_RGB2BGR)

                    # Draw detection boxes on the raw frame (same as dposter output)
                    for det in nn_output:
                        x1 = int(det.get('x1', 0) * dwidth)
                        y1 = int(det.get('y1', 0) * dheight)
                        x2 = int(det.get('x2', 0) * dwidth)
                        y2 = int(det.get('y2', 0) * dheight)
                        conf = det.get('conf', 0)
                        cid = det.get('cid', 0)
                        cls_name = det.get('class_name', f'cls_{cid}')

                        color = (0, 0, 255) if cid in self.defect_class_ids else (0, 255, 0)
                        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)

                        label = f"{cls_name} {conf:.0%}"
                        (tw, th), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)
                        cv2.rectangle(frame, (x1, y1 - th - 4), (x1 + tw + 4, y1), color, -1)
                        cv2.putText(frame, label, (x1 + 2, y1 - 4),
                                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)

                    h, w = frame.shape[:2]
                    if w > 640:
                        new_h = int(h * 640 / w)
                        frame = cv2.resize(frame, (640, new_h),
                                          interpolation=cv2.INTER_AREA)
                    _, jpeg_buf = cv2.imencode('.jpg', frame,
                                               [cv2.IMWRITE_JPEG_QUALITY, 75])
                    output_msg['frame_jpeg'] = base64.b64encode(jpeg_buf).decode('ascii')
                else:
                    logger.debug("cam[{}] raw frame file not found: {}", camera_id, raw_path)
            except Exception:
                logger.exception("cam[{}] Failed to embed frame_jpeg", camera_id)

        # Skip publish if remote broker is disconnected — prevents
        # unbounded message queue buildup in paho's internal buffer.
        if not self.remote_client.is_connected():
            return
        self.remote_client.publish(pub_topic, json.dumps(output_msg), qos=self.qos)

        # ── Record stats (second lock — lightweight) ──
        src_ts = param.get('timestamp', 0)
        latency = (now_ms - src_ts) if src_ts > 0 else 0
        with self._lock:
            ch['stats'].record(latency)

        if frame_count % 100 == 1:
            logger.info("cam[{}] chid={} frame={} dets={} topic={}",
                        camera_id, chid, frame_count,
                        len(detections), pub_topic)

    @staticmethod
    def _read_rss_mb():
        """Read RSS from /proc/self/status. Returns MB, or -1 on failure."""
        try:
            with open('/proc/self/status') as f:
                for line in f:
                    if line.startswith('VmRSS:'):
                        return int(line.split()[1]) / 1024.0  # kB → MB
        except Exception:
            return -1.0

    def _print_stats(self):
        rss_mb = self._read_rss_mb()
        with self._lock:
            chids = sorted(self.active_channels)
            parts = []
            for chid in chids:
                ch = self.active_channels[chid]
                s = ch['stats'].report_and_reset()
                if s['count'] > 0:
                    parts.append(f"ch[{chid}]: {s['count']}msg {s['rate']}msg/s "
                                 f"lat={s['avg_latency_ms']}ms")
        if parts:
            logger.info("Stats | RSS={:.0f}MB | {}", rss_mb, " | ".join(parts))
        else:
            logger.info("Stats | RSS={:.0f}MB | no messages in last interval", rss_mb)

        if rss_mb > 500:
            logger.warning("HIGH MEMORY: RSS={:.0f}MB exceeds 500MB threshold", rss_mb)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <config.json>")
        print("See script header for config format.")
        sys.exit(1)

    config_path = sys.argv[1]

    # Log file path — prefer project-local log dir (created by deploy.sh).
    # Fall back to script dir, then stderr-only. /tmp is deliberately
    # excluded because permission conflicts with other processes.
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_log = os.path.join(os.path.dirname(script_dir), 'log', 'nn_bridge.log')
    log_paths = [project_log,
                 os.path.join(script_dir, 'nn_bridge.log')]

    logger.remove()
    logger.add(sys.stderr, level="INFO",
               format="{time:HH:mm:ss} | {level:<5} | {message}")

    log_ok = False
    for lp in log_paths:
        try:
            os.makedirs(os.path.dirname(lp), exist_ok=True)
            logger.add(lp, rotation="10 MB", retention="3 days")
            log_ok = True
            break
        except PermissionError:
            continue
    if not log_ok:
        logger.warning("Cannot create log file, logging to stderr only")

    bridge = NNBridge(config_path)
    bridge.start()


if __name__ == '__main__':
    main()
