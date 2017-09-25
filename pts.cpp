#include "pts.hpp"

//TcpConn
TcpConn::TcpConn(const sockaddr_in& _addr):addr(_addr){}
TcpConn::TcpConn(void){}
std::string TcpConn::to_string(void){
	std::string rv(inet_ntoa(addr.sin_addr));
	return rv;
}

//ReqHeader
ReqHeader::ReqHeader(void):_method(""), _path(""), _ver(""), _content(""), _query_str(""){_prop.clear();}

bool ReqHeader::readfromsock(int _sfd){
try{
	sfd = _sfd;
	char chbuf = '\0';
	if(recv(sfd, &chbuf, sizeof chbuf, MSG_PEEK)==-1) if(errno!=EWOULDBLOCK) return false;
	while((chbuf=sock_recvchar(sfd))!=32){
		_method+=chbuf;
		if(_method.size()>method_name_max) return false;//so now, it is 8-char-long. One more than we need. It shouldn't be a http request any more. We'll need to stop the process and salvage the data that we have already read from the socket.
	}
	bool ifquery = false;
	while((chbuf=sock_recvchar(sfd))!=32){
		if(chbuf=='?'){ifquery=true;continue;}
		if(ifquery)_query_str+=chbuf;
		else _path+=chbuf;
		if(_path.size()>path_length_max&&_query_str.size()>query_str_max) return false;
	}
	while((chbuf=sock_recvchar(sfd))!=13){
		_ver+=chbuf;
		if(_ver.size()>ver_length_max) return false;
	}
	if(sock_recvchar(sfd)!=10) return false;
	while(1){
		std::string namebuf = "";
		while((chbuf=sock_recvchar(sfd))!=':'){
			namebuf+=chbuf;
			if(namebuf.size()>prop_name_max)return false;
		}
		_prop[namebuf] = "";
		while(sock_peekchar(sfd)==32)sock_recvchar(sfd);
		while((chbuf=sock_recvchar(sfd))!=13){
			_prop[namebuf]+=chbuf;
			if(_prop[namebuf].size()>prop_value_max)return false;
		}
		if(sock_recvchar(sfd)!=10)return false;
		if(sock_peekchar(sfd)==13){sock_recvchar(sfd);break;}
	}
	if(sock_recvchar(sfd)!=10)return false;
}catch(int en){if(en==100)std::cout<<"network is too slow"<<std::endl;}
	return true;
}

std::string ReqHeader::method(void){ return _method;}
std::string ReqHeader::path(void){ return _path;}
std::string ReqHeader::ver(void){ return _ver;}
std::string ReqHeader::query_str(void){ return _query_str;}
std::string& ReqHeader::content(void){
	if(_prop.count("Content-Length")==1&&(!_contentread)){
		int contentlen = std::stoi(_prop["Content-Length"]);
		char cbuf;
		int trys = 0;
		while(_content.size()<contentlen){
			if(recv(sfd, &cbuf, sizeof cbuf, 0)==-1){
				if(errno!=EWOULDBLOCK){printe("recv()-ReqHeader::content()");break;}
				if(trys>2)break;
				trys++;
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				continue;
			}
			_content+=cbuf;
		}
		if(_content.size()<contentlen)_contentread=true;
	}
	return _content;
}
std::string ReqHeader::salvage(void){
	std::string salv = "";
	salv+=(_method+' ');
	salv+=(_path+' ');
	salv+=(_ver+"\r\n");
	for(auto const& property: _prop){
		salv+=(property.first+": ");
		salv+=(property.second+"\r\n");
	}
	salv+="\r\n";
	return salv;
}
std::string ReqHeader::operator[](const std::string& pname){ return _prop[pname];}

//RespMsg
RespMsg::RespMsg(void):_msg(""),_len(0){}

RespMsg::RespMsg(const std::string& _message):_msg(_message){
	_len = _msg.size();
}

bool RespMsg::send(int sfd){
	const std::string firstline(_ver+' '+_code+' '+_status+"\r\n");
	if((size_t)::send(sfd, firstline.data(), firstline.size(), 0)!=firstline.size()){printe("send()");return false;}
	_prop["Content-Length"] = std::to_string(_len);
	time_t now;
	time(&now);
	_prop["Date"] = gmtstr(&now);
	for(const auto& property: _prop){
		const std::string propline(property.first+": "+property.second+"\r\n");
		if((size_t)::send(sfd, propline.data(), propline.size(), 0)!=propline.size()){printe("send()");return false;}
	}
	const char lastline[2] = {'\r', '\n'};
	if(::send(sfd, &lastline, 2, 0)!=2){printe("send()");return false;}
	if((size_t)::send(sfd, _msg.data(), _len, 0)!=_len){printe("send()");return false;}
	return true;
}

