FLAGS =-lLimeSuite -g -lm -O3

all:
	gcc -o mimodump ./lastmini.c  $(FLAGS)
