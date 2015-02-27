#include <string>
#include <iostream>
#include <pthread.h>
#include "st_socket.h"

void *exec_s(void* arg) {
    char buf[256];
    TcpSocket& s = *(TcpSocket*)arg; 

    std::string line;
    while(true) {
        std::cout << "$: ";
        std::getline(std::cin, line);

        if(line.size() == 0)
            break;

        uint16_t slen = line.size();
        uint16_t msize =  slen + sizeof(uint16_t) + 1;

        memcpy(buf, &msize, sizeof(msize));   
        memcpy(buf + sizeof(slen), line.c_str(), slen + 1);   

        s.send((uint8_t*)buf, msize);
    }

    return NULL;
}

//main function takes command line argument as file name to try to read 
int main(int argc, char **argv)
{
    std::string addr = "192.168.1.104:6677";

    if (argc > 1)
        addr = std::string(argv[1]);

    TcpSocket s(SocketAddr(), addr);

    pthread_t pid;
    pthread_create(&pid, NULL, exec_s, (void*)&s); 

    if(s.connect())
        Socket::start_recv_loop_s();

    return 0;
}
