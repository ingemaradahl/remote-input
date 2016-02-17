Building and running on Android
-------------------------------
Assuming ```ANDROID_NDK``` variable is set in your environment (it should be set
if you installed the Android NDK) run:
```
make TARGET=ANDROID
```
Only API level 21 and up are inherently supported, as the ```linux/uinput.h```
header file is missing from the lower API level platforms. However, I've found
that most Android versions do in fact load the ```uinput``` module.

Trying to use a ```inputd``` build built for API level 21 on anything lower
won't work however, as some symbols are ```external``` in 21 and up, whereas
they are ```__inline__``` for lower API levels. To build for anything lower than
21, you must make sure the build has a ```linux/uinput.h``` file in the include
path and hope that the ioctls match up. Don't attempt to use version from your
host machine (```/usr/include/linux/uinput.h```) as it'll probably provide more
ioctls than Android supports.
