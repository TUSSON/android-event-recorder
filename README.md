# android-event-recorder
### What it is ?
> a tool to record/playback input event for android platform.

### Quick start
1. Download source
```bash
$ git clone https://github.com/TUSSON/android-event-recorder.git
```
2. Building and Running
```bash
$ cd android-event-recorder
$ mm
$ croot && adb push out/target/product/$(TARGET_PRODUCT)/system/bin/eventrec system/bin/
$ adb shell chmod u+x system/bin/eventrec
```

### Usage
* record input event
```bash
$ adb shell inputrec /data/local/tmp/record_test.txt
```
ctrl-c to stop it.

* playback input event
```bash
$ adb shell inputrec /data/local/tmp/record_test.txt -p
```

### Help
```bash
$ adb shell eventrec
Android event record/palyback utility - $Revision: 0.1 $

Usage：eventrec -r|p <event_record.txt>

  -r|p  Record or playback events  (default record)

Example of event_record.txt: 
[   20897.702414] /dev/input/event1: 0003 0035 000000b1
[   20897.702414] /dev/input/event1: 0000 0000 00000000
```