void RespMsg::setprop(const std::string& pname, const std::string& pvalue){
	_prop[pname] = pvalue;
}

void RespMsg::setstatus(const std::string& code, const std::string& status){
	_code = code;
	_status = status;
}

//RespFile
RespFile::RespFile(std::ifstream& file):_file(file){
	getready();
}

bool RespFile::getready(void){
	if(_file.is_open()){
		_file.seekg(0, _file.end);
		_len = _file.tellg();
		_file.seekg(0, _file.beg);
		_ready = true;
		return true;
	}
	return false;
}

bool RespFile::ready(void){return _ready;}

bool RespFile::send(int sfd){
	const std::string firstline(_ver+' '+_code+' '+_status+"\r\n");
	if((size_t)::send(sfd, firstline.data(), firstline.size(), 0)!=firstline.size()){printe("send()-RespFile-1-");return false;}
	_prop["Content-Length"] = std::to_string(_len);
	time_t now;
	time(&now);
	_prop["Date"] = gmtstr(&now);
	for(const auto& property: _prop){
		const std::string propline(property.first+": "+property.second+"\r\n");
		if((size_t)::send(sfd, propline.data(), propline.size(), 0)!=propline.size()){printe("send()-RespFile-2-");return false;}
	}
	const char lastline[2] = {'\r', '\n'};
	if(::send(sfd, &lastline, 2, 0)!=2){printe("send()-RespFile-3-");return false;}
	char sendbuf[msg_size_max];
	do{
		memset(&sendbuf, 0, sizeof sendbuf);
		_file.read(sendbuf, msg_size_max);
		if(::send(sfd, sendbuf, _file.gcount(), 0)<(int)_file.gcount()){printe("send()-RespFile-4-");return false;}
	}while(_file.gcount()==msg_size_max);
	return true;
}

void RespFile::setprop(const std::string& pname, const std::string& pvalue){
	_prop[pname] = pvalue;
}

std::string RespFile::operator[](const std::string& pname){return _prop[pname];}

//TcpServer
TcpServer::TcpServer(unsigned short _port, bool _ifdetach):port(_port), conn(){
	std::thread start_thread([&](){
		while((fd = socket(AF_INET, SOCK_STREAM, 0))==-1){
			if(stoplisten){std::cout<<"halted"<<std::endl;return false;}
			printe("socket");
			std::cout<<"try socket() in a second"<<std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		std::cout<<"socket created successfully"<<std::endl;
		struct sockaddr_in myaddr;
		memset(&myaddr, 0, sizeof myaddr);
		myaddr.sin_family = AF_INET;
		myaddr.sin_port = htons(port);
		myaddr.sin_addr.s_addr = INADDR_ANY;
		conn.insert(std::make_pair(0, TcpConn(myaddr)));
		conn.erase(0);
		while(!stoplisten&&bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr))==-1){
			printe("bind");
			std::cout<<"try bind() in a second"<<std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		std::cout<<"bound on port "<<port<<std::endl;
		readylisten = true;
		return true;
	});
	if(_ifdetach)start_thread.detach();
	else start_thread.join();
}

