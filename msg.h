#pragma once
#include <string.h>

/*
struct __attribute__((packed)) Msg
{
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

template <int t_size_>
struct fixed_str
{
    fixed_str(const fixed_str&)=delete;
    fixed_str() { buf_[t_size_] = buf_[0] = 0; }
    fixed_str(const char* s) { strncpy(buf_, s, t_size_); buf_[t_size_] = 0; }
    fixed_str(const std::string& s) { strncpy(buf_, s.c_str(), t_size_); buf_[t_size_] = 0; }

    operator const char*() const { return buf_; }
    char buf_[t_size_ + 1];
};

*/
