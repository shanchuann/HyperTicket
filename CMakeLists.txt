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

# 领域层：服务层(含事务编排)。Repository/Service 头文件为 header-only，
# 仅 TicketService 的实现需编译，ser 与 admin 共享。
set(DOMAIN_SRC
	Domain/src/service/TicketService.cpp
	Domain/src/service/TicketServiceTxn.cpp
	Domain/src/ServiceUtil.cpp
)

set(CHRONOLITE_SRC
	ChronoLite/src/AppendFile.cpp
	ChronoLite/src/AsynLogging.cpp
	ChronoLite/src/CountDownLatch.cpp
	ChronoLite/src/LogFile.cpp
	ChronoLite/src/Logger.cpp
	ChronoLite/src/LogMessage.cpp
	ChronoLite/src/Timestamp.cpp
)

set(SERVER_SRC
	Server/src/ser.cpp
	${DOMAIN_SRC}
	SqlConnPool/src/ConnectionPool.cpp
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
	${CHRONOLITE_SRC}
)

add_executable(ser ${SERVER_SRC} ${COMMON_SRC})
target_include_directories(ser PRIVATE
	Common/include
	Domain/include
	Server/include
	SqlConnPool/include
	Inet/include
	ScheduledThreadPool/include
	ChronoLite/include
	FixedThreadPool/include
)
target_link_libraries(ser PRIVATE ${JSONCPP_LIB} ${MYSQLCLIENT_LIB} crypt Threads::Threads)

add_executable(client Client/src/client.cpp ${COMMON_SRC} ${CHRONOLITE_SRC})
target_include_directories(client PRIVATE Common/include Client/include ChronoLite/include)
target_link_libraries(client PRIVATE ${JSONCPP_LIB} Threads::Threads)

add_executable(admin Admin/src/admin.cpp ${COMMON_SRC} Domain/src/ServiceUtil.cpp ${CHRONOLITE_SRC})
target_include_directories(admin PRIVATE Common/include Domain/include Admin/include ChronoLite/include)
target_link_libraries(admin PRIVATE ${MYSQLCLIENT_LIB} ${JSONCPP_LIB} crypt Threads::Threads)

# ---- Unit tests (zero external deps, registered with CTest) ----
enable_testing()

add_executable(test_buffer tests/test_buffer.cpp Inet/src/Buffer.cpp)
target_include_directories(test_buffer PRIVATE Inet/include)
target_link_libraries(test_buffer PRIVATE Threads::Threads)
add_test(NAME test_buffer COMMAND $<TARGET_FILE:test_buffer>)

add_executable(test_timestamp tests/test_timestamp.cpp ChronoLite/src/Timestamp.cpp)
target_include_directories(test_timestamp PRIVATE ChronoLite/include)
target_link_libraries(test_timestamp PRIVATE Threads::Threads)
add_test(NAME test_timestamp COMMAND $<TARGET_FILE:test_timestamp>)

add_executable(test_session tests/test_session.cpp)
target_include_directories(test_session PRIVATE Server/include)
target_link_libraries(test_session PRIVATE Threads::Threads)
add_test(NAME test_session COMMAND $<TARGET_FILE:test_session>)

add_executable(test_protocol tests/test_protocol.cpp)
target_include_directories(test_protocol PRIVATE Common/include)
target_link_libraries(test_protocol PRIVATE ${JSONCPP_LIB} Threads::Threads)
add_test(NAME test_protocol COMMAND $<TARGET_FILE:test_protocol>)
