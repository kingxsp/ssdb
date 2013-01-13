#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "repl.h"
#include "util/log.h"
#include "util/strings.h"

#ifdef NDEBUG
	static const int LOG_QUEUE_SIZE  = 1000 * 1000;
#else
	static const int LOG_QUEUE_SIZE  = 5;
#endif

MyReplication::MyReplication(leveldb::DB* meta_db){
	logs = new PersistentSyncLogQueue(meta_db);
	/*
	logs = new MemorySyncLogQueue(backlog);
	p_logs = new PersistentSyncLogQueue(meta_db);

	// load persistent sync logs into SyncLogQueue
	if(p_logs->seq_min > 0){
		uint64_t seq = p_logs->seq_min;
		if(p_logs->seq_min + backlog < p_logs->seq_max){
			seq = p_logs->seq_max - backlog;
		}
		for(; seq<=p_logs->seq_max; seq++){
			SyncLog log;
			int ret = p_logs->find(seq, &log);
			if(ret != 1){
				log_error("error loading sync logs from leveldb, seq: %llu", seq);
				break;
			}
			log_trace("load %llu, key: %s", seq, hexmem(log.key().data(), log.key().size()).c_str());
			logs->put(log);
		}
	}
	*/
}

MyReplication::~MyReplication(){
	delete logs;
	//delete p_logs;
}

void MyReplication::Put(uint64_t seq, const leveldb::Slice& key, const leveldb::Slice& val){
	SyncLog log(seq, SyncLog::SET, key);
	log_trace("%llu, set %s", seq, hexmem(key.data(), key.size()).c_str());
	logs->put(log);
}

void MyReplication::Delete(uint64_t seq, const leveldb::Slice& key){
	SyncLog log(seq, SyncLog::DEL, key);
	log_trace("%llu, del %s", seq, hexmem(key.data(), key.size()).c_str());
	logs->put(log);
}

/* SyncLog */

SyncLog::SyncLog(uint64_t seq, char type, const leveldb::Slice &key){
	buf.append((char *)(&seq), sizeof(uint64_t));
	buf.push_back(type);
	buf.append(key.data(), key.size());
}

uint64_t SyncLog::seq() const{
	return *((uint64_t *)(buf.data()));
}

char SyncLog::type() const{
	return buf[sizeof(uint64_t)];
}

const Bytes SyncLog::key() const{
	return Bytes(buf.data() + KEY_POS, buf.size() - KEY_POS);
}

/* SyncLogQueue */

int SyncLogQueue::find(uint64_t seq, SyncLog *log){
	Locking l(&mutex);

	if(this->size == 0 || seq > this->seq_max){
		return 0;
	}
	int pos;
	if(seq < this->seq_min){
		pos = this->start;
	}else{
		int offset = (int)(seq - this->seq_min);
		if(offset >= this->size){
			offset = this->size - 1;
		}
		pos = this->index(offset);
	}
	return find_by_pos(pos, log);
}

int SyncLogQueue::find_last(SyncLog *log){
	Locking l(&mutex);
	log_debug("");

	if(this->size == 0){
		return 0;
	}
	int pos = this->index(this->size - 1);
	return find_by_pos(pos, log);
}

/**/

MemorySyncLogQueue::MemorySyncLogQueue(){
	this->total = LOG_QUEUE_SIZE;
	this->seq_min = 0;
	this->seq_max = 0;
	this->size = 0;
	this->start = 0;
	this->ptr = new SyncLog[this->total];
}

MemorySyncLogQueue::~MemorySyncLogQueue(){
	delete[] this->ptr;
}

int MemorySyncLogQueue::find_by_pos(int pos, SyncLog *log){
	assert(pos < this->total);
	*log = this->ptr[pos];
	return 1;
}

void MemorySyncLogQueue::put(const SyncLog &log){
	Locking l(&mutex);

	int idx = this->index(this->size);
	this->ptr[idx] = log;
	if(this->size < this->total){
		if(this->size == 0){
			this->seq_min = log.seq();
		}
		this->size ++;
	}else{
		this->seq_min ++;
		this->start = this->index(+1);
	}
	this->seq_max = log.seq();
	log_trace("start: %d, size: %d, min: %llu, max: %llu",
		start, size, seq_min, seq_max);
}


