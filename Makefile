LDLIBS := -lncurses

.PHONY: build clean

build: episim

clean:
	rm -f episim
