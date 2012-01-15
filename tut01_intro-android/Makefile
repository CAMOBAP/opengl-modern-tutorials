all:
	ndk-build
	android update project --name wikibooks-opengl --path . --target "android-10"
	ant clean
	ant debug
	adb install -r bin/wikibooks-opengl-debug.apk

clean:
	ndk-build clean
	ant clean
	rm -rf obj/ libs/ build.xml proguard.cfg project.properties local.properties
