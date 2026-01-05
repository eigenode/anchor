CC = clang
CFLAGS = -Wall -Wextra -O2 -Iinclude

anchor:
	clang -Wall -Wextra anchor.c -o anchor

clean:
	rm -f src/*.o anchor
