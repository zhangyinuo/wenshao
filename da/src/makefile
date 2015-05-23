CC=gcc
CXX=g++
INC+= -I./ -I../inc -I../../lib -I../../3rdlib -I/usr/include/mysql
LIB+= -lpthread -ldl -lrt -rdynamic 
LIB+=-L../lib/ -lcdc -L../../lib -lvfs -L../../3rdlib -lnmshm -L/usr/lib64/mysql/ -lmysqlclient

CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE=1 -D_LARGE_FILES=1 -D_XOPEN_SOURCE=600
TARGET= cdc_db
ALL:$(TARGET)

cdc_db_OBJ = cdc_db.o cdc_db_oper.o

cdc_db:$(cdc_db_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)

#	install $(TARGET) ../bin
%.o: %.cpp
	$(CXX)  $(CFLAGS) $(INC) -c -o $@ $< 	 
%.o: %.c
	$(CC)  $(CFLAGS) $(INC) -c -o $@ $< 	 
clean:
	rm -f *.o
	rm -f $(TARGET) 	
