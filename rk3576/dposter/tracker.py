import cv2
import math
import ft3
import time
import numpy as np
SafeDistance = 300

# OS='MACOS'
# OS=''

now_str_prefix = " 画面中："
in_str_prefix = " 进："
out_str_prefix = " 出："

# now_str_prefix = "now: "
# in_str_prefix = "in: "
# out_str_prefix = "out: "

def getOverlap(box1_x1,box1_y1,box1_x2,box1_y2,box2_x1,box2_y1,box2_x2,box2_y2):
    if (box1_x2 <= box2_x1 or box2_x2 <= box1_x1) and (box1_y2 <= box2_y1 or box2_y2 <= box1_y1):
        return 0
    else:
        len = min(box1_x2,box2_x2) - max(box1_x1,box2_x1)
        wide = min(box1_y2,box2_y2) - max(box1_y1,box2_y1)
        Overlap = len*wide
        OverlapRate = Overlap/((box1_x2-box1_x1)*(box1_y2-box1_y1) + (box2_x2-box2_x1)*(box2_y2-box2_y1) - Overlap)
        return OverlapRate
def getDistance(box1_x1,box1_y1,box1_x2,box1_y2,box2_x1,box2_y1,box2_x2,box2_y2):
    core1 = [int((box1_x1+box1_x2)/2),int((box1_y1+box1_y2)/2)]
    core2 = [int((box2_x1+box2_x2)/2),int((box2_y1+box2_y2)/2)]
    w = abs(core1[0] - core2[0])
    h = abs(core1[1] - core2[1])
    distance = (w**2+h**2)**0.5
    return distance


def getCoord(trackcoord):
    coords = []
    for coord in trackcoord:
        coords.append(((int(coord[0]),int(coord[1])),
                    (int(coord[0]+coord[2]),int(coord[1]+coord[3]))))

    return coords

def crossCheck(line1,line2):
    x,y,k1,b1,k2,b2 = 0,0,0,0,0,0
    crossed = False
    x1,y1,x2,y2 = line1
    x3,y3,x4,y4 = line2
    
    if (x2-x1) == 0:
        k1 = None
        b1 = 0
    else:
        k1 = (y2-y1) * 1.0 /(x2 -x1)
        b1 = y1 * 1.0 -x1*k1*1.0

    if (x4 - x3) == 0:
        k2 = None
        b2 = 0
    else:
        k2 = (y4 - y3) * 1.0 / (x4 -x3)
        b2 = y3 * 1.0 - x3*k2 *1.0
    
    if k1 is None:
        if not k2 is None:
            x = x1
            y = k2 * x1 + b2
            crossed = True
    elif k2 is None:
        x = x3
        y = k1 * x3 + b1
        crossed = True
    elif not k2 == k1:
        x = (b2-b1)*1.0/(k1 -k2)
        y = k1 * x * 1.0 + b1 * 1.0
        crossed = True

    # print("x~y:",x,y)
    # print("x1~x2:",x1,x2)
    if crossed:
        print("x:%d x1:%d x2:%d"%(x,x1,x2))
        print("y:%d y1:%d y2:%d"%(y,y1,y2))
        x_inline = (x>=x1 and x<=x2) or (x>=x2 and x<=x1)
        y_inline = (y>=y1 and y<=y2) or (y>=y2 and y<=y1)
        if x_inline and y_inline:
            return True
    
    return False

def getSafeRoi(roi,w,h):
    safeRoi = []
    x1 = roi[0] if roi[0]>0 else 1
    y1 = roi[1] if roi[1]>0 else 1
    x2 = roi[2] if roi[2]<w else (w-1)
    y2 = roi[3] if roi[3]<h else (h-1)

    safeRoi = [x1,y1,x2,y2]

    return safeRoi

class MessageItem(object):
    def __init__(self,frame,message):
        self._frame = frame
        self._message = message

    def getFrame(self):
        return self._frame

    def getMessage(self):
        return self._message

def getCoord(trackcoord):
    coords = []
    for coord in trackcoord:
        coords.append(((int(coord[0]),int(coord[1])),
                    (int(coord[0]+coord[2]),int(coord[1]+coord[3]))))

    return coords

