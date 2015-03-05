#include "st_socket.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
//#include <sys/epoll.h>
#include <sys/event.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <unordered_map>

/****** SOCKET ADDR *******/

void SocketAddr::init(const char * addr_str, uint16_t port) {
    addr_.sin_addr.s_addr = INADDR_ANY;
    addr_.sin_port        = htons(port_ = port);
    addr_.sin_family      = AF_INET;

    if( addr_str && inet_aton(addr_str, &addr_.sin_addr) == 0 )  
        std::cerr << "ERROR: BAD ADDRESS" << std::endl;
}


SocketAddr::SocketAddr() {
    init();
}


SocketAddr::SocketAddr(const std::string& addr) {
    
    int pos;
    if( (pos = addr.find(':')) != std::string::npos  ) { 
        std::string addr_str = addr.substr(0, pos);
        std::string port_str = addr.substr(pos + 1);

        init(addr_str.c_str(), atoi(port_str.c_str()));
    } else {
        init(addr.c_str());
    }
}


/****** EPOLL *****
struct Epoll
{
	typedef void (* event_cb_t)(Socket * sock, int error);

	Epoll() : epoll_fd_(epoll_create1(0)), terminated_(false) { }

	bool add_socket(int fd, Socket * sock, event_cb_t sock_cb);
	bool del_socket(int fd);

	void loop();
	void stop();

private:

	struct EventData
	{
		EventData() : sock_(0), cb_(0) { } 
		EventData(Socket * sock, event_cb_t cb) : sock_(sock), cb_(cb) { } 

		Socket * sock_;
		event_cb_t cb_;
	};

	int epoll_fd_;
	bool terminated_;

	std::map<int, EventData> sockets_; 
};

static Epoll epoll_s;


bool Epoll::add_socket(int fd, Socket * sock, event_cb_t sock_cb) {

	sockets_[fd] = EventData(sock, sock_cb);

	epoll_event event;
	event.data.ptr = &sockets_[fd];
	event.events   = EPOLLIN | EPOLLET | EPOLLRDNORM;

	return epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) != -1;
}


bool Epoll::del_socket(int fd) {
    return epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, 0) != -1;
}


void Epoll::loop() {
    epoll_event event;

	while( !terminated_ && sockets_.size() ) {
        int nfds = epoll_wait(epoll_fd_, &event, 1, -1);

        if( nfds > 0 ) {
            EventData * event_data = (EventData *)event.data.ptr;
            event_data->cb_(event_data->sock_ , event.events & EPOLLERR ? 1234 : 0);
        } else if( nfds == -1 && errno != EINTR ) {
            std::cout << "ERROR: " << errno << std::endl;
            break;
        }
	}
}


void Epoll::stop() {
	terminated_ = true;
}*/

struct Kqueue
{
	typedef void (* event_cb_t)(Socket * sock, int error);

	Kqueue() : kqueue_fd_(kqueue()), terminated_(false) 
        { }

	bool add_socket(int fd, Socket * sock, event_cb_t sock_cb);
	bool del_socket(int fd);

	void loop();
	void stop();

private:

	struct EventData
	{
		EventData() : sock_(0), cb_(0) { } 
		EventData(Socket * sock, event_cb_t cb) : sock_(sock), cb_(cb) { } 

		Socket * sock_;
		event_cb_t cb_;
	};

	int kqueue_fd_;
	bool terminated_;
	std::unordered_map<int, EventData> sockets_; 
};

static Kqueue kqueue_s;


bool Kqueue::add_socket(int fd, Socket * sock, event_cb_t sock_cb) {
	sockets_[fd] = EventData(sock, sock_cb);

    struct kevent evt;
    EV_SET(&evt, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*)&sockets_[fd]); 

    return kevent(kqueue_fd_, &evt, 1, NULL, 0, NULL) != -1;
}


bool Kqueue::del_socket(int fd) {
    struct kevent evt;
    EV_SET(&evt, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL); 

    return kevent(kqueue_fd_, &evt, 1, NULL, 0, NULL) != -1;
}


