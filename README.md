remote-input
============
Forward input from your desktop to another Linux device over a TCP connection!

`remote-input` is split in two parts:
- `inputd`: a daemon which listens for incoming connections and propagates
  client events to the Linux input subsystem.
- a forwarding client. Currently, there is only an implementation for X11:
  `xforward-input`.

`inputd` uses the Linux `uinput` module to create a virtual device. There are no
other dependencies, which makes `remote-input` suitable for use in
embedded/Android devices, but as there is no protection whatsoever it shouldn't
be used outside of private/trusted networks.

Building
--------
To build the `inputd` daemon, issue
```
make
```

To build `forward_input`, issue
```
make forward_input
```

Running
-------
On the machine which will receive input, run
```
sudo remote-inputd
```

On the client, run
```
forward_input <hostname>
```
to grab the mouse and keyboard and forward input to `<hostname>`.

Building and running on Android
-------------------------------
A rooted device is required!

Assuming `ANDROID_NDK` variable is set in your environment (it should be set
if you installed the Android NDK) run:
```
make TARGET=ANDROID
```
Remount the system filesystem read-writeable, and install push `inputd`:
```
adb root
adb remount
adb push inputd /system/bin
```
If that doesn't work, but you have managed to get `su` installed, you can
try to manually remount the `/system` partition and install without using `cp`:
```
adb push inpud /data/local/tmp
adb shell
$ su
# mount -o remount,rw /system
# cat /data/local/tmp > /system/bin/inputd
# chmod 755 /system/bin/inputd
```
Only API level 21 and up are inherently supported, as the `linux/uinput.h`
header file is missing from the lower API level platform images. However, I've
found that most Android versions do in fact load the `uinput` module.

Even though it's possible to connect wirelessly, using `adb forward` results in
lower latency:

```
adb forward tcp:4004 tcp:4004
adb shell remote-inputd
forward_input localhost
```

Other software
--------------
This is hardly new technology, plenty of more "complete" solutions are
available:
- [Synergy](http://symless.com/): Designed to **share** mouse and keyboard
  between multiple computers. Requires X.
- [x2x](https://en.wikipedia.org/wiki/X2x): Forward input one X server to
  another, i.e. no console input.
- SSH + [xdotool](http://www.semicomplete.com/projects/xdotool/): Also requires
  a X server, and isn't really designed for input forwarding..
