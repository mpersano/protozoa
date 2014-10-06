mknod /tmp/video p
ppmtoy4m < /tmp/video | y4mscaler -O chromass=420mpeg2 | mpeg2enc -F 3 -q 1 -V 230k -b 7500 -o capture.mpg &
i686/game > /tmp/video
