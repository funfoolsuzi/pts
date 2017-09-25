#ifndef _POSTMGMT_HPP

#define _POSTMGMT_HPP

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <thread>
#include <chrono>
#include "sockdata.hpp"

#include <sys/socket.h>

class YMonth{
	int year;
	int month;
public:
	YMonth(void);
	YMonth(int yy, int mm);
	YMonth last(void);
	std::string to_string(void);
};

struct PostHeader{
	std::string title;
	std::vector<std::string> tag;
	PostHeader(void);
	PostHeader(const std::string& in_str);
	bool arrange_tag(const std::string& in_str);
};

std::string today(void);
bool push_id(const std::string& fpath, const std::string& id);
bool pull_id(const std::string& fpath, const std::string& id);
bool savepost(const std::string& _id, const std::string& _content);
bool savepost(const std::string& _id, int sockfd);
#endif