class Person(object):
    def __init__(self,roi,id,box_conf):
        self.roi = roi
        core1 = [int((roi[0]+roi[2])/2),int((roi[1]+roi[3])/2)]
        self.core = core1
        self.id = id
        self.find = True
        self.status = ''
        self.box_conf = box_conf
        self.miss = 0
    def getRoi(self):
        return self.roi
    def checkDistance(self,box):
        roi = self.roi
        return getDistance(box[0],box[1],box[2],box[3],roi[0],roi[1],roi[2],roi[3])
    def update(self,box,box_conf,h,w,line2,in_direction):
        # roi = [box[0]-10,box[1],box[2],box[3]]
        # roi[0] = max(roi[0]-10,0)
        # roi[1] = max(roi[1]-10,0)
        # roi[2] = min(roi[2]+10,w)
        # roi[3] = min(roi[3]+10,h)
        roi = getSafeRoi(box,w,h)
        self.status = ''
        core1 = [int((roi[0]+roi[2])/2),int((roi[1]+roi[3])/2)]
        # core2 = [int((self.roi[0]+self.roi[2])/2),int((self.roi[1]+self.roi[3])/2)]
        core2 = self.core
        line1 = [core1[0],core1[1],core2[0],core2[1]]
        # line2 = [540,0,540,720]
        
        if in_direction == 'all':
            print('in_direction all...')
            self.status = 'in'
        else:
            crossed = crossCheck(line1,line2)
            # print("line1:",line1)
            # print("line2:",line2)
            print("cross status: ",crossed)
            if crossed:
                p = np.array(core1)
                a = np.array([line2[0],line2[1]])
                b = np.array([line2[2],line2[3]])
                print("p:",p)
                print("a:",a)
                print("b:",b)
                
                direction = np.cross(p-a,p-b)
                if direction <= 0:
                    self.status = 'in'
                else:
                    self.status = 'out'
                
                # if in_direction == 'up':
                #     if core2[0] > line2[0]:
                #         print('in_direction up,status in, start_point_x1[%d] > detect_line_x1[%d]...'%(core2[0],line2[0]))
                #         self.status = 'in'
                #     else:
                #         print('in_direction up,status out, start_point_x1[%d] <= detect_line_x1[%d]...'%(core2[0],line2[0]))
                #         self.status = 'out'
                # elif in_direction == 'down':
                #     if core2[0] > line2[0]:
                #         print('in_direction down,status out, start_point_x1[%d] > detect_line_x1[%d]...'%(core2[0],line2[0]))
                #         self.status = 'out'
                #     else:
                #         print('in_direction down,status in, start_point_x1[%d] <= detect_line_x1[%d]...'%(core2[0],line2[0]))
                #         self.status = 'in'
                # else:
                #     self.status = ''
                #     print("unknown in_direction:",in_direction)
            else:
                self.status = ''

        self.roi = roi
        self.core = core1
        self.box_conf = box_conf
    
    def drawInfo(self,frame,h,w):
        if self.find != True:
            return
        conf_str = 'r-'+str(int(self.box_conf * 100)) + '%'
        safeRoi = getSafeRoi(self.roi,w,h)
        print("draw box:%d %d %d %d"%(safeRoi[0],safeRoi[1],safeRoi[2],safeRoi[3]))
        cv2.rectangle(frame, (safeRoi[0],safeRoi[1]),(safeRoi[2],safeRoi[3]),(255,0,0),5,1)
        safeRoi2 = [self.roi[0],self.roi[1],self.roi[2],self.roi[1]+30]
        safeRoi2 = getSafeRoi(safeRoi2,w,h)
        cv2.rectangle(frame, (safeRoi2[0],safeRoi2[1]),(safeRoi2[2],safeRoi2[3]),(255,0,0),thickness=-1)
        
        cp_x = self.roi[0] if self.roi[0] < w else (w-1)
        cp_y = self.roi[1]+25 if self.roi[1]+25 < h else (h-1)
        corePoint = (cp_x,cp_y)
        cv2.putText(frame,conf_str,corePoint,cv2.FONT_HERSHEY_SIMPLEX,1,(255,255,255),2)

        cp2_x = self.core[0]-15 if self.core[0]-15 > 0 else 1
        cp2_y = self.core[1]-15 if self.core[1]-15 > 0 else 1
        corePoint2 = (cp2_x,cp2_y)
        cv2.putText(frame,str(self.id),corePoint2,cv2.FONT_HERSHEY_SIMPLEX,1,(100,255,0),2)

        safeRoi3 = [self.core[0] - 5,self.core[1] - 5,self.core[0] + 5,self.core[1] + 5]
        safeRoi3 = getSafeRoi(safeRoi3,w,h)
        cv2.rectangle(frame, (safeRoi3[0],safeRoi3[1]),(safeRoi3[2],safeRoi3[3]),(0,255,0),thickness=-1)


