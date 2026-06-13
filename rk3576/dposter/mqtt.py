import paho.mqtt.client as mqtt
import threading
from multiprocessing import Queue
import time

HOST = "127.0.0.1"  
# HOST = "192.168.2.160"  

PORT = 1883
queue_recv = Queue(300)
queue_send = Queue(300)

global TopicSub
global TopicPub
TopicSub="/dposter/cmd"
TopicPub="/smart_gw/cmd"

def MqttGetQueueLen():
    rlen = queue_recv.qsize()
    slen = queue_send.qsize()
    return rlen, slen

def on_connect(mqtt, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    global TopicSub
    print("mqtt-sub:",TopicSub)
    mqtt.subscribe(TopicSub) 

def on_message(mqtt, userdata, msg):
    queue_recv.put(str(msg.payload.decode('utf-8')))

def on_subscribe(mqtt, userdata, mid, granted_qos):
    print("On Subscribed: qos = %d" % granted_qos)

def on_disconnect(mqtt, userdata, rc):
    if rc != 0:
        print("Unexpected disconnection %s" % rc)

def MqttInit(topic_sub,topic_pub, client_id):
    mqttc = mqtt.Client(client_id)
    mqttc.username_pw_set(client_id, "123456")
    global TopicSub
    global TopicPub
    TopicSub = topic_sub
    TopicPub = topic_pub
    mqttc.on_connect = on_connect
    mqttc.on_message = on_message
    mqttc.on_subscribe = on_subscribe
    mqttc.on_disconnect = on_disconnect
    mqttc.connect(HOST, PORT, 60)
    
    return mqttc,queue_recv,queue_send

class MqttSubscriber(threading.Thread):
    def __init__(self, threadID, name, mqttc):
        threading.Thread.__init__(self)
        self.threadID = threadID
        self.name = name
        self.mqttc = mqttc
        # self.queue = process_queue
        print("init subscriber")
    def run(self):
        print("start subscriber")
        self.mqttc.loop_forever()

class MqttPublisher(threading.Thread):
    def __init__(self, threadID, name, mqttc,publish_queue):
        threading.Thread.__init__(self)
        self.threadID = threadID
        self.name = name
        self.mqttc = mqttc
        self.queue_send = publish_queue
        print("init publish")

    def run(self):
        print("start publish")
        global TopicPub
        while True:
            param = self.queue_send.get()
            # print(param)
            # self.mqttc.publish(param["topic"], payload=param["payload"], qos=0)   
            self.mqttc.publish(TopicPub, payload=param["payload"], qos=0)     