void Kqueue::loop() {
    struct kevent evts[256];

	while( !terminated_ && sockets_.size() ) {

        int nevts = kevent(kqueue_fd_, NULL, 0, evts, sizeof(evts) / sizeof *evts, NULL); 

        if( nevts > 0 ) {
            for(int i = 0; i < nevts; ++i) {

                EventData * event_data = (EventData *)evts[i].udata;

                if( evts[i].flags & EV_EOF )  {
                    event_data->cb_(event_data->sock_, 1234);

                    if(!del_socket(evts[i].ident))
                        std::cerr << "failed to delete socket" << std::endl;

                    sockets_.erase(evts[i].ident);
                } else {
                    event_data->cb_(event_data->sock_, (evts[i].flags & EV_ERROR) ? evts[i].data : 0);
                }
            }
        } else if( nevts < 0 && errno != EINTR ) {
            std::cerr << "ERROR: " << errno << std::endl;
            break;
        }
	}
    std::cerr << sockets_.size() << std::endl;
}


void Kqueue::stop() {
	terminated_ = true;
}


/****** SOCKET *****/

Socket::Socket(const SocketAddr& local_addr, const SocketAddr& remote_addr) 
	: fd_(0), recv_buf_size_(32 * 1024), recv_buf_(new uint8_t[recv_buf_size_]), nbytes_(0), 
      user_data_(NULL), error_cb_(NULL), local_addr_(local_addr), remote_addr_(remote_addr) { }

Socket::~Socket() {

    if( fd_ != -1 ) { 
        close();
    }

    delete[] recv_buf_;
}

void Socket::close() {
    if( kqueue_s.del_socket(fd_) )
        ::close(fd_);
    //else
    //    std::cout << "failed to delete" << std::endl;
}

TcpSocket::TcpSocket(const SocketAddr& local_addr, const SocketAddr& group_addr)
    : Socket(local_addr, group_addr), accept_cb_(NULL) { }

TcpSocket::TcpSocket(int fd) 
	: Socket(SocketAddr(), SocketAddr()) { fd_ = fd; set_reuse(); kqueue_s.add_socket(fd_, this, Socket::recv_cb); }

int TcpSocket::send(const uint8_t * data, uint16_t size) {
    //std::cerr << "====== sending <" << size << "> bytes ========" << std::endl;
    return ::send(fd_, data, size, 0);
}


bool TcpSocket::connect() {

    if( init() ) {

        //set_reuse();

        if( ::connect(fd_, (struct sockaddr *)&remote_addr_.get_sockaddr(), sizeof(struct sockaddr)) < 0)
            std::cerr << "ERROR: FAILED TO CONNECT" << std::endl;
        else
            return kqueue_s.add_socket(fd_, this, Socket::recv_cb);
    }

	return false;
}

bool TcpSocket::listen() {

    if( init() ) {

        //set_reuse();

        if( ::listen(fd_, 32) < 0)
            std::cerr << "ERROR: FAILED TO LISTEN" << std::endl;
        else
            return kqueue_s.add_socket(fd_, this, TcpSocket::listen_cb);
    }

	return false;
}


bool TcpSocket::init() {

    fd_ = socket(PF_INET, SOCK_STREAM, 0); 

    if( fd_ < 0 ) {
        std::cerr << "ERROR: COULDNT ALLOCATE SOCKET" << std::endl;
        return false;
    } 

    if( bind(fd_, (struct sockaddr *)&local_addr_.get_sockaddr(), sizeof(struct sockaddr)) < 0 ) {
        std::cerr << "ERROR: FAILED TO BIND" << std::endl;
        return false;
    }

    set_reuse();
    return true;
}


void Socket::recv_cb(Socket * socket, int error) {
	std::cerr << "[+] recv_cb()" << std::endl;
    while( !error && socket->fd_ != -1 ) {

        int nbytes = recv(socket->fd_, socket->recv_buf_ + socket->nbytes_, socket->recv_buf_size_ - socket->nbytes_, MSG_DONTWAIT); 

        if( nbytes > 0 )
        {
            uint16_t user_bytes = socket->nbytes_ + nbytes;

            if( socket->user_cb_ )
                socket->user_cb_(socket->user_data_, socket->recv_buf_, user_bytes);

            if( user_bytes )
                memmove(socket->recv_buf_, socket->recv_buf_ + nbytes + socket->nbytes_ - user_bytes, user_bytes);

            socket->nbytes_ = user_bytes;

        } else if( errno == EAGAIN || errno == EWOULDBLOCK || nbytes == 0 ) {

            if( nbytes == 0 )
                error = 1234;

            break;

        } else {
          error = errno;
        }
    }

    if(error && socket->error_cb_)
        socket->error_cb_(socket->user_data_, error == 1234 ? 0 : error); 

	std::cerr << "[-] recv_cb()" << std::endl;
}


