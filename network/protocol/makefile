CC=gcc
CXX=g++
INC+= -I../ -I../../lib  -I../../3rdlib -I/usr/include/mysql
LIB+=-lpthread  -L../../lib -lvfs   -L../../3rdlib -lnmshm -lstdc++
LIB+= -L/usr/lib64/mysql/ -lmysqlclient
CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64 -fPIC -shared #-z defs
TARGET=protocol.so
ALL:$(TARGET)

OBJ=vfs_data.o

$(TARGET):$(OBJ)
	$(CC)	$(CFLAGS) -o $@ $^ $(LIB)
%.o: %.cpp
	$(CXX)  $(CFLAGS) $(INC) -c -o $@ $< 	 
%.o: %.c
	$(CC)  $(CFLAGS) $(INC) -c -o $@ $< 	 
clean:
	rm -f *.o *.so
	rm -f $(TARGET) 	
