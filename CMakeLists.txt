cmake_minimum_required(VERSION 3.10)

project(HyperTicket LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Place runtime executables into repository-level bin/ directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

find_package(Threads REQUIRED)

find_library(JSONCPP_LIB jsoncpp)
if(NOT JSONCPP_LIB)
	set(JSONCPP_LIB jsoncpp)
endif()

find_library(MYSQLCLIENT_LIB mysqlclient)
if(NOT MYSQLCLIENT_LIB)
	set(MYSQLCLIENT_LIB mysqlclient)
endif()

set(COMMON_SRC
	Common/src/AppConfig.cpp
)

set(SERVER_SRC
	Server/src/ser.cpp
	CGImysql/src/SqlConnectionPool.cpp
	ScheduledThreadPool/src/Timer.cpp
	ScheduledThreadPool/src/TimerQueue.cpp
	Inet/src/Acceptor.cpp
	Inet/src/Buffer.cpp
	Inet/src/Channel.cpp
	Inet/src/Connector.cpp
	Inet/src/EPollPoller.cpp
	Inet/src/EventLoop.cpp
	Inet/src/EventLoopThread.cpp
	Inet/src/EventLoopThreadPool.cpp
	Inet/src/InetAddress.cpp
	Inet/src/Poller.cpp
	Inet/src/Socket.cpp
	Inet/src/SocketsOps.cpp
	Inet/src/TcpClient.cpp
	Inet/src/TcpConnection.cpp
	Inet/src/TcpServer.cpp
	Inet/src/Timer.cpp
	Inet/src/TimerQueue.cpp
	ChronoLite/src/AppendFile.cpp
	ChronoLite/src/AsynLogging.cpp
	ChronoLite/src/CountDownLatch.cpp
	ChronoLite/src/LogFile.cpp
	ChronoLite/src/Logger.cpp
	ChronoLite/src/LogMessage.cpp
	ChronoLite/src/Timestamp.cpp
)

add_executable(ser ${SERVER_SRC} ${COMMON_SRC})
target_include_directories(ser PRIVATE
	Common/include
	Server/include
	CGImysql/include
	Inet/include
	ScheduledThreadPool/include
	ChronoLite/include
	FixedThreadPool/include
)
target_link_libraries(ser PRIVATE ${JSONCPP_LIB} ${MYSQLCLIENT_LIB} Threads::Threads)

add_executable(client Client/src/client.cpp ${COMMON_SRC})
target_include_directories(client PRIVATE Common/include Client/include)
target_link_libraries(client PRIVATE ${JSONCPP_LIB} Threads::Threads)

add_executable(admin Admin/src/admin.cpp ${COMMON_SRC})
target_include_directories(admin PRIVATE Common/include Admin/include)
target_link_libraries(admin PRIVATE ${MYSQLCLIENT_LIB} ${JSONCPP_LIB} Threads::Threads)
