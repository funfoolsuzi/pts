#include "postmgmt.hpp"

YMonth::YMonth(void){
	time_t now;
	time(&now);
	struct tm *nowtm = localtime(&now);
	year = nowtm->tm_year - 100;
	month = nowtm->tm_mon + 1;
}
YMonth::YMonth(int yy, int mm){
	if(yy<1)year = 1;
	else if(yy>100)year = 99;
	else year = yy;
	if(mm<1)month = 1;
	else if(mm>12)month = 12;
	else month = mm;
}
YMonth YMonth::last(void){
	if(month==1){
		if(year==1){std::cout<<"YMonth::last(void):lowest month already"<<std::endl;return *this;}
		return YMonth(year-1,12);
	} else return YMonth(year, month-1);
}
std::string YMonth::to_string(void){
	std::string rv = (year<10&&year>0)?"0":""+std::to_string(year)+((month<10&&month>0)?"0":"")+std::to_string(month);
	return rv;
}

PostHeader::PostHeader(void){}
PostHeader::PostHeader(const std::string& in_str){
	//interprete arg:_content, generate title, tags.
	size_t first_break = in_str.find("\r\n\r\n");
	if(first_break==std::string::npos){throw "PostHeader:input string invalid"; return;}
	std::string tags = in_str.substr(0, first_break);
	title = in_str.substr(first_break+4, in_str.find("\r\n\r\n", first_break+4)-first_break-4);
	//interprete tags, break it into vector of tags
	arrange_tag(tags);
}
bool PostHeader::arrange_tag(const std::string& in_str){
		size_t startpos = 0;
	while(startpos!=std::string::npos){
		if(in_str.size()==0)break;
		size_t next;
		if((next=in_str.find(',', startpos+1))==std::string::npos){
			tag.push_back(in_str.substr(startpos, in_str.size()-startpos));
			break;
		}
		tag.push_back(in_str.substr(startpos, next-startpos));
		startpos = next+1;
	}
	return true;
}

std::string today(void){
	time_t now;
	time(&now);
	struct tm *nowtm = localtime(&now);
	std::string rv = "";
	
	if(nowtm->tm_year<101)rv+="00";
	else if(nowtm->tm_year>200)rv+="99";
	else{
		if(nowtm->tm_year<110) rv+="0";
		rv+=std::to_string(nowtm->tm_year-100);
	}
	
	if(nowtm->tm_mon<9)rv+="0";
	rv+=std::to_string(nowtm->tm_mon+1);
	
	if(nowtm->tm_mday<10)rv+="0";
	rv+=std::to_string(nowtm->tm_mday);
	
	return rv;
}

bool push_id(const std::string& fpath, const std::string& id){
	bool exist = false;
	std::fstream tf(fpath); //target file
	if(tf.is_open()){
		char idbuf[9];
		memset(idbuf, 0, 9);
		while(tf.readsome(idbuf, 8)==8){
			if(strcmp(idbuf, id.c_str())==0){exist = true; break;}
		}
	} else tf.open(fpath, std::fstream::out);
	if(!exist){
		std::cout<<"push:"<<id<<' '<<fpath<<std::endl;
		tf.seekp(0,std::ios::end);
		tf<<id;
	}
	tf.close();
	return true;
}
bool pull_id(const std::string& fpath, const std::string& id){
	std::cout<<"triggering pull_id   "<<fpath<<std::endl;
	std::fstream tf(fpath, std::fstream::in); //target file
	if(!tf.is_open())return false;
	std::string list;
	char cbuf[8];
	while(1){
		tf.read(cbuf, 8);
		if(tf.eof())break;
		std::string sbuf(cbuf);
		if(sbuf==id){std::cout<<"pull:"<<id<<' '<<fpath<<std::endl;continue;}
		list+=sbuf;
	}
	tf.close();
	tf.open(fpath, std::fstream::out|std::fstream::trunc);
	tf<<list;
	tf.close();
	return true;
}

