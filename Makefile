CFLAGS = -Wall -Wextra -g -Iinclude

.PHONY: c java run-c run-java clean

c: src/*.c
	gcc $(CFLAGS) -o tracker src/*.c
	
test: test/*.c src/tracker.c src/peer.c src/file.c
	gcc $(CFLAGS) -o test_c test/*.c src/tracker.c src/peer.c src/file.c

java:
	mkdir -p java/bin
	javac -d java/bin java/*.java

run-java: java
	cd java && java -cp bin Main

clean:
	rm -rf java/bin tracker test_c
