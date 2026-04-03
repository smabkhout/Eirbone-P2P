.PHONY: c java run-c run-java clean

c:
	mkdir -p build
	sed -E 's/[[:space:]]*\[cite:[^]]*\]//g' tracker.c > build/tracker.clean.c
	gcc -Iinclude build/tracker.clean.c -o tracker_server
	gcc -Iinclude src/main.c src/file.c src/peer.c src/tracker.c -o tracker_demo

java:
	mkdir -p java/bin
	javac -d java/bin java/*.java

run-c: c
	./tracker_server

run-java: java
	cd java && java -cp bin Main

clean:
	rm -rf build java/bin tracker_server tracker_demo
