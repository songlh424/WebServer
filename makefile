CXX ?= g++

DEBUG ?= 1
ifeq($(DEBUG), 1)
	CXXFLAGS += -g
else
	CXXFLAGS += -O2

endif

server: main.cpp ./timer/lst_timer.cpp ./http/http_conn ./async_log/log.cpp ./mysql/sql_connection_pool.cpp webserver.cpp config.cpp
	$(CXX) -o server $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm -r server