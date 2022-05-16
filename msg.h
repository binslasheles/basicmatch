#ifndef __SC_MSG_H__
#define __SC_MSG_H__
#include <string.h>
#include "types.h"


struct __attribute__((packed)) Msg {
    static Msg* read_s(uint8_t *buf, uint32_t bytes)
    {
        return (sizeof(uint16_t) <= bytes) && *(uint16_t*)buf <= bytes ? (Msg*)(buf) : NULL;
    }

    const char *get_text() const { return str_; }
    uint16_t get_size() const { return size_; }
private:
    uint16_t size_;
    char str_[];//null
};

struct __attribute__((packed)) MdMsg {
    uint16_t get_size() const { return size_; }
    uint16_t get_type() const { return type_; }
    uint64_t txn_id() const { return txn_id_; }

    const uint8_t* get_data() const { return (uint8_t*)this; }

    static MdMsg* read_s(uint8_t* buf, uint16_t bytes) {
        MdMsg* msg = reinterpret_cast<MdMsg*>(buf);
        return (sizeof(msg->size_) <= bytes) && msg->size_ <= bytes ? msg : NULL;
    }

protected:

    MdMsg(uint16_t size, uint16_t type, uint64_t txn_id)
        : size_(size), type_(type), txn_id_(txn_id) { }

    template <typename msg_type>
    static msg_type* read_s(uint8_t* buf, uint16_t bytes) {
        MdMsg* msg = read_s(buf, bytes);
        return msg && msg->type_ == msg_type::type ? reinterpret_cast<msg_type*>(msg) : NULL;
    }

    uint16_t size_;
    uint16_t type_;
    uint64_t txn_id_;
};

template <typename SockType>
inline void send(SockType& sock, const MdMsg& m) {
    sock.send(m.get_data(), m.get_size());
}

template <int t_size_>
struct fixed_str {
    fixed_str(const fixed_str&)=delete;
    fixed_str() { buf_[t_size_] = buf_[0] = 0; }
    fixed_str(const char* s) { strncpy(buf_, s, t_size_); buf_[t_size_] = 0; }
    fixed_str(const std::string& s) { strncpy(buf_, s.c_str(), t_size_); buf_[t_size_] = 0; }

    operator const char*() const { return buf_; }
    char buf_[t_size_ + 1];
};


struct __attribute__((packed)) Trade : public MdMsg {

    Trade() : MdMsg(sizeof *this, type, 0) { }

    Trade(uint64_t txn_id, uint16_t qty, double price, const char* symbol)
        : MdMsg(sizeof *this, type, txn_id), qty_(qty), price_(price), symbol_(symbol) { }

    static Trade* read_s(uint8_t* buf, uint32_t bytes) { return MdMsg::read_s<Trade>(buf, bytes); }

    uint16_t qty() const { return qty_; }
    double price() const { return price_; }
    const char* symbol() const { return symbol_; }

private:
    enum { type = 4 };

    friend struct MdMsg;

    uint16_t qty_;
    double   price_;
    fixed_str<15> symbol_; //XXX replace with book id
};

struct __attribute__((packed)) Submit : public MdMsg {

    Submit() : MdMsg(sizeof *this, type, 0) { }

    Submit(uint64_t txn_id, uint32_t id, uint16_t qty, double price, uint8_t side, const char* symbol)
        : MdMsg(sizeof *this, type, txn_id), id_(id), qty_(qty), price_(price), side_(side), symbol_(symbol)
    { unused_ = 0; }

    static Submit* read_s(uint8_t* buf, uint32_t bytes) { return MdMsg::read_s<Submit>(buf, bytes); }

    uint16_t qty() const { return qty_; }
    double price() const { return price_; }
    uint32_t id()  const { return id_; }
    uint8_t side() const { return side_; }
    const char* symbol() const { return symbol_; }

private:
    enum { type = 5 };

    friend struct MdMsg;

    uint32_t id_;
    uint16_t qty_;
    double   price_;
    uint8_t  side_;
    uint8_t  unused_;
    fixed_str<15> symbol_;
};

struct __attribute__((packed)) Cancel : public MdMsg {

    Cancel() : MdMsg(sizeof *this, type, 0) { }

    Cancel(uint64_t txn_id, uint16_t qty, double price, const char* symbol)
        : MdMsg(sizeof *this, type, txn_id), qty_(qty), price_(price), symbol_(symbol) { }

    static Cancel* read_s(uint8_t* buf, uint32_t bytes) { return MdMsg::read_s<Cancel>(buf, bytes); }

    uint16_t qty() const { return qty_; }
    double price() const { return price_; }
    const char* symbol() const { return symbol_; }

private:
    enum { type = 6 };

    friend struct MdMsg;

    uint16_t qty_;
    double   price_;
    fixed_str<15> symbol_;
};

struct __attribute__((packed)) SnapRequest : public MdMsg {

    SnapRequest() : MdMsg(sizeof *this, type, 0), levels_(0) { }

    SnapRequest(const char* symbol, uint16_t levels)
        : MdMsg(sizeof *this, type, 0), levels_(levels), symbol_(symbol)  { }

    static SnapRequest* read_s(uint8_t* buf, uint32_t bytes) { return MdMsg::read_s<SnapRequest>(buf, bytes); }

    const char* symbol() const { return symbol_; }
    const uint16_t levels() const { return levels_; }

private:
    enum { type = 7 };

    friend struct MdMsg;

    uint16_t levels_;
    fixed_str<15> symbol_; //XXX replace with book id
};


struct __attribute__((packed)) SnapResponse : public MdMsg {

    enum { MAX_LEVELS = 32 };

    struct __attribute__((packed)) Level { //bad size...
        side_t side_;
        uint32_t qty_;
        double price_;
    };

    SnapResponse() : MdMsg(sizeof *this, type, 0), n_levels_(0) { }

    SnapResponse(uint64_t txn_id, const char* symbol)
        : MdMsg(sizeof *this, type, txn_id) , n_levels_(0), symbol_(symbol) { }

    static SnapResponse* read_s(uint8_t* buf, uint32_t bytes) { return MdMsg::read_s<SnapResponse>(buf, bytes); }

    const char* symbol() const { return symbol_; }

    //if cursor is null, returns
    /*void add_level(Level*& cursor, side_t side, uint16_t qty, double price) {
        if(!cursor) {
            cursor = reinterpret_cast<Level*>(buf_);
        }

        *cursor++ = { side, qty, price };

        if( reinterpret_cast<uint8_t*>(cursor) < buf_ + sizeof(buf_) ) {
            cursor->side_ = side_t::NONE;
        }
    }*/

    bool add_level(side_t side, uint32_t qty, double price) {

        if(n_levels_ < MAX_LEVELS) {
            levels_[n_levels_++] = { side, qty, price };

            if( n_levels_ < MAX_LEVELS ) {
                levels_[n_levels_].side_ = side_t::NONE;
            }

            return true;
        }

        return false;
    }

    const Level* levels() const { return levels_; }
    void clear_levels() { n_levels_ = 0; }

private:
    enum { type = 7 };

    friend struct MdMsg;
    //uint8_t n_buys_;
    //uint8_t n_sells_;
    //uint8_t buf_[MAX_LEVELS * sizeof(Level)];
    uint16_t n_levels_;
    Level levels_[MAX_LEVELS];
    fixed_str<15> symbol_; //XXX replace with book id
};




#endif