class Tracker(object):
    def __init__(self, chid,draw_coord=True):
        # (magor_ver, minor_ver,subminor_ver) = (cv2.__version__).split('.')
        # print(magor_ver, minor_ver,subminor_ver)
        self.chid = chid
        self.draw_coord = draw_coord
        self.trackers = []
        self.isWorking = True
        self.id = 0
        self.last_time = int(time.time())
        self.count = 0
        self.count_in = 0
        self.count_out = 0
        self.count_now = 0
        self.h = 720
        self.w = 1080
        self.detect_line = [0,0,0,0]
        self.in_direction = 'up'
        self.ft = ft3.put_chinese_text('../../fonts/msyh.ttf')
    def addTracker(self, box,box_conf):
        person = Person(box,self.id,box_conf)
        self.id = self.id+1
        self.trackers.append(person)
        print("new ROI:",box)

    def setParam(self,inperson,outperson,totalperson):
        self.count_in = inperson
        self.count_out = outperson
        self.count_now = totalperson

    def clear(self):
        self.trackers = []
        print("clear all trackers for timeout.")

    def setfindPerson(self,id,box,box_conf,detect_line,in_direction):
        for person in self.trackers:
            if (person.id == id):
                person.update(box,box_conf,self.h,self.w,detect_line,in_direction)
                person.find = True
                break
    def updatePerson(self):
        for person in self.trackers:
            if (person.find == False):
                person.miss = person.miss + 1
    def getOutput(self,h,w):
        output = []
        for person in self.trackers:
            if (person.find == True):
                out = {}
                out['id'] = person.id
                out['x1'] = float(person.roi[0])/w
                out['y1'] = float(person.roi[1])/h
                out['x2'] = float(person.roi[2])/w
                out['y2'] = float(person.roi[3])/h
                out['conf'] = person.box_conf
                output.append(out)

        return output
                
    def track(self, frame,nn_output,detect_line,in_direction,location):
        if self.isWorking:
            h,w = frame.shape[0:2]
            self.detect_line = [int(w*detect_line[0]),int(h*detect_line[1]),int(w*detect_line[2]),int(h*detect_line[3])]
            self.in_direction = in_direction
            self.updateTracker(nn_output,h,w)
            self.h = h
            self.w = w
            if self.draw_coord:
                # tracker_count = 0
                for person in self.trackers:
                    person.drawInfo(frame, h,w)
                    # tracker_count = tracker_count +1

                now_str = now_str_prefix + str(self.count_now)
                # now_str = now_str_prefix + str(tracker_count)
                in_str = in_str_prefix + str(self.count_in)
                out_str = out_str_prefix + str(self.count_out)

                cv2.line(frame,(self.detect_line[0],self.detect_line[1]),(self.detect_line[2],self.detect_line[3]),(10,255,10),4)

                frame = self.ft.draw_text(frame,(10,60),location,24, (60,239,234))
                frame = self.ft.draw_text(frame,(10,90),now_str,24, (60,239,234))
                frame = self.ft.draw_text(frame,(10,120),in_str,24, (60,239,234))
                frame = self.ft.draw_text(frame,(10,150),out_str,24, (60,239,234))
                # cv2.putText(frame,now_str,(10,90),cv2.FONT_HERSHEY_SIMPLEX,1,(60,239,234),2)
                # cv2.putText(frame,in_str,(10,120),cv2.FONT_HERSHEY_SIMPLEX,1,(60,239,234),2)
                # cv2.putText(frame,out_str,(10,150),cv2.FONT_HERSHEY_SIMPLEX,1,(60,239,234),2)

        return frame
                

    def updateTracker(self,nn_output,h,w):
        # new_boxs = []
        
        for person in self.trackers:
            person.find = False

        maxDistance = (h**2+w**2)**0.5
        for item in nn_output:
            box = [int(item["x1"]*w),int(item["y1"]*h),int(item["x2"]*w),int(item["y2"]*h)]
            # box_w,box_h = (item["x2"] - item["x1"]),(item["y2"] - item["y1"])
            # if box_w < 0.1 or box_h < 0.1:
                # print("pos0,w~h:",box_w,box_h)
                # continue
            # print("pos-start")
            box_conf = item["conf"]
            closestId = -1
            closestDistance = maxDistance
            for person in self.trackers:
                if person.find == True:
                    # print("pos11")
                    continue
                distance = person.checkDistance(box)
                if distance > SafeDistance:
                    # print("pos-2")
                    continue
                if distance < closestDistance:
                    closestDistance = distance
                    closestId = person.id
            
            if (closestDistance < SafeDistance) and (closestId != -1):
                # print("pos1")
                self.setfindPerson(closestId,box,box_conf,self.detect_line,self.in_direction)
            else:
                # print("pos-new2")
                self.addTracker(box,box_conf)

        # print("self.trackers1:",self.trackers)
        getIn = len(list(filter(lambda p: p.status=='in', self.trackers)))
        getOut = len(list(filter(lambda p: p.status=='out', self.trackers)))
        print("in&out list len",getIn,getOut)
        self.count = self.count + getIn - getOut
        if self.count < 0:
            self.count = 0

        self.count_in = self.count_in + getIn
        self.count_out = self.count_out + getOut

        self.updatePerson()
        self.trackers = list(filter(lambda p: p.miss<3, self.trackers))
        self.count_now  = len(list(filter(lambda p: p.find==True, self.trackers)))
        # print("self.trackers2:",self.trackers)


