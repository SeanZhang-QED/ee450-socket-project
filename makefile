all: hospitalA.cpp hospitalB.cpp hospitalC.cpp scheduler.cpp client.cpp
	g++ -o hospitalA hospitalA.cpp
	g++ -o hospitalB hospitalB.cpp
	g++ -o hospitalC hospitalC.cpp
	g++ -o scheduler scheduler.cpp
	g++ -o client client.cpp

.PHONY: hospitalA
hospitalA:
	./hospitalA

.PHONY: hospitalB
hospitalB:
	./hospitalB

.PHONY: hospitalC
monitor:
	./hospitalC

.PHONY: scheduler
AWS:
	./scheduler

.PHONY: client
client:
	./client
