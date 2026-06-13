import time
import utils

from config import ALGO_IDCT

from loguru import logger

def bbOverlapForBox2(box1,box2):
    x1 = box1['x1'] - 0.005
    y1 = box1['y1'] - 0.005
    w1 = box1['x2'] - box1['x1'] + 0.01
    h1 = box1['y2'] - box1['y1'] + 0.01

    x2 = box2['x1']
    y2 = box2['y1']
    w2 = box2['x2'] - box2['x1']
    h2 = box2['y2'] - box2['y1']

    endx = max(x1+w1,x2+w2)
    startx = min(x1,x2)
    width = w1+w2 - (endx - startx)

    endy = max(y1 + h1, y2+ h2)
    starty = min(y1,y2)
    height = h1 + h2 - (endy - starty)

    if width <= 0 or height <= 0:
        return 0.0
    else:
        area = width * height
        area1 = w1*h1
        area2 = w2*h2
        ratio2 = area / (area2)
        return ratio2

class Detect(object):
    def __init__(self, writer,draw):
        # (magor_ver, minor_ver,subminor_ver) = (cv2.__version__).split('.')
        # print(magor_ver, minor_ver,subminor_ver)
        self.draw = draw
        self.writer = writer
        # self.extend_dic = extend_dic
    def process(self,boxs,frame,geid,seq):
        h,w = frame.shape[0:2]
        dposter_out = []
        # print(boxs)
        logger.info("detect boxs: {}", self.draw)
        if self.draw:
            frame_new = frame
            det_cnt = 0
            alarmList = []
            
            for box in boxs:
                if box['conf'] >= ALGO_IDCT["m36"]["det_thres"]:
                    det_cnt += 1
                    
                    bbox = box.copy()
                    dposter_out.append(bbox)
                    
                    index_str = "["+str(det_cnt)+"] "
                    alarm_str = index_str + box['class_name']
                    roi = [int(box['x1']*w),int(box['y1']*h),int(box['x2']*w),int(box['y2']*h)]
                    
                    utils.drawInfo2(roi,index_str,h,w,frame_new)
                    alarmList.append(alarm_str)
            
            if det_cnt == 0:
                text_pos = (10, 30)
                frame_new = self.writer.text(frame_new,"人员离岗",text_pos)
                box = {
                    "cid":	0,
                    "gcid":	ALGO_IDCT["m36"]["gcid"],
                    "aid":	0,
                    "class_name":	ALGO_IDCT["m36"]["class_name"],
                    "conf":	0.9,
                    "x1":	0,
                    "x2":	0.1,
                    "y1":	0,
                    "y2":	0.1
                }

                return frame_new,True,[box]
            else:
                text_height_pos = 30
                for ala in alarmList:
                    text_pos = (10,text_height_pos)
                    frame_new = self.writer.text(frame_new,ala,text_pos)
                    text_height_pos = text_height_pos + 20
                    h,w = frame_new.shape[0:2]
                    if text_height_pos+20 >= h:
                        break

                return frame_new,True,dposter_out
                
        return frame,True,dposter_out

