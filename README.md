# StreamServer
An implementation of multi-threaded streaming.  
Start a RTSP service.  
Read H264/AAC media source file.  
Push UDP stream using RTP protocol.  
  
# Feature
```  
         | H.264 | H.265 | AAC
--------------------------------
RTSP/RTP |   O   |   X   |  O
--------------------------------
...      | ...
...      | ...
```  
    
# Usage
1. Build  
`make clean;make`  
The executable binary file will be generated into a directory named `bin`.  
  
2. Prepare data  
Make video and audio have the same name.  
And put them in the same directory.  
`ffmpeg -i test.mp4 -c h264 test.h264`  
`ffmpeg -i test.mp4 -c aac test.aac`  
  
3. Start RTSP stream server  
`cd bin`  
Usage Format: `./streamSvr [media data path]`  
ex: `./streamSvr ../testData`  
  
4. Play stream via VLC  
Open network stream:  `rtsp://<ip>:<port>/<media name>`  
ex: `rtsp://192.168.1.126:8554/test`  
  
