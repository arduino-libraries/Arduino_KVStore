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
    bool begin() { return true; }
    bool end()   { return true; }
    bool clear();

    typename KVStoreInterface<Key>::res_t remove(const Key& key) override;
    bool exists(const Key& key) const override;
    typename KVStoreInterface<Key>::res_t putBytes(const Key& key, uint8_t b[], size_t s) override;
    typename KVStoreInterface<Key>::res_t getBytes(const Key& key, uint8_t b[], size_t s) const override;
    size_t getBytesLength(const Key& key) const override;

private:
    std::map<Key, std::pair<uint8_t*, size_t>> kvmap;
};

template<typename Key>
bool KVStore<Key>::clear() {
    kvmap.clear();

    return true;
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
    std::memset(buf, 0, s);
    std::memcpy(buf, b, s);

    kvmap[key] = {buf, s};

    return s;
}

template<typename Key>
typename KVStoreInterface<Key>::res_t KVStore<Key>::getBytes(const Key& key, uint8_t *b, size_t s) const {
    auto el = kvmap.at(key);

    std::memcpy(b, el.first, s <= el.second? s : el.second);

    return el.second;
}

template<typename Key>
size_t KVStore<Key>::getBytesLength(const Key& key) const {
    auto el = kvmap.at(key);

    return el.second;
}



TEST_CASE( "KVStore can store values of different types, get them and remove them", "[kvstore][putgetremove]" ) {
    KVStore<const char*> store;
    store.begin();

    SECTION( "adding a char and getting it back" ) {
        char value = 'A';

        REQUIRE( store.putChar("0", value) == sizeof(value));
        REQUIRE( store.getChar("0") == value );
        REQUIRE( store.remove("0") == 0 );
    }

    SECTION( "adding a uchar and getting it back" ) {
        unsigned char value = 0x55;

        REQUIRE( store.putUChar("0", value) == sizeof(value));
        REQUIRE( store.getUChar("0") == value );
        REQUIRE( store.remove("0") == 0 );
    }

    SECTION( "adding a short and getting it back" ) {
        short value = 0x5555;

        REQUIRE( store.putShort("0", value) == sizeof(value));
        REQUIRE( store.getShort("0") == value );
        REQUIRE( store.remove("0") == 0 );
    }

    SECTION( "adding an unsigned short and getting it back" ) {
        unsigned short value = 0x5555;

        REQUIRE( store.putUShort("0", value) == sizeof(value));
        REQUIRE( store.getUShort("0") == value );
        REQUIRE( store.remove("0") == 0 );
    }

    SECTION( "adding an uint32_t and getting it back" ) {
        uint32_t value = 0x01020304;

        REQUIRE( store.putUInt("0", value) == sizeof(value));
        REQUIRE( store.getUInt("0") == value );
        REQUIRE( store.remove("0") == 0 );
    }

    SECTION( "adding a string and getting it back" ) {
        char value[] = "pippo";
        char res[6];

        REQUIRE( store.putString("0", value) == strlen(value));

        store.getString("0", res, 6);
        REQUIRE( strcmp(res, value) == 0 );
        REQUIRE( store.remove("0") == 0 );
    }
}



TEST_CASE( "KVStore references are a useful tool to indirectly access kvstore", "[kvstore][references]" ) {
    KVStore<const char*> store;
    store.begin();

    REQUIRE( store.put("0", (uint8_t) 0x55) == 1);
    REQUIRE( store.put("1", (uint16_t) 0x5555) == 2);
    REQUIRE( store.put("2", (uint32_t) 0x55555555) == 4);
    REQUIRE( store.put("3", (uint32_t) 0x55555555) == 4);

    SECTION( "I can get an uint8_t reference and update its value indirectly" ) {
        auto ref = store.get<uint8_t>("0");

        REQUIRE( ref == 0x55 );
        ref = 0x56;

        REQUIRE( store.getUChar("0") == 0x56 );
    }

    SECTION( "I can get an uint16_t reference and update its value indirectly" ) {
        auto ref = store.get<uint16_t>("1");

        REQUIRE( ref == 0x5555 );
        ref = 0x5656;

        REQUIRE( store.getUShort("1") == 0x5656 );
    }

    SECTION( "I can get an uint32_t reference and update its value indirectly" ) {
        auto ref = store.get<uint32_t>("2");

        REQUIRE( ref == 0x55555555 );
        ref = 0x56565656;

        REQUIRE( store.getUInt("2") == 0x56565656 );
    }

    SECTION( "If I update the value from a reference I am able to load it from another one" ) {
        auto ref1 = store.get<uint32_t>("3");
        auto ref2 = store.get<uint32_t>("3");

        REQUIRE(ref1 == 0x55555555);
        REQUIRE(ref2 == 0x55555555);

        ref1 = 0x56565656;

        REQUIRE(ref2 == 0x56565656);
    }
}
