LIBDIR=../lib/cryptopp
CXXFLAGS += -std=c++11 -I$(LIBDIR) -pthread -O0 -g -D$(DEBUG)
DEBUG = DEBUG
OBJS = Main.o Chord.o MessageProcessor.o Node.o Query.o csapp.o 
myChord : $(OBJS)
	g++ $(CXXFLAGS) -o myChord $(OBJS)  -L$(LIBDIR) -lcryptopp 
main.o : main.cpp query/Query.h Chord.h 
	g++ $(CXXFLAGS) -c main.cpp
Chord.o : Chord.cpp Chord.h Node.h utils/csapp.h 
	g++ $(CXXFLAGS)  -c Chord.cpp 
MessageProcessor.o : MessageProcessor.cpp MessageProcessor.h
	g++ $(CXXFLAGS) -c MessageProcessor.cpp
Node.o : Node.cpp Node.h Chord.h utils/csapp.h MessageProcessor.h
	g++ $(CXXFLAGS) -c Node.cpp  
Query.o : query/Query.cpp Chord.h query/Query.h
	g++ $(CXXFLAGS) -c query/Query.cpp
csapp.o : utils/csapp.cpp utils/csapp.h
	g++ $(CXXFLAGS) -c utils/csapp.cpp
clean :
	rm -f *.o myChord
