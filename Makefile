.PHONY: all common sw_renderer raytracer clean

all: common sw_renderer raytracer

common:
	$(MAKE) -C common

sw_renderer: common
	$(MAKE) -C sw_renderer

raytracer: common
	$(MAKE) -C raytracer

clean:
	$(MAKE) -C common clean
	$(MAKE) -C sw_renderer clean
	$(MAKE) -C raytracer clean
