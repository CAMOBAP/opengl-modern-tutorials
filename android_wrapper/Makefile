all:
	ndk-build NDK_DEBUG=1 V=1
	android update project --name wikibooks-opengl --path . --target "android-10"
	ant debug

install:
	ant installd

clean:
	ndk-build clean
	ant clean
	rm -rf obj/ libs/ build.xml proguard.cfg project.properties local.properties
