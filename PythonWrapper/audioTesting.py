import pyaudio
import struct
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation

CHUNK = 1764
FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 44100

p = pyaudio.PyAudio();


#for i in range(0,100):
#    try:
#        print(p.get_device_info_by_index(i))
#    except: 
#        break


stream = p.open(
    format=FORMAT,
    channels=CHANNELS,
    rate=RATE,
    input=True,
    output=True,
    frames_per_buffer=CHUNK,
    input_device_index=2 # virtual audio cable index is 2
)

fig, ax = plt.subplots()

bars = 3
points = CHUNK / 2 + 1
logSize = np.log10(points)
logBins = np.linspace(0, logSize, bars)
bins = (10**logBins).astype(int)
for i in range(1, len(bins)):
    if i==0: continue
    if bins[i] == bins[i-1]:
        for j in range(i, len(bins)):
            bins[j] += 1
print(bins)

def sumBins(dat, b):
    binVals = []
    index = 0
    amt = 0
    for i in b:
        if index == i: continue
        jndex = index
        amt = 0
        while index < i and index < len(dat):
            amt +=  dat[index]
            index += 1
        amt /= index-jndex
        binVals.append(amt)
    return binVals

#x = np.arange(0, bars)
#line, = ax.plot(x, np.random.rand(bars), "ko")
x = np.arange(0, CHUNK)
xFreq = np.fft.fftfreq(CHUNK, d=1.0/RATE)
found100 = False;
found1000 = False;
for i in range(0, 1000):
    if (xFreq[i] > 100 and found100 == False):
        print(i)
        found100 = True
    if (xFreq[i] > 1000 and found1000 == False):
        print(i)
        found1000 = True
line, = ax.semilogx(xFreq, np.random.rand(CHUNK), "ko")
ax.set_ylim(0, 10)

#while True:
#    data = stream.read(CHUNK)
#    data_int = np.log10( np.abs( np.fft.fft(struct.unpack(str(CHUNK) + 'h', data)) ) )
#    binVals = sumBins(data_int, bins)
#    print('hello world')

def animate(i):
    dataString = stream.read(CHUNK)
    data = np.divide( struct.unpack(str(CHUNK) + 'h', dataString), 32768.0 )
    data_int = np.log10(np.abs( np.fft.fft(data) ))
    #binVals = sumBins(data_int, bins)
    ax.clear()
    ax.semilogx(xFreq[0:883], data_int[0:883])
    ax.set_ylim(0, 10)

ani = animation.FuncAnimation(fig, animate, interval=1000)
plt.show()


