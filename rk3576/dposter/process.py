# from asyncio.windows_events import NULL
import json
import time
import threading
import base64
import cv2 
import utils
import os
# import tracker
import numpy as np
import mqtt
import handler

from config import ALGO_IDCT

from loguru import logger

class Dmsg:
    def __init__(self):
        self.img_base64 = ""
        self.chid = 0
        self.ncid = 0
        self.gcids = []
        self.geid = 0
        self.nn_output = []
        self.location = ""
        self.filter_type = 0
        self.pub_freq = 5000
        self.seq = 0
        self.dwidth = 0
        self.dheight = 0
        self.ip = ""
        self.surl = ""
        self.sn = ""
        self.sn32 = ""
        self.desc = ""

class PNum:
    def gfile(self, chid, geid):
        fname = (".ch%d_m%d.pnum" % (chid, geid))
        abs_fname = "/userdata/mpp/disk/" + fname
        return abs_fname
        
    def save(self, chid, geid, pnum):
        data = {}
        data["pnum"] = pnum
        msg = json.dumps(data)
        fname = self.gfile(chid, geid)
        with open(fname,'w') as f:
            f.write(msg)

    def load(self, chid, geid):
        fname = self.gfile(chid, geid)
        if os.path.exists(fname):
            with open(fname,'r',encoding='utf8') as fp:
                json_data = json.load(fp)
                if 'pnum' in json_data:
                    return json_data["pnum"]
        return 0
		
class SnapExData:
    def __init__(self):
        self.sdpath = ""
        self.dpath = "" 
        self.sfname = "" 
        self.fname = ""
        self.seq = 0

def decodeMsg(msg):
    # print(msg)
    msgData = json.loads(msg)
    # print(msgData)
    cmd = msgData["cmd"]
    dmsg = Dmsg()
    if cmd == "ch_detect_rsp":
        param = msgData["param"]
        if "pic_data" in param:
            dmsg.img_base64 = param["pic_data"]
        dmsg.chid = param["chid"]
        dmsg.ncid = param["ncid"]
        dmsg.gcids = param["gcids"]
        dmsg.geid = param["geid"]
        dmsg.seq = param["seq"]
        dmsg.nn_output = param["nn_output"]
        dmsg.location = param['location']
        dmsg.dwidth = param['dwidth']
        dmsg.dheight = param['dheight']
        dmsg.is_cache_src = False
        if "is_cache_src" in param:
            dmsg.is_cache_src = param['is_cache_src']
        if "swidth" in param:
            dmsg.swidth = param['swidth']
        if "sheight" in param:
            dmsg.sheight = param['sheight']
        if "desc" in param:
            dmsg.desc = param['desc']
        if "ip" in param:
            dmsg.ip = param['ip']
        if "surl" in param:
            dmsg.surl = param['surl']
        if "sn" in param:
            dmsg.sn = param['sn']
        if "sn32" in param:
            dmsg.sn32 = param['sn32']
        if "filter_type" in param:
            # print("have key[filter_type]")
            dmsg.filter_type = param['filter_type']
        if "pub_freq" in param:
            # print("have key[pub_freq]")
            dmsg.pub_freq = param['pub_freq']
        # print("ch[%d] seq(%d), (%dx%d)" % (dmsg.chid, dmsg.seq, dmsg.dwidth, dmsg.dheight))
        return dmsg

    return None

def encodeOutMsg(chid,img_base64,person_now,person_in,person_out):
    msgData = {}
    msgData["cmd"] = "track"
    data = {}
    data["chid"] = chid
    data["person_in"] = person_in
    data["person_out"] = person_out
    data["person_now"] = person_now
    data["pic_data"] = str(img_base64,encoding='utf-8')
    msgData["data"] = data
    msg = json.dumps(msgData)

    return msg
# def encodeSnapMsg(chid,ncid,geid,seq,nn_output,location,img_base64, sdpath, dpath, sfname, fname, width, height):
def encodeSnapMsg(dmsg, sdata):
    msgData = {}
    msgData["cmd"] = "ch_detect_rsp"
    data = {}
    data["chid"] = dmsg.chid
    data["ncid"] = dmsg.ncid
    data["ip"] = dmsg.ip
    data["surl"] = dmsg.surl
    data["geid"] = dmsg.geid
    data["sn"] = dmsg.sn
    data["sn32"] = dmsg.sn32
    data["location"] = dmsg.location
    data["width"] = dmsg.dwidth
    data["height"] = dmsg.dheight
    data["nn_output"] = dmsg.nn_output
    data["desc"] = dmsg.desc
    data["seq"] = sdata.seq
    data["sdpath"] = sdata.sdpath
    data["dpath"] = sdata.dpath
    data["sfname"] = sdata.sfname
    data["fname"] = sdata.fname
    # framestr = str(img_base64,encoding='utf-8')
    # # with open('frame.txt',mode='wb') as f:
    # #     f.write(img_base64)
    # #     exit()
    # data["pic_data"] = framestr
    msgData["param"] = data
    msg = json.dumps(msgData)

    return msg