bool savepost(const std::string& _id, const std::string& _content){
	//interprete arg:_id, generate month.
	std::string month = _id.substr(0,4);
	//interprete arg:_content, generate pheader.
	PostHeader pheader(_content);
	//check if id exists, delete id from old tag-map if necessary
	std::ifstream previous("./posts/"+_id);
	if(previous.is_open()){
		std::string sbuf;
		while(sbuf.size()<512){
			char cbuf = previous.get();
			if(cbuf=='\r')break;
			sbuf+=cbuf;
		}
		PostHeader prev_header;
		prev_header.arrange_tag(sbuf);
		////now compare pheader:prev_header
		for(unsigned int idx = 0; idx<prev_header.tag.size(); idx++){
			bool hasdup = false;
			for(unsigned int jdx = 0; jdx<pheader.tag.size(); jdx++){if(prev_header.tag[idx]==pheader.tag[jdx]){hasdup=true;break;}}
			if(!hasdup)pull_id("./posts/tag/"+prev_header.tag[idx]+".dat", _id);
		}
	}
	//save entire content to the file named by id.
	std::ofstream target("./posts/"+_id, std::fstream::trunc);
	target<<_content;
	target.close();
	
	//detecting and inserting month-file
	push_id("./posts/month/"+month+".dat", _id);
	//detecting and inserting tag-file
	for(size_t idx = 0; idx<pheader.tag.size(); idx++){
		push_id("./posts/tag/"+pheader.tag[idx]+".dat", _id);
	}
	return true;
}

bool savepost(const std::string& _id, int sockfd){
	//interprete arg:_id, generate month.
	std::string month = _id.substr(0,4);
	//read from sockfd. generate pheader
	char cbuf;
	std::string headerstring;
	int counter = 0;
	int trys = 0;
	while(counter<8&&headerstring.size()<1024){
		try{cbuf=sock_recvchar(sockfd);}
		catch(int excepno){
			if(excepno!=100||trys>3)return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}
		headerstring+=cbuf;
		if(cbuf!=13&&cbuf!=10){
			if(counter>0&&counter<4)counter=0;
			else if(counter>4&&counter<8)counter=4;
			continue;
		}
		if(cbuf==13&&counter%2==0)counter++;
		else if(cbuf==10&&counter%2==1)counter++;
	}
	if(headerstring.size()>=1024){std::cout<<"post header input too long"<<std::endl;return false;}
	PostHeader pheader(headerstring);
	//check if id exists, delete id from old tag-map if necessary
	std::ifstream previous("./posts/"+_id);
	if(previous.is_open()){
		std::string sbuf;
		while(sbuf.size()<512){
			char cbuf = previous.get();
			if(cbuf=='\r')break;
			sbuf+=cbuf;
		}
		PostHeader prev_header;
		prev_header.arrange_tag(sbuf);
		////now compare pheader:prev_header
		for(unsigned int idx = 0; idx<prev_header.tag.size(); idx++){
			bool hasdup = false;
			for(unsigned int jdx = 0; jdx<pheader.tag.size(); jdx++){if(prev_header.tag[idx]==pheader.tag[jdx]){hasdup=true;break;}}
			if(!hasdup)pull_id("./posts/tag/"+prev_header.tag[idx]+".dat", _id);
		}
	}
	//save entire file from socket, including header string.
	std::ofstream target("./posts/"+_id, std::fstream::trunc);
	target<<headerstring;
	char insertbuf[2048];
	int byterecv;
	while((byterecv = recv(sockfd, insertbuf, 2048, 0))!=-1){
		target.write(insertbuf, byterecv);
	}
	if(byterecv==-1&&errno!=EWOULDBLOCK&&errno!=0){
		std::cout<<"bool savepost(const std::string& _id, int sockfd):"<<strerror(errno)<<std::endl;
		target.flush();//if something goes wrong, flush the ofstream buffer and then close it.!!! don't save a bad edit.
		target.close();
		return false;
	}
	target.close();
	//detecting and inserting month-file
	push_id("./posts/month/"+month+".dat", _id);
	//detecting and inserting month-file
	for(size_t idx = 0; idx<pheader.tag.size(); idx++){
		push_id("./posts/tag/"+pheader.tag[idx]+".dat", _id);
	}
	return true;
}
