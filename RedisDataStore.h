#ifndef REDISDATASTORE_H
#define REDISDATASTORE_H

#include <string>
#include <iostream>

#include <DataStore.h>

#include <sw/redis++/redis++.h>
#include <sw/redis++/patterns/redlock.h>
#include <sw/redis++/errors.h>

// - - - - - - - - - - - - - - - - 8< - - - - - - - - - - - - - - - - //

class RedisDataStore: public DataStore {
  public:
    RedisDataStore (std::string serverUri) {
      if (!serverUri.empty()) {
	fClient = std::make_shared<sw::redis::Redis>(serverUri);
      }
      fPipe = std::make_unique<sw::redis::Pipeline>(std::move(fClient->pipeline()));
    }
    void write(const std::string& key, const std::string& value) override {
      fClient->set(key,value);
    }
    void ts_add(const std::string& key, const std::string& timestamp, const std::string& value) {
      fClient->command("ts.add", key, timestamp, value);
    }
    void hset(const std::string& hash, const std::string& field, const std::string& value) {
      fClient->hset(hash, field, value);
    }
    void del(const std::string& key) {
      fClient->del(key);
    }
    void del(const std::vector<std::string>& keys) {
      fClient->del(keys.begin(),keys.end());
    }

  private:
    std::shared_ptr<sw::redis::Redis> fClient;
    std::unique_ptr<sw::redis::Pipeline> fPipe;
};

// - - - - - - - - - - - - - - - - 8< - - - - - - - - - - - - - - - - //

#endif // REDISDATASTORE_H
