import cv2
import math
import os
import ft3

# 中文字体搜索列表（按优先级）
_FONT_CANDIDATES = [
    '../../fonts/msyh.ttf',
    '/usr/share/fonts/truetype/wqy/wqy-microhei.ttc',
    '/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc',
    '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc',
    '/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf',
]

def _find_font():
    for path in _FONT_CANDIDATES:
        if os.path.isfile(path):
            return path
    return _FONT_CANDIDATES[0]

class Writer(object):
    def __init__(self):
        self.ft = ft3.put_chinese_text(_find_font())
    def text(self,frame,str,pos):
        # print('text:',str)
        # print('pos:',pos)
        frame = self.ft.draw_text(frame,pos,str,17, (60,239,234))
        return frame

def getSafeRoi(roi,w,h):
    safeRoi = []
    x1 = roi[0] if roi[0]>0 else 1
    y1 = roi[1] if roi[1]>0 else 1
    x2 = roi[2] if roi[2]<w else (w-1)
    y2 = roi[3] if roi[3]<h else (h-1)

    safeRoi = [x1,y1,x2,y2]

    return safeRoi

def drawInfo(roi,class_name,box_conf,h,w,frame):
        conf_str = class_name+'-'+str(int(box_conf * 100)) + '%'
        safeRoi = getSafeRoi(roi,w,h)
        cv2.rectangle(frame, (safeRoi[0],safeRoi[1]),(safeRoi[2],safeRoi[3]),(255,0,0),5,1)
        safeRoi2 = [roi[0],roi[1],roi[2],roi[1]+30]
        safeRoi2 = getSafeRoi(safeRoi2,w,h)
        cv2.rectangle(frame, (safeRoi2[0],safeRoi2[1]),(safeRoi2[2],safeRoi2[3]),(255,0,0),thickness=-1)

        cp_x = roi[0] if roi[0] < w else (w-1)
        cp_y = roi[1]+25 if roi[1]+25 < h else (h-1)
        corePoint = (cp_x,cp_y)
        cv2.putText(frame,conf_str,corePoint,cv2.FONT_HERSHEY_SIMPLEX,1,(255,255,255),2)

def drawInfo2(roi,infostr,h,w,frame):
        safeRoi = getSafeRoi(roi,w,h)
        cv2.rectangle(frame, (safeRoi[0],safeRoi[1]),(safeRoi[2],safeRoi[3]),(255,0,0),2,1)

        cp_x = roi[0] if roi[0] < w else (w-1)
        cp_y = roi[1]+25 if roi[1]+25 < h else (h-1)
        corePoint = (cp_x,cp_y)
        cv2.putText(frame,infostr,corePoint,cv2.FONT_HERSHEY_SIMPLEX,0.5,(255,255,0),2)
