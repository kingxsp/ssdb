LEVELDB=leveldb-1.8.0
LEVELDB_PATH = ../../deps/${LEVELDB}
CFLAGS += -I./ -I${LEVELDB_PATH}/include
CLIBS +=  -L${LEVELDB_PATH} -lleveldb
OBJS = log.o fde.o config.o bytes.o
EXES = 

all: ${OBJS}

log.o: log.h log.cpp
	g++ ${CFLAGS} -c log.cpp

config.o: config.h config.cpp
	g++ ${CFLAGS} -c config.cpp

fde.o: fde.h fde.cpp fde_select.cpp fde_epoll.cpp
	g++ ${CFLAGS} -c fde.cpp

bytes.o: bytes.h bytes.cpp
	g++ ${CFLAGS} -c bytes.cpp

clean:
	rm -f ${EXES} ${OBJS}

