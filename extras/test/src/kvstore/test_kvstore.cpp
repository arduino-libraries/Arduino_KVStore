#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include <kvstore/kvstore.h>
#include <map>
#include <cstdint>
#include <cstring>
#include <stdexcept>

template<typename Key=const char*>
class KVStore: public KVStoreInterface<Key> {
public:
    void begin() {}
    void end()   {}
    void clear();

    typename KVStoreInterface<Key>::res_t remove(const Key& key) override;
    bool exists(const Key& key) const override;
    typename KVStoreInterface<Key>::res_t putBytes(const Key& key, uint8_t b[], size_t s) override;
    typename KVStoreInterface<Key>::res_t getBytes(const Key& key, uint8_t b[], size_t s) const override;
    size_t getBytesLength(Key key) const override;

private:
    std::map<Key, std::pair<uint8_t*, size_t>> kvmap;
};

template<typename Key>
void KVStore<Key>::clear() {
    kvmap.clear();
}

template<typename Key>
typename KVStoreInterface<Key>::res_t KVStore<Key>::remove(const Key& key) {
    kvmap.erase(key);

    return 0;
}

template<typename Key>
bool KVStore<Key>::exists(const Key& key) const {
    try {
        kvmap.at(key);
        return true;
    } catch(const std::out_of_range&) {
        return false;
    }
}

template<typename Key>
typename KVStoreInterface<Key>::res_t KVStore<Key>::putBytes(const Key& key, uint8_t b[], size_t s) {
    uint8_t* buf = new uint8_t[s];
    std::memcpy(buf, b, s);

    kvmap[key] = {b, s};

    return 0;
}

template<typename Key>
typename KVStoreInterface<Key>::res_t KVStore<Key>::getBytes(const Key& key, uint8_t b[], size_t s) const {
    auto el = kvmap.at(key);

    std::memcpy(b, el.first, s <= el.second? s : el.second);
    return 0;
}

template<typename Key>
size_t KVStore<Key>::getBytesLength(Key key) const {
    auto el = kvmap.at(key);

    return el.second;
}



TEST_CASE( "KVStore can store byte array", "[kvstore][bytearray]" ) {
    KVStore Store;



    SECTION( "" ) {

    }
}
