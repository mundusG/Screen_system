#!/usr/bin/env python3
"""
test_bridge.py - 模拟 nn_server 输出，用于测试 nn_bridge 和显示系统

生成 nn_server 格式的检测消息，发布到本地 MQTT broker，
nn_bridge 会将其转换并转发到显示系统。

用法:
    python3 test_bridge.py [选项]

选项:
    --host HOST       MQTT broker 地址 (默认: 127.0.0.1)
    --port PORT       MQTT broker 端口 (默认: 1883)
    --topic TOPIC     发布 topic (默认: /dposter/58/cmd)
    --channels N      模拟的通道数 (默认: 4, chid 0 ~ N-1)
    --interval SEC    发送间隔秒数 (默认: 0.5)
    --duration SEC    运行时长秒数 (默认: 0, 无限)
    --det-min N       每帧最少检测数 (默认: 0)
    --det-max N       每帧最多检测数 (默认: 3)
"""

import json
import time
import random
import signal
import argparse
import paho.mqtt.client as mqtt


running = True


def signal_handler(signum, frame):
    global running
    running = False


def make_nn_output(det_min, det_max):
    """生成 nn_server 格式的 nn_output 数组"""
    count = random.randint(det_min, det_max)
    detections = []
    for _ in range(count):
        cx = random.uniform(0.1, 0.9)
        cy = random.uniform(0.1, 0.9)
        half_w = random.uniform(0.03, 0.15)
        half_h = random.uniform(0.05, 0.25)
        detections.append({
            'cid': 0,
            'gcid': 9216,
            'aid': 0,
            'class_name': '人员',
            'conf': round(random.uniform(0.5, 0.98), 3),
            'x1': round(max(0, cx - half_w), 4),
            'y1': round(max(0, cy - half_h), 4),
            'x2': round(min(1, cx + half_w), 4),
            'y2': round(min(1, cy + half_h), 4),
        })
    return detections


def make_message(chid, geid, seq, nn_output):
    """生成完整的 nn_server ch_detect_rsp 消息"""
    return {
        'cmd': 'ch_detect_rsp',
        'param': {
            'chid': chid,
            'ncid': 0,
            'gcids': [9216],
            'geid': geid,
            'seq': seq,
            'nn_output': nn_output,
            'location': '',
            'dwidth': 1920,
            'dheight': 1080,
            'filter_type': 0,
            'pub_freq': 5000,
        },
    }


def main():
    parser = argparse.ArgumentParser(description='模拟 nn_server 输出')
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=1883)
    parser.add_argument('--topic', default='/dposter/200/cmd')
    parser.add_argument('--channels', type=int, default=4)
    parser.add_argument('--interval', type=float, default=0.5)
    parser.add_argument('--duration', type=float, default=0)
    parser.add_argument('--det-min', type=int, default=0)
    parser.add_argument('--det-max', type=int, default=3)
    args = parser.parse_args()

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    client = mqtt.Client(f"test_bridge_{int(time.time())}")
    client.connect(args.host, args.port, 60)
    client.loop_start()

    print(f"模拟 nn_server: {args.channels} 通道, 间隔 {args.interval}s, "
          f"检测 {args.det_min}-{args.det_max} 个/帧")
    print(f"发布到: {args.host}:{args.port} topic={args.topic}")
    print("Ctrl+C 停止\n")

    seq_counters = {ch: 0 for ch in range(args.channels)}
    start_time = time.time()
    total_sent = 0

    while running:
        if args.duration > 0 and (time.time() - start_time) >= args.duration:
            break

        for chid in range(args.channels):
            if not running:
                break

            seq_counters[chid] += 1
            nn_output = make_nn_output(args.det_min, args.det_max)
            msg = make_message(chid, 200, seq_counters[chid], nn_output)
            client.publish(args.topic, json.dumps(msg), qos=0)
            total_sent += 1

        elapsed = time.time() - start_time
        if total_sent % (args.channels * 10) == 0:
            print(f"  已发送 {total_sent} 条消息, 运行 {elapsed:.0f}s")

        time.sleep(args.interval)

    client.loop_stop()
    client.disconnect()
    elapsed = time.time() - start_time
    print(f"\n完成: 发送 {total_sent} 条消息, 运行 {elapsed:.1f}s, "
          f"平均 {total_sent / elapsed:.1f} msg/s")


if __name__ == '__main__':
    main()
