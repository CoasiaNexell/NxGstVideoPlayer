source /opt/poky/2.5.1/environment-setup-cortexa9hf-neon-poky-linux-gnueabi

adb shell mkdir -p /nexell/daudio/NxGstVideoPlayer/
adb push Package/* /nexell/daudio/NxGstVideoPlayer/

cd libnxgstvplayer/src/
make clean
make
make install
adb push libnxgstvplayer.so /usr/lib/
adb shell sync

cd ../../
mkdir -p build
cd build
qmake ../
make clean
make

adb push NxGstVideoPlayer/NxGstVideoPlayer /nexell/daudio/NxGstVideoPlayer/NxGstVideoPlayer
adb shell sync
