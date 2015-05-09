NS=worker
CC=gcc
CXX=g++
INC+= 
LIB+= -lpthread -ldl -lrt -rdynamic -L../lib -lvfs -lstdc++ -L../3rdlib -lnmshm 
TARGET=$(NS)
ALL:$(TARGET)

CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_NS_=\"$(NS)\" -I../lib -I../3rdlib
SUBOBJS=vfs_so.o

OBJ= main.o	\
	daemon.o \
	fdinfo.o	\
	thread.o	\
	version.o	\
	watchdog.o	\
	mybuff.o \
	vfs_init.o \
	vfs_task.o \
	vfs_localfile.o \
	vfs_timer.o \
	parsepara.o	\
	vfs_file_filter.o 

OBJ += $(SUBOBJS)
		
$(TARGET):$(OBJ)
	$(CC)	$(CFLAGS) -o $@ $^ $(LIB)
#	install $(TARGET) ../bin
%.o: %.cpp
	$(CXX)  $(CFLAGS) $(INC) -c -o $@ $< 	 
%.o: %.c
	$(CC)  $(CFLAGS) $(INC) -c -o $@ $< 	 
clean:
	rm -f $(OBJ) *.so
	rm -f $(TARGET) 	
