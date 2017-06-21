adb wait-for-device
adb root
adb wait-for-device
adb remount
adb wait-for-device
adb push eventrec /system/bin/
adb shell chmod u+x /system/bin/eventrec
