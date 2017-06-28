# android-event-recorder
### What it is ?
> a tool to record/replay input event for android platform, automated testing will be easier.

### Installation
* Install from binary

	Downalod ZIP and run bin/install.bat

* Install from source
```bash
$ git clone https://github.com/TUSSON/android-event-recorder.git
$ cd android-event-recorder
$ mm
$ croot && adb push out/target/product/$(TARGET_PRODUCT)/system/bin/eventrec system/bin/
$ adb shell chmod u+x system/bin/eventrec
```

#### Problems

1. adbd cannot run as root in production builds

> Push eventrec to tmp directory, run cmd with full path.
```bash
adb push eventrec /data/local/tmp/
adb chmod u+x /data/local/tmp/eventrec
adb shell /data/local/tmp/eventrec
```

### Usage
* record input event
```bash
$ adb shell eventrec /data/local/tmp/record_test.txt
```
ctrl-c to stop it.

* replay input event
```bash
$ adb shell eventrec /data/local/tmp/record_test.txt -p
```

### Help
```bash
$ adb shell eventrec
Android event record/palyback utility - $Revision: 1.0 $

Usage：eventrec -r|p [-c count] [-d second] <event_record.txt>

  -r|p       Record or replay events  (default record)

  -c count   Repeat count for replay

  -d secound  delay for everytime replay start

Example of event_record.txt:
[   20897.702414] /dev/input/event1: 0003 0035 000000b1
[   20897.702414] /dev/input/event1: 0000 0000 00000000
```
