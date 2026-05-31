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

# Redis support (optional)
find_library(HIREDIS_LIB hiredis)
if(HIREDIS_LIB)
	message(STATUS "Found hiredis: ${HIREDIS_LIB}")
	add_definitions(-DUSE_HIREDIS)
else()
	message(WARNING "hiredis not found, Redis Session will use placeholder implementation")
	message(WARNING "Install with: sudo apt install libhiredis-dev")
endif()

set(COMMON_SRC
	backend/Common/src/AppConfig.cpp
)

# 领域层：服务层(含事务编排)。Repository/Service 头文件为 header-only，
# 仅 TicketService 的实现需编译，ser 与 admin 共享。
set(DOMAIN_SRC
	backend/Domain/src/service/TicketService.cpp
	backend/Domain/src/service/TicketServiceTxn.cpp
	backend/Domain/src/ServiceUtil.cpp
)

set(CHRONOLITE_SRC
	backend/ChronoLite/src/AppendFile.cpp
	backend/ChronoLite/src/AsynLogging.cpp
	backend/ChronoLite/src/CountDownLatch.cpp
	backend/ChronoLite/src/LogFile.cpp
	backend/ChronoLite/src/Logger.cpp
	backend/ChronoLite/src/LogMessage.cpp
	backend/ChronoLite/src/Timestamp.cpp
)

set(SERVER_SRC
	backend/Server/src/ser.cpp
	backend/Server/src/MetricsManager.cpp
	backend/Server/src/RedisSessionManager.cpp
	backend/Server/src/RedisConnPool.cpp
	${DOMAIN_SRC}
	backend/SqlConnPool/src/ConnectionPool.cpp
	backend/ScheduledThreadPool/src/Timer.cpp
	backend/ScheduledThreadPool/src/TimerQueue.cpp
	backend/Inet/src/Acceptor.cpp
	backend/Inet/src/Buffer.cpp
	backend/Inet/src/Channel.cpp
	backend/Inet/src/Connector.cpp
	backend/Inet/src/EPollPoller.cpp
	backend/Inet/src/EventLoop.cpp
	backend/Inet/src/EventLoopThread.cpp
	backend/Inet/src/EventLoopThreadPool.cpp
	backend/Inet/src/InetAddress.cpp
	backend/Inet/src/Poller.cpp
	backend/Inet/src/Socket.cpp
	backend/Inet/src/SocketsOps.cpp
	backend/Inet/src/TcpClient.cpp
	backend/Inet/src/TcpConnection.cpp
	backend/Inet/src/TcpServer.cpp
	backend/Inet/src/Timer.cpp
	backend/Inet/src/TimerQueue.cpp
	${CHRONOLITE_SRC}
)

add_executable(ser ${SERVER_SRC} ${COMMON_SRC})
target_include_directories(ser PRIVATE
	backend/Common/include
	backend/Domain/include
	backend/Server/include
	backend/SqlConnPool/include
	backend/Inet/include
	backend/ScheduledThreadPool/include
	backend/ChronoLite/include
	backend/FixedThreadPool/include
)
target_link_libraries(ser PRIVATE ${JSONCPP_LIB} ${MYSQLCLIENT_LIB} crypt Threads::Threads)
if(HIREDIS_LIB)
	target_link_libraries(ser PRIVATE ${HIREDIS_LIB})
endif()

add_executable(client backend/Client/src/client.cpp ${COMMON_SRC} ${CHRONOLITE_SRC})
target_include_directories(client PRIVATE backend/Common/include backend/Client/include backend/ChronoLite/include)
target_link_libraries(client PRIVATE ${JSONCPP_LIB} Threads::Threads)

add_executable(admin backend/Admin/src/admin.cpp ${COMMON_SRC} backend/Domain/src/ServiceUtil.cpp ${CHRONOLITE_SRC})
target_include_directories(admin PRIVATE backend/Common/include backend/Domain/include backend/Admin/include backend/ChronoLite/include)
target_link_libraries(admin PRIVATE ${MYSQLCLIENT_LIB} ${JSONCPP_LIB} crypt Threads::Threads)

# ---- Unit tests (zero external deps, registered with CTest) ----
enable_testing()

add_executable(test_buffer tests/test_buffer.cpp backend/Inet/src/Buffer.cpp)
target_include_directories(test_buffer PRIVATE backend/Inet/include)
target_link_libraries(test_buffer PRIVATE Threads::Threads)
add_test(NAME test_buffer COMMAND $<TARGET_FILE:test_buffer>)

add_executable(test_timestamp tests/test_timestamp.cpp backend/ChronoLite/src/Timestamp.cpp)
target_include_directories(test_timestamp PRIVATE backend/ChronoLite/include)
target_link_libraries(test_timestamp PRIVATE Threads::Threads)
add_test(NAME test_timestamp COMMAND $<TARGET_FILE:test_timestamp>)

add_executable(test_session tests/test_session.cpp)
target_include_directories(test_session PRIVATE backend/Server/include)
target_link_libraries(test_session PRIVATE Threads::Threads)
add_test(NAME test_session COMMAND $<TARGET_FILE:test_session>)

add_executable(test_protocol tests/test_protocol.cpp)
target_include_directories(test_protocol PRIVATE backend/Common/include)
target_link_libraries(test_protocol PRIVATE ${JSONCPP_LIB} Threads::Threads)
add_test(NAME test_protocol COMMAND $<TARGET_FILE:test_protocol>)

add_executable(test_redis_session
	tests/test_redis_session.cpp
	backend/Server/src/RedisSessionManager.cpp
	backend/Server/src/RedisConnPool.cpp
	${CHRONOLITE_SRC}
)
target_include_directories(test_redis_session PRIVATE
	backend/Server/include
	backend/Domain/include
	backend/ChronoLite/include
)
target_link_libraries(test_redis_session PRIVATE ${JSONCPP_LIB} Threads::Threads)
if(HIREDIS_LIB)
	target_link_libraries(test_redis_session PRIVATE ${HIREDIS_LIB})
endif()
add_test(NAME test_redis_session COMMAND $<TARGET_FILE:test_redis_session>)
