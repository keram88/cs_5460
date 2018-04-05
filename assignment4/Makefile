CC=gcc
CFLAGS=-O2 -Wall -Wextra -Werror

all: problem_1 problem_2 problem_3

problem_1: problem_1.c
	${CC} ${CFLAGS} problem_1.c -lpthread -o problem_1

test_problem_1: problem_1
	taskset -c 1 ./problem_1 99 600

problem_2: problem_2.c
	${CC} ${CFLAGS} problem_2.c -lpthread -o problem_2

test_problem_2: problem_2
	taskset -c 1 ./problem_2 99 600

problem_3: problem_3.c
	${CC} ${CFLAGS} problem_3.c -lpthread -o problem_3

test_problem_3: problem_3
	./problem_3 99 600

clean:
	rm -f problem_1 problem_2 problem_3
