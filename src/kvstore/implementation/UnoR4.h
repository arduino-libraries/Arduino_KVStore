#include "../kvstore.h"

class Unor4KVStore: public KVStoreInterface {
public:
    void begin();
    void end();
    void clear();

    typename KVStoreInterface<Key>::res_t remove(const Key& key) override;
    bool exists(const Key& key) const override;
    typename KVStoreInterface<Key>::res_t putBytes(const Key& key, uint8_t b[], size_t s) override;
    typename KVStoreInterface<Key>::res_t getBytes(const Key& key, uint8_t b[], size_t s) const override;
    size_t getBytesLength(Key key) const override;

}
