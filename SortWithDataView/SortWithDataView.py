import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"]="1"
import numpy
import cv2

# then just type in following
PATH2EXR = "C:\Src\images\LuminanceChroma\Flowers.exr"
img = cv2.imread(PATH2EXR, cv2.IMREAD_ANYCOLOR | cv2.IMREAD_ANYDEPTH)
'''
you might have to disable following flags, if you are reading a semantic map/label then because it will convert it into binary map so check both lines and see what you need
''' 
# img = cv2.imread(PATH2EXR) 
 
img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
