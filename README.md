# OpenGL Audio Visualizer
A 3D visualizer that reads audio from an input source and displays different frequencies as octaves of perlin noise mountains

## Features
* Fly through proceedurally generated mountains bouncing to the rhythm of your music
* Different frequencies are represented by different sized distortions in the grid
* Can change the field of view
* Can change the colour of the mountains

## Controls
R - zoom in  
F - zoom out  
U - increase red  
J - decrease red  
I - increase green  
K - decrease green  
O - increase blue  
L - decrease blue  

## Requirements
* visual studio 2019
* python 3.7 (32-bit)
* numpy
* PyAudio from https://www.lfd.uci.edu/~gohlke/pythonlibs/#pyaudio
* build c++ project at PythonWrapper/OpenglBuild/

To compile the whole project, install pyinstaller and use install.ps1 (assumes environment is called "env")

## Preview
![Crystal](./visualizer_example.gif)
