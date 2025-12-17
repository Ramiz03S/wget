build:
	gcc -o $(exe)  $(exe).c `sdl2-config --cflags --libs` -lm -lcurl -g
run:
	./$(exe)

br:
	gcc -o $(exe)  $(exe).c `sdl2-config --cflags --libs` -lm -lcurl -g && ./$(exe)