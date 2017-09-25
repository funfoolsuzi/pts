#ifndef _PTS_H

#define _PTS_H

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <map>
#include <mutex>
#include <fstream>

#include <cstring>
#include <cerrno>

#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "sockdata.hpp"

class TcpConn{
public:
	sockaddr_in addr;
	int age = 0;
	bool ifclose = false;
	TcpConn(void);
	TcpConn(const sockaddr_in& _addr);
	std::string to_string(void);
};

class ReqHeader{
private:
	int sfd;
	std::string _method;
	std::string _path;
	std::string _ver;
	std::string _query_str;
	std::map<std::string, std::string> _prop;
	std::string _content;
	bool _contentread = false;
public:
	static const size_t method_name_max = 7;//Longest method name is CONNECT, which has 7 characters
	static const size_t path_length_max = 512;//I don't think I need more than 512.
	static const size_t query_str_max = 1024;
	static const size_t ver_length_max = 8;// HTTP/1.1 would be the most often case.
	static const size_t prop_name_max = 64;
	static const size_t prop_value_max = 256;
	ReqHeader(void);
	bool readfromsock(int _sfd);
	std::string method(void);
	std::string path(void);
	std::string ver(void);
	std::string query_str(void);
	std::string& content(void);
	std::string salvage(void);
	std::string operator[](const std::string& pname);
};

class RespMsg{
private:
	std::string _msg;
	size_t _len;
	std::string _ver = "HTTP/1.1";
	std::string _code = "200";
	std::string _status = "OK";
	std::map<std::string,std::string> _prop;
public:
	RespMsg(const std::string& _message);
	RespMsg(void);
	bool send(int sfd);
	void setprop(const std::string& pname, const std::string& pvalue);
	void setstatus(const std::string&, const std::string&);
	std::string operator[](const std::string& pname);
};

class RespFile{
private:
	static const size_t msg_size_max = 2048;
	std::ifstream& _file;
	size_t _len;
	bool _ready = false;
	std::string _ver = "HTTP/1.1";
	std::string _code = "200";
	std::string _status = "OK";
	std::map<std::string,std::string> _prop;
public:
	RespFile(std::ifstream& file);
	void setprop(const std::string& pname, const std::string& pvalue);
	std::string operator[](const std::string& pname);
	bool getready(void);
	bool ready(void);
	bool send(int sfd);
};

class TcpServer{
private:
	std::mutex conn_mtx;
	static const int conn_max_life = 3;//seconds
	std::map<int, TcpConn> conn;
	int fd;
	unsigned short port;
	bool stoplisten = false;
	bool readylisten = false;
public:
	TcpServer(unsigned short _port,  bool _ifdetach = true);
	bool listen(int _backlog, bool (*reactfunc)(int, std::map<int,TcpConn>&),bool _ifdetach = true);
	void halt(void);
	void listconn(void);
};

void printe(const std::string& msg);
void setnonblock(int _fd);
//bool sock_recvtimeout(int, int);
void getMyAddr(void);
std::string gmtstr(time_t *t);
bool sendfile(const std::string& path, int sock, bool ifcache = false);

#endif
