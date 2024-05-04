#ifndef DATASTORE_H
#define DATASTORE_H

#include <string>

// - - - - - - - - - - - - - - - - 8< - - - - - - - - - - - - - - - - //

class DataStore {
  public:
    virtual ~DataStore() = default;
    virtual void write(const std::string& key, const std::string& value) = 0;
};

// - - - - - - - - - - - - - - - - 8< - - - - - - - - - - - - - - - - //

#endif // DATASTORE_H
