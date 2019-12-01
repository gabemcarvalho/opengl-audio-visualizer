import OpenGL_Experiments as gl
import pyaudio
import struct
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import time

gl.runProgram()

print("Hello World\n")

CHUNK = 1764 # 40.0 ms
FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 44100

p = pyaudio.PyAudio();
stream = p.open(
    format=FORMAT,
    channels=CHANNELS,
    rate=RATE,
    input=True,
    output=True,
    frames_per_buffer=CHUNK,
    input_device_index=2 # virtual audio cable index is 2
)

bars = 3
logBins = np.linspace(0,  np.log10(CHUNK / 2 + 1), bars)
bins = (10**logBins).astype(int)
for i in range(1, len(bins)):
    if i==0: continue
    if bins[i] == bins[i-1]:
        for j in range(i, len(bins)):
            bins[j] += 1

bins = [5, 41, 883] # 100 Hz, 1000 Hz, and above 1000 Hz
print(bins)

def sumBins(dat, b):
    binVals = []
    index = 0
    amt = 0
    m = 0
    for i in b:
        if index == i: continue
        jndex = index
        amt = 0
        while index < i and index < len(dat):
            amt +=  dat[index]
            m = max(m, dat[index])
            index += 1
        amt /= index-jndex
        binVals.append( (amt + m) / 2 )
    return binVals

print('program running. stop with ctrl-c')

bin0av = 0
bin1av = 0
bin2av = 0

start = time.time()
k = 0

try:
    k = 0
    while True:

        dataString = stream.read(CHUNK)
        data = np.divide( struct.unpack(str(CHUNK) + 'h', dataString), 32768.0 )
        data_int = np.log10(np.abs( np.fft.fft(data) ))
        binVals = sumBins(data_int, bins)

        if (bin0av < 0):
            bin0av = 0
        if (bin1av < 0):
            bin1av = 0
        if (bin2av < 0):
            bin2av = 0

        bin0av = bin0av * 0.2 + binVals[0] * 0.8 # 0.2 - 0.8
        bin1av = bin1av * 0.7 + binVals[1] * 0.3
        bin2av = bin2av * 0.7 + binVals[2] * 0.3

        #print(binVals)

        gl.setShaderBrightness(max((bin1av - 4) / 6, 0.0))
        gl.setMountainHeight(
            max(bin0av - 1.0, 0.0),
            max(bin1av, 0.0),
            max(bin2av, 0.0)
        )
        
        #k += 1
        #if (time.time() - start > 1.0):
        #    print(k)
        #    k = 0
        #    start = time.time()
except KeyboardInterrupt:
    pass

gl.stopProgram()
print('program stopped')
