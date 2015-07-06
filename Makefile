#CXX=g++ --std=c++11 -O2
CXX=g++ --std=c++11 
EXES=echo_cli echo_server
CXXFLAGS += -g -Wall  
#LIBS += -L../fpnn/core -lfpnn -L../fpnn/proto -lfpproto -L../fpnn/base -lfpbase -lpthread -lcurl #-L../zookeeper-3.4.6/src/c/.libs -lzookeeper_mt
LIBS += -lpthread


all: $(EXES)
echo_cli: echo_cli.o 
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

echo_server: echo_server.o ByteBuffer.o Connection.o TCPServer.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) 

	
.SUFFIXES: .cpp .o .h

.cpp.o:
	$(CXX) -c $< $(INC) $(CXXFLAGS) 

.c.o:
	$(CC) -c $< $(INC) $(CFLAGS)

clean:
	$(RM) -f *.o $(EXES) 
