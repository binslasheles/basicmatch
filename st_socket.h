#ifndef _ST_SOCKET_H_
#define _ST_SOCKET_H_

#include <netinet/in.h>
#include <cstdlib>
#include <iostream>


class SocketAddr
{
public:
    SocketAddr();
    SocketAddr(const std::string& addr);

    const struct sockaddr_in& get_sockaddr() { return addr_; };

private:
    void init(const char * addr_str = 0, uint16_t port = 0);

    struct sockaddr_in addr_;
    uint16_t port_;
};


class Socket
{
public:
	typedef void (* recv_cb_t)(void * user_data, uint8_t * buf, uint16_t& bytes); 
    typedef void (* accept_cb_t)(void * user_data, Socket * new_sock);
    typedef void (* error_cb_t)(void * user_data, int error);

    virtual ~Socket();

    void set_recv_cb(void * user_data, recv_cb_t user_cb) 
        { user_cb_ = user_cb; user_data_ = user_data; }

    void set_error_cb(error_cb_t error_cb) 
        { error_cb_ = error_cb; }

	static void start_recv_loop_s();
	static void stop_recv_loop_s();

protected:
    Socket(const SocketAddr& local_addr, const SocketAddr& remote_addr);

    void set_reuse();

    Socket();

	static void recv_cb(Socket * socket, int error);

	int         fd_;
    uint16_t    recv_buf_size_;
    uint8_t *   recv_buf_;  
    uint16_t    nbytes_;

	void *      user_data_;
	recv_cb_t  user_cb_;
	error_cb_t  error_cb_;

    SocketAddr  local_addr_;
    SocketAddr  remote_addr_;
};

class TcpSocket : public Socket {
public:
    TcpSocket(const SocketAddr& local_addr, const SocketAddr& remote_addr);

    void set_accept_cb(void * user_data, accept_cb_t accept_cb) 
        { accept_cb_ = accept_cb; user_data_ = user_data; }

	int send(const uint8_t * data, uint16_t size);

	bool connect();
	bool listen();

private:
    TcpSocket(int fd);

    static void listen_cb(Socket * socket, int error);

	bool init();

	accept_cb_t  accept_cb_;
};

class UdpSocket : public Socket {
public:
    UdpSocket(const SocketAddr& iface, const SocketAddr& addr);

    bool init_send();
    bool init_recv();

    int send(const uint8_t* data, uint16_t size);
private:
    int ttl_ = 32;
};

#endif
