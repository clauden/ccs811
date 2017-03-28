
import sys, serial, argparse
import numpy as np
from time import sleep
from collections import deque

import matplotlib.pyplot as plt 
import matplotlib.animation as animation

    

class AnalogPlot:

  def __init__(self, strPort, maxLen):
      # open serial port
      self.ser = serial.Serial(strPort, 9600)
      print("opened %s" % strPort)

      self.ax = deque([0.0]*maxLen)
      self.ay1 = deque([0.0]*maxLen)
      self.ay2 = deque([0.0]*maxLen)
      self.maxLen = maxLen

  # keep buffer full, dropping oldest data
  def addToBuf(self, buf, val):
      if len(buf) < self.maxLen:
          buf.append(val)
      else:
          buf.pop()
          buf.appendleft(val)

  # add a whole data record
  # assume <co2> <voc> <timestamp>
  def add(self, data):
      assert(len(data) == 3)
      self.addToBuf(self.ax, data[2])
      self.addToBuf(self.ay1, data[0])
      self.addToBuf(self.ay2, data[1])

  # update plot
  def update(self, frameNum,  a1, a2):
      print("enter update")
      line = ""
      try:
          line = self.ser.readline()
          print("read %s" % line)
          data = [float(val) for val in line.split()]
          
          if(len(data) == 3):
              self.add(data)
              # a0.set_data(range(self.maxLen), self.ax)
              a1.set_data(range(self.maxLen), self.ay1)
              a2.set_data(range(self.maxLen), self.ay2)
          print("data: ", data)
          print("ax: ", self.ax)
          print("ay1: ", self.ay1)
          print("ay2: ", self.ay2)

      except KeyboardInterrupt:
          print('exiting')
      except ValueError:
          print("junk \'%s\'" % line)
          pass
      
      return a1,    #  (a1,a2)

  # clean up
  def close(self):
      self.ser.flush()
      self.ser.close()    

def main():
  parser = argparse.ArgumentParser(description="LDR serial")
  parser.add_argument('--port', dest='port', required=True)

  args = parser.parse_args()
  
  # strPort = '/dev/tty.usbserial-A7006Yqh'
  strPort = args.port

  print('reading from serial port %s...' % strPort)

  # plot parameters
  analogPlot = AnalogPlot(strPort, 100)

  print('plotting data...')

  # set up animation

  # fig = plt.figure()
  # fig, axes = plt.subplots(2, sharex=True)
  # ax_co2 = axes[0]
  # ax_voc = axes[1]

  fig, ax_co2 = plt.subplots()
  ax_voc = ax_co2.twinx()

  ax_co2.set_xlim((0, 100))
  ax_co2.set_xlim((0, 100))

  ax_co2.set_ylim((0, 1023))
  ax_voc.set_ylim((0, 255))

  ax_co2.set_ylabel("CO2")
  ax_voc.set_ylabel("VOC")

  # ax_co2 = plt.axes(xlim=(0, 100), ylim=(0, 1023))
  # ax_voc = plt.axes(xlim=(0, 100), ylim=(0, 255))

  # a0, = ax_co2.plot([], [])
  a1, = ax_co2.plot( [])
  a2, = ax_voc.plot( [])
  anim = animation.FuncAnimation(fig, analogPlot.update, 
                                 fargs=( a1, a2), 
                                 interval=50)

  plt.show()
  
  # clean up
  analogPlot.close()

  print('exiting.')
  

# call main
if __name__ == '__main__':
  main()