/* PersistentSyncLogQueue */

static inline std::string encode_key(int pos){
	pos = big_endian((uint32_t)pos);
	std::string ret = "sync|";
	ret.append((char *)&pos, sizeof(int));
	return ret;
}

uint64_t PersistentSyncLogQueue::find_seq_at_pos(int pos){
	SyncLog log;
	if(find_by_pos(pos, &log) == 0){
		return 0;
	}
	return log.seq();
}

int PersistentSyncLogQueue::find_most_greater_seq_pos(uint64_t low_seq, int spos, int epos){
	while(spos < epos){
		int pos = (spos + epos)/2 + 1;
		uint64_t seq = find_seq_at_pos(pos);
		if(seq > low_seq){
			spos = pos;
		}else{
			epos = pos - 1;
		}
	}
	return spos;
}

PersistentSyncLogQueue::PersistentSyncLogQueue(leveldb::DB *meta_db){
	this->total = LOG_QUEUE_SIZE;
	this->seq_min = 0;
	this->seq_max = 0;
	this->size = 0;
	this->start = 0;
	this->meta_db = meta_db;

	// TEST:
	//std::string key_buf = encode_key(3);
	//meta_db->Delete(leveldb::WriteOptions(), key_buf);

	// load persitent sync logs from leveldb
	// sync logs are saved in leveldb circularly
	uint64_t first_seq;
	first_seq = find_seq_at_pos(0);
	if(first_seq == 0){
		// empty
		// idx: 0  - 1  - 2  - (total-1)
		// seq: ni - ni - ni - ni
	}else{
		int pos = find_most_greater_seq_pos(first_seq, 0, this->total - 1);
		if(pos == this->total - 1){
			// idx: 0  - 1  - 2  - (total-1)
			// seq: s0 - s1 - s2 - s3
			this->start = 0;
			this->size = this->total;
		}else{
			uint64_t next = find_seq_at_pos(pos + 1);
			if(next == 0){
				// idx: 0  - 1  - 2  - (total-1)
				// seq: s0 - s1 - ni - ni
				this->start = 0;
				this->size = pos + 1;
			}else{
				// idx: 0  - 1  - 2  - (total-1)
				// seq: s2 - s3 - s0 - s1
				this->start = pos + 1;
				this->size = this->total;
			}
		}
	}

	if(this->size > 0){
		this->seq_min = find_seq_at_pos(this->start);
		this->seq_max = find_seq_at_pos(this->index(this->size - 1));
	}
	log_debug("p_logs idx:%d, len:%d, total:%d, min:%llu, max:%llu",
		this->start, this->size, this->total,
		this->seq_min, this->seq_max);
}

PersistentSyncLogQueue::~PersistentSyncLogQueue(){
}

void PersistentSyncLogQueue::put(const SyncLog &log){
	Locking l(&mutex);
	int idx = this->index(this->size);

	std::string key_buf = encode_key(idx);
	leveldb::Slice b(log.data(), log.size());
	leveldb::Status s;
	s = meta_db->Put(write_options, key_buf, b);
	if(!s.ok()){
		log_error("meta_db set error: %s", s.ToString().c_str());
	}else{
		if(this->size < this->total){
			if(this->size == 0){
				this->seq_min = log.seq();
			}
			this->size ++;
		}else{
			this->seq_min ++;
			this->start = this->index(+1);
		}
	}
	this->seq_max = log.seq();
	log_trace("start: %d, size: %d, min: %llu, max: %llu",
		start, size, seq_min, seq_max);
}

int PersistentSyncLogQueue::find_by_pos(int pos, SyncLog *log){
	leveldb::ReadOptions read_options;
	std::string val;
	//log_debug("get pos: %d", pos);

	std::string key_buf = encode_key(pos);
	leveldb::Status s = meta_db->Get(read_options, key_buf, &val);
	if(s.IsNotFound()){
		return 0;
	}
	if(s.ok()){
		if(val.size() < SyncLog::MIN_SIZE){
			return 0;
		}
		*log = SyncLog(val);
		return 1;
	}
	return 0;
}