bool TcpServer::listen(int _backlog, bool (*_reactfunc)(int, std::map<int,TcpConn>&), bool _ifdetach){
	std::this_thread::sleep_for(std::chrono::seconds(1));
	while(!readylisten){
		if(stoplisten){std::cout<<"halted"<<std::endl;return false;}
		std::cout<<"listen_thread:socket is not ready. try again in 3 seconds."<<std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
	}
	std::thread listen_thread([&](int backlog,bool (*reactfunc)(int, std::map<int,TcpConn>&)){
		while(::listen(fd, backlog)==-1){
			printe("::listen()");
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		setnonblock(fd);/**set listening socket as non-blocking**/
		std::cout<<"server is listening"<<std::endl;
		while(!stoplisten){//start of MAIN WHILE LOOP
			/***construct pollfd array***/
			int num_of_sock = 1 + conn.size();// 1 is for the main listening socket, conn.size() is for all the connections.
			pollfd pfd[num_of_sock];
			pfd[0].fd = fd;
			pfd[0].events = POLLIN;
			int pfd_idx = 1;
			for(const auto& _conn:conn){
				pfd[pfd_idx].fd = _conn.first;
				pfd[pfd_idx].events = POLLIN;
				pfd_idx++;
			}
			/***construct pollfd array***/
			sockaddr_in clientaddr;
			socklen_t clientaddr_size = sizeof(clientaddr);
			int npfd = poll(pfd, num_of_sock, 1000);
			//std::cout<<"polled "<<npfd<<'/'<<num_of_sock<<std::endl;
			for(int p_idx = 0; p_idx<num_of_sock; p_idx++){
				//std::cout<<"idx:"<<p_idx<<",revents:"<<pfd[p_idx].revents<<std::endl;
				if(pfd[p_idx].revents == 0)continue;
				if(pfd[p_idx].revents!=POLLIN){
					if(pfd[p_idx].revents==25){
						conn[pfd[p_idx].fd].ifclose=true;
						continue;
					}
					std::cout<<"poll() error. poll revents:"<<pfd[p_idx].revents<<std::endl;
					halt();
					break;
				}
				if(pfd[p_idx].fd == fd){
					while(1){//new connection loop
						int nfd = accept(fd,(struct sockaddr*)&clientaddr, &clientaddr_size);
						if(nfd==-1){
							if(errno!=EWOULDBLOCK){
								printe("accept()");
								halt();
							}
							break;
						}
						conn_mtx.lock();
						setnonblock(nfd);
						conn.insert(std::make_pair(nfd, TcpConn(clientaddr)));
						conn_mtx.unlock();
					}// end new connection loop
					continue;
				}
				//now the rest of fds are all recv()ing data
				conn_mtx.lock();
				reactfunc(pfd[p_idx].fd, conn);
				conn_mtx.unlock();
			}//end of for loop
			conn_mtx.lock();
			auto conn_it = conn.begin();
			while(conn_it!=conn.end()){
				if((conn_it->second).ifclose){
					close(conn_it->first);
					conn_it = conn.erase(conn_it);
				} else { conn_it++;}
			}
			conn_mtx.unlock();
		}//end of MAIN WHILE LOOP
		for(const auto& cn:conn){
			close(cn.first);
		}
	}, _backlog, _reactfunc);//end of listen_thread;
	std::thread timer_thread([&](){
		std::this_thread::sleep_for(std::chrono::seconds(1));
		while(!stoplisten){
			for(const auto& _conn:conn){
				conn_mtx.lock();
				conn[_conn.first].age++;
				if(_conn.second.age>conn_max_life)conn[_conn.first].ifclose = true;
				conn_mtx.unlock();
			}
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	});
	timer_thread.detach();
	if(_ifdetach)listen_thread.detach();
	else listen_thread.join();
	return true;
}

void TcpServer::halt(void){
	stoplisten = true;
	close(fd);
}

void TcpServer::listconn(void){
	conn_mtx.lock();
	for(const auto& cn:conn){
		std::cout<<cn.first<<" age:"<<conn[cn.first].age<<std::endl;
		//rv+=(std::to_string(cn.first)+'/'+std::to_string(conn.size())+" age:"+std::to_string(cn.second.age)+"\r\n");
	}
	conn_mtx.unlock();
}


//standalone functions:
void printe(const std::string& msg){
	std::cout<<msg<<": "<<strerror(errno)<<std::endl;
}

void setnonblock(int _fd){
	if(fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL, 0)|O_NONBLOCK)==-1){
		printe("fcntl()");
	}
}

std::string gmtstr(time_t *t){
	char ts[30];
	memset(ts, 0, 30);
	tm *tt = gmtime(t);
	strftime(ts, 30, "%a, %d %b %Y %T GMT", tt);
	return std::string(ts);
}

bool sendfile(const std::string& path, int sock, bool ifcache){
	bool rv = false;
	std::ifstream fs(path);
	RespFile rf(fs);
	if(ifcache) rf.setprop("Cache-Control", "max-age=30");
	//type detection	
	if(rf.ready()) rv = rf.send(sock);
	fs.close();
	return rv;
}