void TcpSocket::listen_cb(Socket * sock, int error) {
	std::cerr << "[+] listen_cb()" << std::endl;

    TcpSocket * accept_sock = (TcpSocket*)sock;

    if( !error && accept_sock->fd_ != -1 ) {
        struct sockaddr_in client_addr;
        socklen_t sock_size = sizeof(struct sockaddr_in);

        int new_fd = accept(accept_sock->fd_, (struct sockaddr *)&client_addr, &sock_size);

        if( new_fd < 0 )
            std::cerr << "ERROR: FAILED TO ACCEPT" << std::endl;//accept cb with error
        else if(!accept_sock->accept_cb_)
            ::close(new_fd);
        else
            accept_sock->accept_cb_(accept_sock->user_data_, new TcpSocket(new_fd));
    }

	std::cerr << "[-] listen_cb()" << std::endl;
}


void Socket::start_recv_loop_s() {
	kqueue_s.loop();
}


void Socket::stop_recv_loop_s() {
	kqueue_s.stop();
}

/**********************/

UdpSocket::UdpSocket(const SocketAddr& local_addr, const SocketAddr& group_addr)
    : Socket(local_addr, group_addr) { }

int UdpSocket::send(const uint8_t* data, uint16_t size) {
    return ::sendto(fd_, data, size, 0, (struct sockaddr *)&remote_addr_.get_sockaddr(), sizeof(local_addr_.get_sockaddr()));
}

void Socket::set_reuse() {
    int flag = 1;
    if( setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0 )   
        std::cerr << "Failed to set reuse addr" << std::endl;
}

bool UdpSocket::init_send() {

    if( (fd_ = socket(PF_INET, SOCK_DGRAM, 0)) < 0 )  {
        std::cerr << "Failed to get socket" << std::endl;
    } else {
        set_reuse();

        const struct sockaddr_in& s = local_addr_.get_sockaddr();

        if( setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_IF, &s.sin_addr.s_addr, sizeof(s.sin_addr.s_addr) ) < 0 )   
            std::cerr << "Failed to set send iface for multicast" << std::endl;
        else if(setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl_, sizeof(ttl_)) < 0)
            std::cerr << "Failed to set TTL" << std::endl;
        else
            return true;
    }

    return false;
}

bool UdpSocket::init_recv() {

    if( (fd_ = socket(PF_INET, SOCK_DGRAM, 0)) < 0 )  {
        std::cerr << "Failed to get socket" << std::endl;
    } else {
        set_reuse();

        const struct sockaddr_in& s = remote_addr_.get_sockaddr();

        if( bind(fd_, (const struct sockaddr*)&s, sizeof(s)) < 0 ) {
            std::cerr << "ERROR: FAILED TO BIND" << std::endl;
        } else {
            struct ip_mreq mreq;
            bzero(&mreq, sizeof(mreq));

            mreq.imr_multiaddr.s_addr = s.sin_addr.s_addr;
            mreq.imr_interface.s_addr = local_addr_.get_sockaddr().sin_addr.s_addr;

            if( setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == 0) {
                return kqueue_s.add_socket(fd_, this, Socket::recv_cb);
            }
        }
    }

    return false;
}
/**********************/

/*
void test_error(void * user_data, int error) {
    std::cout << "++++++++++++++ ERROR +++++++++++++" << std::endl;
}

void test_recv(void * user_data, uint8_t* buf, uint16_t& bytes) {
    std::cout << "bytes <" << bytes << "> : " 
        << std::string((const char*)buf, bytes) << std::endl;
    bytes = 0;
}

void test_accept(void * user_data, Socket* s) {
    s->set_recv_cb(NULL, test_recv);
    s->set_error_cb(test_error);
}

int main() {
    UdpSocket s(SocketAddr(), SocketAddr("228.0.0.5:8005"));
    if( s.init_recv() ) {
        std::cerr << "=== READY TO START RECEIVING ===" << std::endl;

        s.set_recv_cb(nullptr, test_recv); 

        Socket::start_recv_loop_s();
    }
    s.init_send();
    char buf[128];
    int i = 0;
    for(;;) {
        std::cout << "." << std::endl;
        int bytes = sprintf(buf, "== %d == ", i++);
        if( !s.send((uint8_t*)buf, bytes + 1) )
            std::cerr << "EEEEEEEER" << std::endl;
            
        usleep(1000 * 1000);
    }
    TcpSocket s(SocketAddr("192.168.1.104:5555"), SocketAddr());
    s.set_accept_cb(NULL, test_accept);
    s.listen();
    Socket::start_recv_loop_s();
}
*/
