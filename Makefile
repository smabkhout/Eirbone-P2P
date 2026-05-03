CFLAGS = -Wall -Wextra -g -Iinclude

.PHONY: c java test-java run-c run-java clean rapport

all: c java

c: src/*.c
	gcc $(CFLAGS) -o tracker src/*.c

run-c: c
	./tracker 12345
	
test-c: test/*.c src/tracker.c src/peer.c src/file.c
	gcc $(CFLAGS) -o test_c test/*.c src/tracker.c src/peer.c src/file.c

java:
	mkdir -p java/bin
	javac -d java/bin java/*.java

test-java: java
	cd java && java -cp bin PeerProtocolTest

run-java: java
	cd java && java -cp bin Main $(ARGS)

rapport:
	pdflatex rapport/rapport_final.tex

clean:
	rm -rf java/bin tracker test_c *.aux *.log *.pdf *.toc *.out
