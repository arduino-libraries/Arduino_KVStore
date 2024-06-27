/*
 * This file is part of ArduinoIoTCloud.
 *
 * Copyright 2019 ARDUINO SA (http://www.arduino.cc/)
 *
 * This software is released under the GNU General Public License version 3,
 * which covers the main part of arduino-cli.
 * The terms of this license can be found at:
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * You can be released from the requirements of the above licenses by purchasing
 * a commercial license. Buying such a license is mandatory if you want to modify or
 * otherwise use the software for commercial activities involving the Arduino
 * software without disclosing the source code of your own applications. To purchase
 * a commercial license, send an email to license@arduino.cc.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h> // TODO understand if this raises flash considerabily

template<typename Key=const char*>
class KVStoreInterface {
public:

    typedef Key key_t;
    typedef uint8_t res_t;

    // proxy class to allow operator[] with assignment
    template<typename T>
    class reference {
    public:
        constexpr reference(const key_t &key, T value): key(key), value(value) {}

        // assign a new value to the reference and update the store
        reference& operator=(T t) noexcept {
            value = t;
            return *this;
        }

        // assign a new value to the reference copyiong from another reference value
        reference& operator=(const reference<T>& r) noexcept {
            value = r.value;
            return *this;
        }

        // get the referenced value
        T operator*() noexcept       { return getValue(); }

        // cast the reference to the value it contains -> get the value references
        operator T () const noexcept { return getValue(); }

        inline key_t getKey() const  { return key; }
        inline T getValue() const    { return value; }

        // load the stored value
        void load() const            { value = get<T>(key).value; }

        // save the value contained in this reference
        void save()                  { put(key, value); }

        // check if this reference is contained in the store
        bool exists() const          { return exists(key); }

        // remove this reference from the store
        void remove()                { remove(key); }
    private:
        const key_t key;
        T value;
    };
    // static constexpr reference<void*> NullReference = reference<void*>(0, nullptr);

    virtual ~KVStoreInterface() {};

    virtual void begin() = 0; // FIXME consider error during initialization
    virtual void end() = 0;
    virtual void clear() = 0;

    // pure virtual methods that need to be implemented
    virtual res_t remove(const key_t& key) = 0;
    virtual bool exists(const key_t& key) const = 0;
    virtual res_t putBytes(const key_t& key, uint8_t b[], size_t s) = 0;
    virtual res_t getBytes(const key_t& key, uint8_t b[], size_t s) const = 0;
    virtual size_t getBytesLength(key_t key) const = 0;

    template<typename T> // TODO define res_t
    inline res_t put(const key_t& key, T value) { return putBytes(key, (uint8_t*)&value, sizeof(value)); }

    template<typename T>
    inline reference<T> get(const key_t& key, T def = 0) const {
        if(exists(key)) {
            T t;
            getBytes(key, (uint8_t*)&t, sizeof(t));
            return reference<T>(key, t);
        } else {
            return reference<T>(key, def);
        }
    }

    template<typename T>
    reference<T>& operator[](const key_t& key) { // write access to the value
        return get<T>(key);
    }

    template<typename T>
    const reference<T>& operator[](const key_t& key) const { // ro access to the value
        return get<T>(key);
    }

    virtual size_t      putChar(key_t key, int8_t value)                        { return put(key, value); }
    virtual size_t      putUChar(key_t key, uint8_t value)                      { return put(key, value); }
    virtual size_t      putShort(key_t key, int16_t value)                      { return put(key, value); }
    virtual size_t      putUShort(key_t key, uint16_t value)                    { return put(key, value); }
    virtual size_t      putInt(key_t key, int32_t value)                        { return put(key, value); }
    virtual size_t      putUInt(key_t key, uint32_t value)                      { return put(key, value); }
    virtual size_t      putLong(key_t key, int32_t value)                       { return put(key, value); }
    virtual size_t      putULong(key_t key, uint32_t value)                     { return put(key, value); }
    virtual size_t      putLong64(key_t key, int64_t value)                     { return put(key, value); }
    virtual size_t      putULong64(key_t key, uint64_t value)                   { return put(key, value); }
    virtual size_t      putFloat(key_t key, float value)                        { return put(key, value); }
    virtual size_t      putDouble(key_t key, double value)                      { return put(key, value); }
    virtual size_t      putBool(key_t key, bool value)                          { return put(key, value); }
    virtual size_t      putString(key_t key, const char* value)                 { return putBytes(key, (uint8_t*)value, strlen(value)); }
    // virtual size_t      putString(key_t key, String value)      { return put(key, value); } // TODO verify this

    virtual int8_t      getChar(key_t key, int8_t defaultValue = 0) const       { return get(key, defaultValue); }
    virtual uint8_t     getUChar(key_t key, uint8_t defaultValue = 0) const     { return get(key, defaultValue); }
    virtual int16_t     getShort(key_t key, int16_t defaultValue = 0) const     { return get(key, defaultValue); }
    virtual uint16_t    getUShort(key_t key, uint16_t defaultValue = 0) const   { return get(key, defaultValue); }
    virtual int32_t     getInt(key_t key, int32_t defaultValue = 0) const       { return get(key, defaultValue); }
    virtual uint32_t    getUInt(key_t key, uint32_t defaultValue = 0) const     { return get(key, defaultValue); }
    virtual int32_t     getLong(key_t key, int32_t defaultValue = 0) const      { return get(key, defaultValue); }
    virtual uint32_t    getULong(key_t key, uint32_t defaultValue = 0) const    { return get(key, defaultValue); }
    virtual int64_t     getLong64(key_t key, int64_t defaultValue = 0) const    { return get(key, defaultValue); }
    virtual uint64_t    getULong64(key_t key, uint64_t defaultValue = 0) const  { return get(key, defaultValue); }
    virtual float       getFloat(key_t key, float defaultValue = NAN) const     { return get(key, defaultValue); }
    virtual double      getDouble(key_t key, double defaultValue = NAN) const   { return get(key, defaultValue); }
    virtual bool        getBool(key_t key, bool defaultValue = false) const     { return get(key, defaultValue); }
    virtual size_t      getString(key_t key, char* value, size_t maxLen) const  { return getBytes(key, (uint8_t*)value, maxLen); }
    // virtual String      getString(key_t key, String defaultValue = String()) const;
};
