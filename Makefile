.PHONY: java

java:
	cd java && mkdir -p bin
	cd java && javac -d bin *.java
	cd java && java -cp bin Main