# class PersonBox(object):
#     def __init__()

def loadRawData(fname):
    # Open a file: file
    file = open(fname, mode='rb')

    # read all lines at once
    data = file.read()

    # close the file
    file.close()
    return data

class Mainloop(threading.Thread):
    def __init__(self, threadID, name, in_queue, out_queue,need_draw,need_tracker, pic_cnt):
        threading.Thread.__init__(self)
        #print("init mainlopp")
        self.threadID = threadID
        self.name = name
        self.in_queue = in_queue
        self.out_queue = out_queue
        self.writer = utils.Writer()
        self.need_draw = need_draw
        self.chs_pub_time = {}
        self.chs_outs = {}
        self.out_total_cnt = 0
        self.pic_cnt = pic_cnt
        self.pnum = {}
        # logger.info("loading invasion_person")
        # self.invasion_person = invasion_person.InvasionPerson(self.writer,True)
        # logger.info("loading flame_detect")
        # self.flame_detect = flame_detect.FlameDetect(self.writer,True)
        # logger.info("loading flame_detect")
        # self.crowd_gathered = crowd_gathered.CrowdGathered(self.writer,True)
        # logger.info("loading helmet_detect")
        # self.helmet_detect = helmet_detect.HelmetDetect(self.writer,True)
        self.need_tracker = need_tracker
        logger.info("loading ......")
        self.detect = handler.Detect(self.writer,self.need_draw)
 #       print('done')
        logger.info("Init finish")

    def pub_filter(self, dmsg):
        is_pub = False
        if dmsg.filter_type == 0:
            chid = dmsg.chid
            if chid not in self.chs_outs:
                self.chs_outs[chid] = {}
            nn_output = dmsg.nn_output
            ch_out = {}
            for out in nn_output: 
                cid = out["cid"]
                if cid in ch_out:
                    ch_out[cid] += 1
                else:
                    ch_out[cid] = 1

            # logger.info("chs_outs(%s) ch_out(%s)" % (str(self.chs_outs[chid]), str(ch_out)))

            out_dif = set(self.chs_outs[chid].items()) ^ set(ch_out.items())
            # logger.info("diff(%s) len(%d)" % (str(out_dif), len(out_dif)))

            self.chs_outs[chid] = ch_out
            if len(out_dif) != 0:
                is_pub = True
            else:
                if chid in self.chs_pub_time:
                    pre = self.chs_pub_time[chid]
                    now = int(time.time()*1000)
                    diff = now - pre

                    # logger.info("chid(%d) in dict, time(%d), dif=(%d)" % (chid, self.chs_pub_time[chid], diff))
                    if diff >= dmsg.pub_freq*1000:
                        is_pub = True
                else:
                    # logger.info("chid(%d) not in dict" % (chid))
                    is_pub = True
            if is_pub:
                 self.chs_pub_time[chid] = int(time.time()*1000)
            
        return is_pub
    
    def run(self):
        logger.info("m200 start process....")
        seq = 0
        while True:
            msg = self.in_queue.get()
            rcv_time = int(time.time()*1000)
            # print(msg)
            dmsg = decodeMsg(msg)
            dec_time = int(time.time()*1000)
            t = int(time.time())
            # print("TimeInSeconds:",t)
            logger.info(f"dmsg: {msg}")

            if dmsg != None:
                is_pub = self.pub_filter(dmsg)
                if not is_pub:
                    prc_fin_time = int(time.time()*1000)
                    rlen, slen = mqtt.MqttGetQueueLen()
                    logger.info("ch[%02d] ?time[%02d](%02d:%d)ms, out(%d), queue(%d:%d) [dup drop]"%
                        (dmsg.chid, prc_fin_time-rcv_time, prc_fin_time-dec_time, dec_time-rcv_time, len(dmsg.nn_output),
                        rlen, slen))
                    continue

                fname = ("/mpp/mem/ch%d_%d.raw" % (dmsg.chid, dmsg.seq))
                ps_width = dmsg.dwidth
                ps_height = dmsg.dheight
                
                if dmsg.is_cache_src:
                    sfname = ("/mpp/mem/ch%d_%d.raw.src" % (dmsg.chid, dmsg.seq))
                    if os.path.exists(sfname):
                        fname = sfname
                        ps_width = dmsg.swidth
                        ps_height = dmsg.sheight
                if not os.path.exists(fname):
                    prc_fin_time = int(time.time()*1000)
                    rlen, slen = mqtt.MqttGetQueueLen()
                    logger.info("ch[%02d] ?time[%02d](%02d:%d)ms, out(%d), queue(%d:%d) [no file drop]"%
                        (dmsg.chid, prc_fin_time-rcv_time, prc_fin_time-dec_time, dec_time-rcv_time, len(dmsg.nn_output),
                        rlen, slen))
                    continue

                idata = np.fromfile(fname, dtype='uint8')
                # idata = idata.reshape(dmsg.dheight, dmsg.dwidth, 3)
                idata = idata.reshape(ps_height, ps_width, 3)
                frame = cv2.cvtColor(idata, cv2.COLOR_RGB2BGR)
                sframe = frame.copy()
                # img = base64.b64decode(dmsg.img_base64)
                # img_np = np.frombuffer(img,dtype=np.int8)

                # frame = cv2.imdecode(img_np,cv2.IMREAD_COLOR)
                needAlarm = False
                # nstr = "nn_out:" + str(dmsg.nn_output)
                # gcidstr = "gcids:" + gcids
                # logger.info(nstr)
                frame,needAlarm,dposter_out = self.detect.process(dmsg.nn_output,frame,dmsg.geid,dmsg.gcids)

                if needAlarm:
                    dmsg.nn_output = dposter_out
                    # logger.info("needAlarm: True")
                    img_out = cv2.imencode('.jpg',frame)[1]
                    frame_base64 = base64.b64encode(img_out)
                    # h,w = frame.shape[0:2]
                    # frame_base64 = None
                    seq = seq+1

                    lpnum = PNum()
                    if dmsg.chid not in self.pnum:
                        pic_num = lpnum.load(dmsg.chid, dmsg.geid)
                    else:
                        pic_num = self.pnum[dmsg.chid]
                    pic_num = pic_num+1
                    pnum = pic_num%self.pic_cnt
                    lpnum.save(dmsg.chid, dmsg.geid, pnum)
                    self.pnum[dmsg.chid] = pnum

                    sdata = SnapExData()
                    sdata.dpath = "/userdata/mpp/disk/"
                    sdata.sdpath = "/userdata/mpp/sdisk/"
                    sdata.fname = ("ch%d_m%d_%d.jpg" % (dmsg.chid, dmsg.geid, pnum))
                    sdata.sfname = ("s_%s" %(sdata.fname))
                    sdata.seq = seq
                    
                    if len(dposter_out) == 0:
                        dmsg.geid = ALGO_IDCT["m36"]["geid"]
                        #dmsg.gcids = [ALGO_IDCT["m36"]["gcid"]]
                        dmsg.gcids = [9216, 9218]
                        dmsg.engine_name = ALGO_IDCT["m36"]["engine_name"]

                    track_payload = encodeSnapMsg(dmsg, sdata)
                    msgOut = {}
                    msgOut["topic"] = "/smart_gw/cmd"
                    msgOut["payload"] = track_payload

                    # print("sfname=", sdpath+sfname)
                    # print("fname=", dpath+fname)

                    cv2.imwrite(sdata.sdpath+sdata.sfname, sframe)
                    cv2.imwrite(sdata.dpath+sdata.fname, frame)

                    self.out_queue.put(msgOut)

                # out_payload = encodeOutMsg(chid,frame_base64,mTracker.count_now,mTracker.count_in,mTracker.count_out)
                # msgOut2 = {}
                # msgOut2["topic"] = "/receiver/cmd"
                # msgOut2["payload"] = out_payload
                # self.out_queue.put(msgOut2)
            prc_fin_time = int(time.time()*1000)
            rlen, slen = mqtt.MqttGetQueueLen()
            logger.info("ch[%02d] !time[%02d](%02d:%d)ms, out(%d), queue(%d:%d) [alarm(%d)]"%
                        (dmsg.chid, prc_fin_time-rcv_time, prc_fin_time-dec_time, dec_time-rcv_time, len(dmsg.nn_output),
                        rlen, slen, int(needAlarm))
                        )
            # logger.info(dmsg.nn_output)
                
