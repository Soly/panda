CPP=g++

CPPFLAGS=-std=c++11
 
# all: pandalog_reader
all: pandalog_devmap

pandalog.pb-c.o: pandalog.pb-c.c
	$(CPP) -c pandalog.pb-c.c -I .. -g -O0

pandalog_print.o: pandalog_print.c 
	$(CPP) -c pandalog_print.c  -g -O0 

pandalog.o: pandalog.c
	$(CPP) -c pandalog.c -I .. -D PANDALOG_READER  -g -O0

pandalog_reader.o: pandalog_reader.cpp
	$(CPP) $(CPPFLAGS) -c pandalog_reader.cpp  -g -O0

pandalog_devmap.o: pandalog_devmap.cpp
	$(CPP) $(CPPFLAGS) -c pandalog_devmap.cpp -g -O0

pandalog_reader: pandalog.o pandalog.pb-c.o pandalog_print.o pandalog_reader.o
	$(CPP) $(CPPFLAGS) -o pandalog_reader pandalog.o   pandalog.pb-c.o  pandalog_print.o  pandalog_reader.o -L/usr/local/lib -lprotobuf-c -g -O0  -I .. -lz 

pandalog_devmap: pandalog.o pandalog.pb-c.o pandalog_print.o pandalog_devmap.o
	$(CPP) $(CPPFLAGS) -o pandalog_devmap pandalog.o pandalog.pb-c.o pandalog_print.o pandalog_devmap.o -L/usr/local/lib -lprotobuf-c -g -O0  -I .. -lz 


clean:
	rm pandalog.o   pandalog.pb-c.o  pandalog_print.o  pandalog_reader.o pandalog_devmap.o pandalog_reader pandalog_devmap

