#include "t_kv.h"

int SSDB::set(const Bytes &key, const Bytes &val) const{
	std::string buf = encode_kv_key(key);

	leveldb::Status s = db->Put(write_options, buf, val.Slice());
	if(!s.ok()){
		log_error("set error: %s", s.ToString().c_str());
		return -1;
	}
	return 1;
}

int SSDB::get(const Bytes &key, std::string *val) const{
	std::string buf = encode_kv_key(key);

	leveldb::Status s = db->Get(read_options, buf, val);
	if(s.IsNotFound()){
		return 0;
	}
	if(!s.ok()){
		log_error("get error: %s", s.ToString().c_str());
		return -1;
	}
	return 1;
}

int SSDB::del(const Bytes &key) const{
	std::string buf = encode_kv_key(key);

	leveldb::Status s = db->Delete(write_options, buf);
	if(!s.ok()){
		log_error("del error: %s", s.ToString().c_str());
		return -1;
	}
	return 1;
}

KIterator* SSDB::scan(const Bytes &start, const Bytes &end, int limit) const{
	std::string key_start, key_end;

	key_start = encode_kv_key(start);
	if(end.empty()){
		key_end = "";
	}else{
		key_end = encode_kv_key(end);
	}
	//dump(key_start.data(), key_start.size(), "scan.start");
	//dump(key_end.data(), key_end.size(), "scan.end");

	return new KIterator(this->iterator(key_start, key_end, limit));
}

int SSDB::incr(const Bytes &key, int64_t by, std::string *new_val) const{
	int64_t val;
	std::string old;
	int ret = this->get(key, &old);
	if(ret == -1){
		return -1;
	}else if(ret == 0){
		val = by;
	}else{
		val = str_to_int64(old.data(), old.size()) + by;
	}

	*new_val = int64_to_str(val);
	return this->set(key, *new_val);
}
