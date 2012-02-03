SUBDIRS = tut01_intro tut02_clean tut03_com tut04_transform tut05_cube tut06_textures \
	graph01 graph02 graph03 graph04 graph05 \
	text01_intro text02_atlas \
	bezier_teapot glescraft glescraft-geometryshader mini-portal obj-viewer select stencil

.PHONY: all clean

all:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
