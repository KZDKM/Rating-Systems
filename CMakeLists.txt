cmake_minimum_required(VERSION 3.21)
project(RatingSystems)

set(CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")

add_executable(RatingSystems main.cpp glicko/rating.cpp glicko/rating.hpp trueskill/trueskill.cpp trueskill/trueskill.h trueskill/ndtr.cpp trueskill/ndtr.h trueskill/mathexpr.cpp trueskill/mathexpr.h trueskill/ndtri.cpp trueskill/ndtri.h trueskill/pdf.h trueskill/pdf.cpp trueskill/polevl.h trueskill/const.cpp)

find_package( CURL REQUIRED )
target_link_libraries( RatingSystems CURL::libcurl )