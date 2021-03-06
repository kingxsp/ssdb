/* kv */

int CommandProc::proc_exists(const Link &link, const Request &req, Response *resp){
	if(req.size() < 2){
		resp->push_back("client_error");
	}else{
		const Bytes key = req[1];
		std::string val;
		int ret = ssdb->get(key, &val);
		if(ret == 1){
			resp->push_back("ok");
			resp->push_back("1");
		}else if(ret == 0){
			resp->push_back("ok");
			resp->push_back("0");
		}else{
			resp->push_back("error");
			resp->push_back("0");
		}
	}
	return 0;
}

int CommandProc::proc_multi_exists(const Link &link, const Request &req, Response *resp){
	if(req.size() < 2){
		resp->push_back("client_error");
	}else{
		resp->push_back("ok");
		for(Request::const_iterator it=req.begin()+1; it!=req.end(); it++){
			const Bytes key = *it;
			std::string val;
			int ret = ssdb->get(key, &val);
			resp->push_back(key.String());
			if(ret == 1){
				resp->push_back("1");
			}else if(ret == 0){
				resp->push_back("0");
			}else{
				resp->push_back("0");
			}
		}
	}
}

int CommandProc::proc_set(const Link &link, const Request &req, Response *resp){
	if(req.size() < 3){
		resp->push_back("client_error");
	}else{
		int ret = ssdb->set(req[1], req[2]);
		if(ret == -1){
			resp->push_back("error");
		}else{
			resp->push_back("ok");
		}
	}
	return 0;
}

int CommandProc::proc_multi_set(const Link &link, const Request &req, Response *resp){
	if(req.size() < 3 || req.size() % 2 != 1){
		resp->push_back("client_error");
	}else{
		int ret = ssdb->multi_set(req, 1);
		if(ret == -1){
			resp->push_back("error");
		}else{
			resp->push_back("ok");
			char buf[20];
			sprintf(buf, "%d", ret);
			resp->push_back(buf);
		}
	}
	return 0;
}

int CommandProc::proc_multi_del(const Link &link, const Request &req, Response *resp){
	if(req.size() < 2){
		resp->push_back("client_error");
	}else{
		int ret = ssdb->multi_del(req, 1);
		if(ret == -1){
			resp->push_back("error");
		}else{
			resp->push_back("ok");
			char buf[20];
			sprintf(buf, "%d", ret);
			resp->push_back(buf);
		}
	}
	return 0;
}

int CommandProc::proc_multi_get(const Link &link, const Request &req, Response *resp){
	if(req.size() < 2){
		resp->push_back("client_error");
	}else{
		resp->push_back("ok");
		for(int i=1; i<req.size(); i++){
			std::string val;
			int ret = ssdb->get(req[i], &val);
			if(ret == 1){
				resp->push_back(req[i].String());
				resp->push_back(val);
			}else if(ret == 0){
				//
			}else{
				// error
				log_error("fail");
			}
		}
	}
	return 0;
}

int CommandProc::proc_get(const Link &link, const Request &req, Response *resp){
	if(req.size() < 2){
		resp->push_back("client_error");
	}else{
		std::string val;
		int ret = ssdb->get(req[1], &val);
		if(ret == 1){
			resp->push_back("ok");
			resp->push_back(val);
		}else if(ret == 0){
			resp->push_back("not_found");
		}else{
			log_error("fail");
			resp->push_back("fail");
		}
	}
	return 0;
}

int CommandProc::proc_del(const Link &link, const Request &req, Response *resp){
	if(req.size() < 2){
		resp->push_back("client_error");
	}else{
		int ret = ssdb->del(req[1]);
		if(ret == -1){
			resp->push_back("error");
		}else{
			resp->push_back("ok");
		}
	}
	return 0;
}

int CommandProc::proc_scan(const Link &link, const Request &req, Response *resp){
	if(req.size() < 4){
		resp->push_back("client_error");
	}else{
		int limit = req[3].Int();
		KIterator *it = ssdb->scan(req[1], req[2], limit);
		resp->push_back("ok");
		while(it->next()){
			resp->push_back(it->key);
			resp->push_back(it->val);
		}
		delete it;
	}
	return 0;
}

int CommandProc::proc_rscan(const Link &link, const Request &req, Response *resp){
	if(req.size() < 4){
		resp->push_back("client_error");
	}else{
		int limit = req[3].Int();
		KIterator *it = ssdb->rscan(req[1], req[2], limit);
		resp->push_back("ok");
		while(it->next()){
			resp->push_back(it->key);
			resp->push_back(it->val);
		}
		delete it;
	}
	return 0;
}

int CommandProc::proc_keys(const Link &link, const Request &req, Response *resp){
	if(req.size() < 4){
		resp->push_back("client_error");
	}else{
		int limit = req[3].Int();
		KIterator *it = ssdb->scan(req[1], req[2], limit);
		it->return_val(false);

		resp->push_back("ok");
		while(it->next()){
			resp->push_back(it->key);
		}
		delete it;
	}
	return 0;
}

// dir := +1|-1
static int _incr(const SSDB *ssdb, const Request &req, Response *resp, int dir){
	if(req.size() <= 1){
		resp->push_back("client_error");
	}else{
		std::string new_val;
		int64_t val = 1;
		if(req.size() > 2){
			val = req[2].Int64();
		}
		int ret = ssdb->incr(req[1], dir * val, &new_val);
		if(ret == -1){
			resp->push_back("error");
		}else{
			resp->push_back("ok");
			resp->push_back(new_val);
		}
	}
	return 0;
}

int CommandProc::proc_incr(const Link &link, const Request &req, Response *resp){
	return _incr(ssdb, req, resp, 1);
}

int CommandProc::proc_decr(const Link &link, const Request &req, Response *resp){
	return _incr(ssdb, req, resp, -1);
}


