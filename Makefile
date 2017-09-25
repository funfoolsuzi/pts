main: pts.hpp pts.cpp postmgmt.hpp postmgmt.cpp sockdata.hpp sockdata.cpp main.cpp
	g++ main.cpp pts.cpp postmgmt.cpp sockdata.cpp -std=c++11 -pthread -o main -Wall -g
