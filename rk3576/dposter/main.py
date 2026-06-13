import mqtt
import process
import sys
import json
import os
import time
from loguru import logger

def getBaseConf(base_file):
    with open(base_file, "r") as f:
        baseConf = json.load(f)
    #print(baseConf)
    return baseConf

# def getExtendConf(extend_file):
#     with open(extend_file,"r") as f:
#         exConf = json.load(f)
#     #print(exConf)
#     return exConf

#print(sys.argv)

topic_sub="/dposter/cmd"
topic_pub="/smart_gw/cmd"
geid = 0
need_draw=True
need_tracker = False

extend_dic = {}
for i in range(10):
    sub_cls=[False,False]
    extend_dic[i] = sub_cls

if len(sys.argv) == 2:
    base_file=sys.argv[1]
    # extend_file=sys.argv[2]
    
    base_conf = getBaseConf(base_file)
    # print(base_conf)
    geid = base_conf['geid']
    topic_sub = "/dposter/%d/cmd" % (geid)
    topic_pub = base_conf['snd_topic']
    need_draw = base_conf['is_draw']
    pic_cnt = base_conf['pic_cnt']
    if 'track' in base_conf:
        need_tracker = base_conf['track']

    LogFile = "../../log/dposter_m%d.log" % (geid)
    logger.remove(handler_id=None)
    logger.add(LogFile,rotation="20 MB",retention='10 days',compression='zip')

    # print(LogFile)

else:
 ##   print("use default conf...")
     os._exit(0)


#print("topic_sub:",topic_sub)
#print("topic_pub:",topic_pub)
#print("need_draw:",need_draw)
#print("extend:",extend_dic)
try:
    datatime = time.time()
    client_id = "dposter_%d_%s" % (geid, str(int(datatime)))

    # print(client_id, topic_sub)
    mqttc,queue_recv,queue_send = mqtt.MqttInit(topic_sub,topic_pub, client_id)

    s = mqtt.MqttSubscriber(1,"subscriber",mqttc)
    s.start()

    p = mqtt.MqttPublisher(2, "publisher",mqttc,queue_send)
    p.start()

    loop = process.Mainloop(3,"mainloop",queue_recv,queue_send,need_draw,need_tracker,pic_cnt)
    # loop.start()

    loop.run()
    s.join()
    p.join()

except BaseException as error:
    # print("catch.")
    # print(str(error))
    # logger.info(error)
    logger.exception(error)
    # raise
    # print("exit for main-thread")
    os._exit(1)
#     # print(msg)

os._exit(0)