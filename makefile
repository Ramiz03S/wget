b:
	gcc -Iinclude src/wget.c src/mpc.c -o build/wget -lssl -g
r:
	./build/wget

br:
	gcc -Iinclude src/wget.c src/mpc.c -o build/wget -lssl -g && ./build/wget