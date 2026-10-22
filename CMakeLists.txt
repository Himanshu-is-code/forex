cmake_minimum_required(VERSION 3.20)
project(forex)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# In modern C++, header-based components are included by main.cpp.
# Listing .h files here ensures they are indexed in CLion while only main.cpp is compiled.
add_executable(forex
    main.cpp
    src/domain/Domain.h
    src/rates/ExchangeRateBook.h
    src/fees/FeeStrategy.h
    src/observers/TransactionObserver.h
    src/service/ForexExchangeService.h
)

# REST API Server target
add_executable(forex_server
    server.cpp
    src/domain/Domain.h
    src/rates/ExchangeRateBook.h
    src/fees/FeeStrategy.h
    src/observers/TransactionObserver.h
    src/service/ForexExchangeService.h
)
if(WIN32)
    target_link_libraries(forex_server PRIVATE ws2_32)
endif()
