build:
	gcc -o $(exe)  $(exe).c -g
run:
	./$(exe)

br:
	gcc -o $(exe)  $(exe).c -g && ./$(exe)