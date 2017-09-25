
#include <iostream>
#include "pts.hpp"
#include "postmgmt.hpp"
#include <cstdlib>
#include <sys/stat.h>

using namespace std;

//head<
void replymsg(string msg, int sfd);
bool sockhandle(int sfd, map<int, TcpConn>& _conn);
bool sendlist(const string& listname, int sockfd, bool ifmonth);
bool replypost(int sockfd, const string& p);
bool login(const std::string&, const sockaddr_in&);
char randomalphanum(void);
string createpost(int sfd);
string nextmonth(int numoftime = 1);
//>head
RespMsg fof;
string sessionid;
sockaddr_in sessionaddr;

void replymsg(string msg, int sfd){
	RespMsg m(msg);
	m.send(sfd);
}

bool sendlist(const string& listname, int sockfd, bool ifmonth){
	//need to fix
	string path = "./posts/";
	path+=(ifmonth?"month/":"tag/");
	path+=(listname+".dat");
	ifstream f(path);
	if(!f.is_open()){
		cout<<"sendlist():file not open"<<endl;
		return false;
	}
	RespFile resf(f);
	if(!resf.send(sockfd)){
		f.close();
		cout<<"sendlist():failed to send"<<endl;
		return false;
	}
	f.close();
	return true;
}

bool replypost(int sockfd, const string& post_id){
	ifstream f("./posts/"+post_id);
	if(!f.is_open())return false;
	RespFile resf(f);
	bool result;
	if(resf.send(sockfd))result = true;
	else{cout<<"replypost():file not open"<<endl;result = false;}
	f.close();
	return result;
}

bool login(const string& info, const sockaddr_in& ss){
	ifstream record("./master/login");
	if(!record.is_open())return false;
	for(size_t idx=0; idx<info.size(); idx++){
		if(info[idx]==record.get())continue;
		record.close();return false;
	}
	sessionaddr = ss;
	sessionid = "";
	while(sessionid.size()<10){
		sessionid+=randomalphanum();
	}
	cout<<sessionid<<endl;
	return true;
}

string createpost(int sfd){
	string rv = today();
	struct stat buf;
	for(int idx = 0; idx<100; idx++){
		rv += (((idx<10)?"0":"")+to_string(idx));
		string path = ("./posts/"+rv);
		if(stat(path.c_str(), &buf)!=0){
			savepost(rv, sfd);
			break;
		}
		rv.pop_back();rv.pop_back();
	}
	if(rv.size()==6)return "";
	return rv;
}

bool sockhandle(int sfd, map<int, TcpConn>& _conn){
	ReqHeader req;
	if(!req.readfromsock(sfd)){
		_conn[sfd].ifclose = true;
		return false; 
	}
	time_t tnow;
	time(&tnow);
	cout<<req.method()<<' '<<req.path()<<' '<<req.query_str()<<" addr:"<<_conn[sfd].to_string()<<' '<<ctime(&tnow);
	if(req.method()=="GET"){
		string reqfilepath;
		//make sure it does not try to access any file outside the directory
		if(req.path().find("..")!=string::npos){fof.send(sfd);return true;}
		/*page switch*/
		if(req.path()=="/")reqfilepath="./web/index.html";
		else if(req.path().find("/list/")==0){
			try{
				string t = req.path().substr(6);
				string m = nextmonth(stoi(t));
				if(m!=""){
					if(sendlist(m, sfd, true))return true;
				} else { replymsg("NO", sfd);return true;}
			} catch(invalid_argument){cout<<"/list/ poblem"<<endl;}
		} else if(req.path().find("/post/")==0){
			if(replypost(sfd, req.path().substr(6))) return true;
		} else if(req.path().find("/master/edit/")==0){
			if(sendfile("./web/master/edit.html", sfd)) return true;
		}
		else reqfilepath=("./web"+req.path());
		/*page switch ends*/
		if(sendfile(reqfilepath, sfd, true)) return true;
	} else if(req.method()=="POST"){
		if(req.path()=="/master/login"){
			string login_info = req.content();
			if(login(login_info, _conn[sfd].addr)){
				RespMsg m("login_succ");
				m.setprop("Set-Cookie", "sid="+sessionid);
				m.send(sfd);
			} else {
				replymsg("login_fail", sfd);
			}
			return true;
		}else if(req.path()=="/master/logincheck"){
			if(req["Sid"]==sessionid&&sessionid.size()!=0)replymsg("OK", sfd);
			else replymsg("No", sfd);
			return true;
		}
	} else if(req.method()=="PUT"){
		if(req.path()=="/master/create"){
			if(req["Sid"]==sessionid&&sessionid.size()!=0){
				std::string id = createpost(sfd);
				replymsg(id, sfd);
			} else replymsg("No", sfd);
			return true;
		} else if(req.path().find("/master/save/")==0){
			if(req["Sid"]==sessionid&&sessionid.size()!=0){
				std::string id = req.path().substr(13, 8);
				if(!savepost(id, sfd))replymsg("FAIL", sfd);
				replymsg("Yes", sfd);
			} else replymsg("NA", sfd);
			return true;
		}
	}
	
	fof.send(sfd);
	return false;
}

char randomalphanum(void){
	int n = rand()%36+48;
	if(n>57) n+=39;
	return n;
}

string nextmonth(int numoftime){
	YMonth mon;
	struct stat buf;
	for(int jdx = 0; jdx<numoftime; jdx++){
		bool iffound = false;
		for(int idx = 0; idx<50; idx++){
			if(stat(("./posts/month/"+mon.to_string()+".dat").c_str(),&buf)==0){
				iffound = true;
				break;
			} else { mon = mon.last();}
		}
		if(iffound&&jdx<numoftime-1)mon = mon.last();
		if(!iffound)return "";
	}
	return mon.to_string();
}

int main(void){
	fof.setstatus("404","NotFound");
	TcpServer tsmain(8989, false);
	tsmain.listen(5, sockhandle);
	string userinput;
	while(1){
		cin>>userinput;
		if(userinput=="q"){
			tsmain.halt();
			this_thread::sleep_for(chrono::seconds(2));
			break;
		}else if(userinput=="l"){
			tsmain.listconn();
		}
	}
	return 0;
}
