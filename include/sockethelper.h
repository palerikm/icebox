

#pragma once

int
sendall(int   s,
        char* buf,
        int*  len);

/* int */
/* createUdpSocket(char              host[], */
/*                char              port[], */
/*                char              outprintName[], */
/*                int               ai_flags, */
/*                struct addrinfo*  servinfo, */
/*                struct addrinfo** p); */

int
createTcpSocket(char* dst,
                char* port);

int
getSockaddrFromFqdn(struct sockaddr* addr_out,
                    const char*      fqdn);
