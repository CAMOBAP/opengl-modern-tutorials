all:
	ndk-build
	android update project --name tut01 --path . --target "android-10"
	ant clean
	ant debug
	adb install -r bin/tut01-debug.apk 

clean:
	ndk-build clean
	ant clean
