#include <iostream>
#include <string>
#include <cstring>
#include <nng/nng.h>

void fatal(const char *func, int rv)
{
    std::cerr << func << ": " << nng_strerror((nng_err)rv) << std::endl;
    exit(1);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: ./nng_test <listen|dial> <url>" << std::endl;
        return 1;
    }

    const char *mode = argv[1];
    const char *url = argv[2];
    nng_socket sock;
    int rv;

    // Initialize NNG library
    if ((rv = nng_init(NULL)) != 0)
        fatal("nng_init", rv);

    // Open a Pair socket
    if ((rv = nng_pair0_open(&sock)) != 0)
        fatal("nng_pair0_open", rv);

    if (std::string(mode) == "listen")
    {
        nng_listener listener;
        if ((rv = nng_listener_create(&listener, sock, url)) != 0)
            fatal("nng_listener_create", rv);
        if ((rv = nng_listener_start(listener, 0)) != 0)
            fatal("nng_listener_start", rv);
    }
    else
    {
        nng_dialer dialer;
        if ((rv = nng_dialer_create(&dialer, sock, url)) != 0)
            fatal("nng_dialer_create", rv);
        if ((rv = nng_dialer_start(dialer, 0)) != 0)
            fatal("nng_dialer_start", rv);
    }

    std::cout << "Socket ready in " << mode << " mode at " << url << std::endl;

    while (true)
    {
        // Send a message
        nng_msg *msg;
        const char *text = "Hello from the other Pi!";
        if ((rv = nng_msg_alloc(&msg, 0)) != 0)
            fatal("nng_msg_alloc", rv);
        if ((rv = nng_msg_append(msg, text, strlen(text) + 1)) != 0)
        {
            nng_msg_free(msg);
            fatal("nng_msg_append", rv);
        }
        if ((rv = nng_sendmsg(sock, msg, 0)) == 0)
        {
            std::cout << "Sent message." << std::endl;
        }
        else
        {
            nng_msg_free(msg);
            fatal("nng_sendmsg", rv);
        }

        // Receive a message
        nng_msg *recv_msg;
        if ((rv = nng_recvmsg(sock, &recv_msg, 0)) == 0)
        {
            std::cout << "Received: " << (char *)nng_msg_body(recv_msg) << std::endl;
            nng_msg_free(recv_msg);
        }

        nng_msleep(2000); // Wait 2 seconds
    }

    return 0;
}