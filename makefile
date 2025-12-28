b:
	gcc -Iinclude src/wget.c src/mpc.c src/http.c src/net.c src/url.c src/util.c src/http_common.c -o build/wget -lssl -g
