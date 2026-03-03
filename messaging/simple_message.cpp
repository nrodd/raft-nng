#include <iostream>
#include <string>
#include <cstring>
#include <nng/nng.h>

void fatal(const char *func, int rv)
{
    std::cerr << func << ": " << nng_strerror((nng_err)rv) << std::endl;
    exit(1);
}

int main()
{
    const char *url = "tcp://127.0.0.1:5002";
    nng_socket sock;
    int rv;

    // Initialize NNG library
    if ((rv = nng_init(NULL)) != 0)
        fatal("nng_init", rv);

    // Open a Pair socket
    if ((rv = nng_bus0_open(&sock)) != 0)
    {
        std::cerr << "bus open failed: " << nng_strerror((nng_err)rv) << "\n";
        return 1;
    }

    nng_listener listener;
    if ((rv = nng_listener_create(&listener, sock, url)) != 0)
        fatal("nng_listener_create", rv);
    if ((rv = nng_listener_start(listener, 0)) != 0)
        fatal("nng_listener_start", rv);

    std::cout << "Socket ready at " << url << std::endl;

    // while (true)
    // {
    //     // Send a message
    //     nng_msg *msg;
    //     const char *text = "Hello from the other Pi!";
    //     if ((rv = nng_msg_alloc(&msg, 0)) != 0)
    //         fatal("nng_msg_alloc", rv);
    //     if ((rv = nng_msg_append(msg, text, strlen(text) + 1)) != 0)
    //     {
    //         nng_msg_free(msg);
    //         fatal("nng_msg_append", rv);
    //     }
    //     if ((rv = nng_sendmsg(sock, msg, 0)) == 0)
    //     {
    //         std::cout << "Sent message." << std::endl;
    //     }
    //     else
    //     {
    //         nng_msg_free(msg);
    //         fatal("nng_sendmsg", rv);
    //     }

    //     // Receive a message
    //     nng_msg *recv_msg;
    //     if ((rv = nng_recvmsg(sock, &recv_msg, 0)) == 0)
    //     {
    //         std::cout << "Received: " << (char *)nng_msg_body(recv_msg) << std::endl;
    //         nng_msg_free(recv_msg);
    //     }

    //     nng_msleep(2000); // Wait 2 seconds
    // }

    return 0;
}