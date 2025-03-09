#ifndef DB_LOCK_H
#define DB_LOCK_H

#include <string>
#include <dirent.h>

class db_lock {
public:
    db_lock(const std::string& root, bool exclusive = true);
    ~db_lock();

private:
    DIR* dir;
};

#endif // DB_LOCK_H
