build:
	gcc -o viewer viewer.c `sdl2-config --cflags --libs`

run: build
	./viewer
clean:
	rm ./viewer
