# StreamServer
An implementation of multi-threaded streaming.  
Start a RTSP service.  
Read H264 file.  
Push UDP stream using RTP protocol.  
  
# Feature
```  
      | H.264 | H.265 | AAC
----------------------------
  RTP |   O   |   X   |  X
----------------------------
...   | ...
...   | ...
```  
    
# Usage
1. Build  
`make clean;make`  
The executable binary file will be generated into a directory named `bin`.  
  
2. Prepare h264 data  
`ffmpeg -i test.mp4 -c h264 test.h264`  

3. Start RTSP stream server  
`cd bin`  
Usage Format: `./streamSvr [h264 data]`  
ex: `./streamSvr ../testData/test.h264`  
  
