b:
	rm -r build/* && gcc -Iinclude src/wget.c src/mpc.c -o build/wget -g
r:
	./build/wget

br:
	rm -r build/* && gcc -Iinclude src/wget.c src/mpc.c -o build/wget -g && ./build/wget