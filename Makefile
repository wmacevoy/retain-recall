CFLAGS=-g -Iinclude
CXXFLAGS=$(CFLAGS)
LDFLAGS=-lpthread
JAVA=java
JAVAC=javac

#added a comment here

all : libs bins java
libs : lib/libsort.a
bins : bin/midi1 bin/midi2 bin/test bin/oldsort bin/newsort bin/pool bin/graph bin/i18n bin/logging bin/profile bin/checkpoint

tmp/midi1.o : src/midi1.cpp include/retain.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
bin/midi1 : tmp/midi1.o
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

tmp/midi2.o : src/midi2.cpp include/retain.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
bin/midi2 : tmp/midi2.o
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)


tmp/test.o : src/test.cpp include/thread.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

tmp/thread.o : src/thread.cpp include/thread.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/test : tmp/test.o tmp/thread.o
	$(CXX) $(CXXFLAGS) -o $@ tmp/test.o tmp/thread.o $(LDFLAGS)

tmp/sort.o : src/sort.c include/sort.h 
	$(CC) $(CFLAGS) -c -o $@ $<
lib/libsort.a : tmp/sort.o
	ar cr lib/libsort.a $<

tmp/oldsort.o : src/oldsort.c include/sort.h
	$(CC) $(CFLAGS) -c -o $@ $<
bin/oldsort : tmp/oldsort.o lib/libsort.a
	$(CC) $(CFLAGS) -o $@ $< -Llib -lsort  $(LDFLAGS)

tmp/newsort.o : src/newsort.cpp include/sort.h include/retain.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
bin/newsort : tmp/newsort.o lib/libsort.a
	$(CXX) $(CXXFLAGS) -o $@ $< -Llib -lsort $(LDFLAGS)

tmp/pool.o : src/pool.cpp include/retain.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
bin/pool : tmp/pool.o
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

tmp/graph.o : src/graph.cpp include/retain.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
bin/graph : tmp/graph.o
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

tmp/i18n.o : src/i18n.cpp include/retain.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
bin/i18n : tmp/i18n.o
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

tmp/logging.o : src/logging.cpp include/retain.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
bin/logging : tmp/logging.o
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

tmp/profile.o : src/profile.cpp include/retain.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
bin/profile : tmp/profile.o
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

tmp/checkpoint.o : src/checkpoint.cpp include/retain.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
bin/checkpoint : tmp/checkpoint.o
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean :
	/bin/rm -rf lib/* tmp/* bin/* src/*~

run : all
	./bin/test
	./bin/oldsort
	./bin/newsort
	./bin/i18n greet english greet hello ohla spanish greet hello ohla
	./bin/pool
	./bin/graph
	./bin/logging
	./bin/profile
	./bin/checkpoint

zip : 
	bash ./mkzip

java :
	$(JAVAC) -d bin -classpath bin -sourcepath src src/retained/Static.java src/retained/Test.java

javarun :
	$(JAVA) -classpath bin retained/Test
