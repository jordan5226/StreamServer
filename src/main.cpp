#include <iostream>
#include "service/rtsp.h"

int main( int argc, char* argv[] )
{
    RTSP rtsp( argv[ 1 ] );
    rtsp.StartService();
}
