/**
 * network_config.c - Linux Network Configuration (Real Implementation)
 *
 * Uses Linux ioctl(SIOCGIFADDR, SIOCSIFADDR, SIOCGIFHWADDR, SIOCSIFFLAGS)
 * to query and configure network interfaces, and reads/writes /proc/sys
 * entries for TCP tuning parameters.
 *
 * On non-Linux platforms, stub implementations return -1 with a warning.
 */

#include "network_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  Platform-specific headers                                         */
/* ------------------------------------------------------------------ */
#ifdef PLATFORM_LINUX
  #include <sys/ioctl.h>
  #include <sys/socket.h>
  #include <net/if.h>
  #include <arpa/inet.h>
  #include <unistd.h>
#else
  /* macOS / BSD fallback headers (limited support) */
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <net/if.h>
  #include <arpa/inet.h>
  #ifdef __APPLE__
    #include <ifaddrs.h>
    #include <net/if_dl.h>
    #include <sys/ioctl.h>
    #include <unistd.h>
  #endif
#endif

/* ------------------------------------------------------------------ */
/*  Internal helpers (/proc/sys access - Linux only)                   */
/* ------------------------------------------------------------------ */

#ifdef PLATFORM_LINUX

#define PROC_TCP_TIMESTAMPS   "/proc/sys/net/ipv4/tcp_timestamps"
#define PROC_TCP_WIN_SCALE    "/proc/sys/net/ipv4/tcp_window_scaling"
#define PROC_TCP_SACK         "/proc/sys/net/ipv4/tcp_sack"
#define PROC_TCP_RMEM         "/proc/sys/net/ipv4/tcp_rmem"
#define PROC_TCP_WMEM         "/proc/sys/net/ipv4/tcp_wmem"

/**
 * Write a single integer value to a /proc/sys file.
 * Used for TCP tuning knobs such as tcp_timestamps.
 *
 * Returns 0 on success, -1 on failure.
 */
static int proc_write_int(const char *path, int value)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "[net] failed to open %s for writing\n", path);
        return -1;
    }
    int ret = fprintf(fp, "%d\n", value);
    fclose(fp);
    return (ret > 0) ? 0 : -1;
}

/**
 * Read a single integer value from a /proc/sys file.
 * Returns 0 on success, -1 on failure.
 */
static int proc_read_int(const char *path, int *out)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[net] failed to open %s for reading\n", path);
        return -1;
    }
    int ret = fscanf(fp, "%d", out);
    fclose(fp);
    return (ret == 1) ? 0 : -1;
}

/**
 * Read three space-separated uint32 values from a /proc/sys file.
 * Used for tcp_rmem / tcp_wmem which have the format: min default max
 * Returns 0 on success, -1 on failure.
 */
static int proc_read_u32_triple(const char *path, uint32_t out[3])
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[net] failed to open %s for reading\n", path);
        return -1;
    }
    unsigned int a, b, c;
    int n = fscanf(fp, "%u %u %u", &a, &b, &c);
    fclose(fp);
    if (n != 3) {
        return -1;
    }
    out[0] = (uint32_t)a;
    out[1] = (uint32_t)b;
    out[2] = (uint32_t)c;
    return 0;
}

/**
 * Write three space-separated uint32 values to a /proc/sys file.
 * Returns 0 on success, -1 on failure.
 */
static int proc_write_u32_triple(const char *path, const uint32_t val[3])
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "[net] failed to open %s for writing\n", path);
        return -1;
    }
    int ret = fprintf(fp, "%u %u %u\n", val[0], val[1], val[2]);
    fclose(fp);
    return (ret > 0) ? 0 : -1;
}

/**
 * Create a temporary UDP socket for ioctl queries.
 * The socket is AF_INET/SOCK_DGRAM and not connected to anything.
 * Returns the socket fd on success, -1 on failure.
 */
static int create_ioctl_socket(void)
{
    /* SOCK_DGRAM is sufficient for SIOCG* queries; no actual data is sent */
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("[net] socket() failed");
    }
    return fd;
}

#endif /* PLATFORM_LINUX */

/* ------------------------------------------------------------------ */
/*  Interface query functions                                          */
/* ------------------------------------------------------------------ */

/**
 * net_if_get_ip - Retrieve the IPv4 address of a network interface.
 *
 * Linux: opens an AF_INET datagram socket and issues
 *        ioctl(fd, SIOCGIFADDR, &ifr) to read sin_addr.
 *        The returned address is in network byte order.
 *
 * macOS: uses getifaddrs() to iterate the address list.
 *
 * Returns 0 on success, -1 on failure.
 */
int net_if_get_ip(const char *if_name, uint32_t *ip)
{
    if (!if_name || !ip) {
        return -1;
    }

#ifdef PLATFORM_LINUX
    int fd = create_ioctl_socket();
    if (fd < 0) {
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);

    /* SIOCGIFADDR: get interface address (struct sockaddr_in) */
    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
        perror("[net] ioctl(SIOCGIFADDR) failed");
        close(fd);
        return -1;
    }

    close(fd);

    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
    *ip = sin->sin_addr.s_addr; /* network byte order */
    return 0;

#elif defined(__APPLE__)
    /* macOS/BSD: use getifaddrs() to walk the interface list */
    struct ifaddrs *ifap, *ifa;
    if (getifaddrs(&ifap) != 0) {
        perror("[net] getifaddrs() failed");
        return -1;
    }

    int ret = -1;
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, if_name) != 0) continue;

        struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
        *ip = sin->sin_addr.s_addr;
        ret = 0;
        break;
    }

    freeifaddrs(ifap);
    return ret;

#else
    fprintf(stderr, "[net] net_if_get_ip: unsupported platform\n");
    return -1;
#endif
}

/**
 * net_if_get_mac - Retrieve the hardware (MAC) address of a network interface.
 *
 * Linux: uses ioctl(fd, SIOCGIFHWADDR, &ifr) which fills
 *        ifr.ifr_hwaddr.sa_data with the 6-byte MAC address.
 *
 * macOS: uses getifaddrs() looking for AF_LINK (link-layer) addresses.
 *
 * Returns 0 on success, -1 on failure.
 */
int net_if_get_mac(const char *if_name, uint8_t mac[6])
{
    if (!if_name || !mac) {
        return -1;
    }

#ifdef PLATFORM_LINUX
    int fd = create_ioctl_socket();
    if (fd < 0) {
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);

    /* SIOCGIFHWADDR: get hardware address */
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("[net] ioctl(SIOCGIFHWADDR) failed");
        close(fd);
        return -1;
    }

    close(fd);
    memcpy(mac, (uint8_t *)ifr.ifr_hwaddr.sa_data, 6);
    return 0;

#elif defined(__APPLE__)
    /* macOS/BSD: iterate AF_LINK entries for link-layer (MAC) addresses */
    struct ifaddrs *ifap, *ifa;
    if (getifaddrs(&ifap) != 0) {
        perror("[net] getifaddrs() failed");
        return -1;
    }

    int ret = -1;
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_LINK) continue;
        if (strcmp(ifa->ifa_name, if_name) != 0) continue;

        struct sockaddr_dl *sdl = (struct sockaddr_dl *)ifa->ifa_addr;
        if (sdl->sdl_alen == 6) {
            memcpy(mac, LLADDR(sdl), 6);
            ret = 0;
            break;
        }
    }

    freeifaddrs(ifap);
    return ret;

#else
    fprintf(stderr, "[net] net_if_get_mac: unsupported platform\n");
    return -1;
#endif
}

/* ------------------------------------------------------------------ */
/*  Interface configuration functions (require root on Linux)          */
/* ------------------------------------------------------------------ */

/**
 * net_if_set_ip - Set the IPv4 address of a network interface.
 *
 * Linux: uses ioctl(fd, SIOCSIFADDR, &ifr) with the target address
 *        packed into a sockaddr_in. Requires CAP_NET_ADMIN.
 *
 * Returns 0 on success, -1 on failure.
 */
int net_if_set_ip(const char *if_name, uint32_t ip)
{
    if (!if_name) {
        return -1;
    }

#ifdef PLATFORM_LINUX
    int fd = create_ioctl_socket();
    if (fd < 0) {
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);

    /* Pack the IP into a sockaddr_in for the ioctl */
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = ip; /* network byte order */

    /* SIOCSIFADDR: set interface address */
    if (ioctl(fd, SIOCSIFADDR, &ifr) < 0) {
        perror("[net] ioctl(SIOCSIFADDR) failed");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;

#else
    (void)ip;
    fprintf(stderr, "[net] net_if_set_ip: not supported on this platform (requires Linux)\n");
    return -1;
#endif
}

/**
 * net_if_set_netmask - Set the subnet mask of a network interface.
 *
 * Linux: uses ioctl(fd, SIOCSIFNETMASK, &ifr). Requires CAP_NET_ADMIN.
 *
 * Returns 0 on success, -1 on failure.
 */
int net_if_set_netmask(const char *if_name, uint32_t mask)
{
    if (!if_name) {
        return -1;
    }

#ifdef PLATFORM_LINUX
    int fd = create_ioctl_socket();
    if (fd < 0) {
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);

    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_netmask;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = mask;

    /* SIOCSIFNETMASK: set interface netmask */
    if (ioctl(fd, SIOCSIFNETMASK, &ifr) < 0) {
        perror("[net] ioctl(SIOCSIFNETMASK) failed");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;

#else
    (void)mask;
    fprintf(stderr, "[net] net_if_set_netmask: not supported on this platform (requires Linux)\n");
    return -1;
#endif
}

/**
 * net_if_set_up - Bring a network interface up (set IFF_UP flag).
 *
 * Linux: reads current flags via ioctl(SIOCGIFFLAGS), sets the IFF_UP
 *        bit, then applies with ioctl(SIOCSIFFLAGS). Requires CAP_NET_ADMIN.
 *
 * Returns 0 on success, -1 on failure.
 */
int net_if_set_up(const char *if_name)
{
    if (!if_name) {
        return -1;
    }

#ifdef PLATFORM_LINUX
    int fd = create_ioctl_socket();
    if (fd < 0) {
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);

    /* Read current flags first */
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
        perror("[net] ioctl(SIOCGIFFLAGS) failed");
        close(fd);
        return -1;
    }

    /* Set IFF_UP to activate the interface */
    ifr.ifr_flags |= IFF_UP;

    /* SIOCSIFFLAGS: write flags back to the kernel */
    if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
        perror("[net] ioctl(SIOCSIFFLAGS) failed");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;

#else
    fprintf(stderr, "[net] net_if_set_up: not supported on this platform (requires Linux)\n");
    return -1;
#endif
}

/**
 * net_if_set_down - Bring a network interface down (clear IFF_UP flag).
 *
 * Linux: reads current flags via ioctl(SIOCGIFFLAGS), clears the IFF_UP
 *        bit, then applies with ioctl(SIOCSIFFLAGS). Requires CAP_NET_ADMIN.
 *
 * Returns 0 on success, -1 on failure.
 */
int net_if_set_down(const char *if_name)
{
    if (!if_name) {
        return -1;
    }

#ifdef PLATFORM_LINUX
    int fd = create_ioctl_socket();
    if (fd < 0) {
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);

    /* Read current flags */
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
        perror("[net] ioctl(SIOCGIFFLAGS) failed");
        close(fd);
        return -1;
    }

    /* Clear IFF_UP to deactivate the interface */
    ifr.ifr_flags &= ~IFF_UP;

    /* Write flags back */
    if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
        perror("[net] ioctl(SIOCSIFFLAGS) failed");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;

#else
    fprintf(stderr, "[net] net_if_set_down: not supported on this platform (requires Linux)\n");
    return -1;
#endif
}

/**
 * net_if_get_config - Aggregate query: fill a net_if_config_t with the
 *                     interface name, IP address, and MAC address.
 *
 * Combines net_if_get_ip() and net_if_get_mac() into a single call.
 * Returns 0 on success (both IP and MAC retrieved), -1 if either fails.
 */
int net_if_get_config(const char *if_name, net_if_config_t *config)
{
    if (!if_name || !config) {
        return -1;
    }

    memset(config, 0, sizeof(net_if_config_t));
    strncpy(config->if_name, if_name, sizeof(config->if_name) - 1);

    int ip_ret  = net_if_get_ip(if_name, &config->ip_addr);
    int mac_ret = net_if_get_mac(if_name, config->mac_addr);

    if (ip_ret == 0 && mac_ret == 0) {
        config->configured = true;
        return 0;
    }

    /* Partial success: mark as not fully configured */
    config->configured = false;
    return -1;
}

/* ------------------------------------------------------------------ */
/*  TCP/IP tuning (via /proc/sys)                                     */
/* ------------------------------------------------------------------ */

/**
 * net_tcp_tuning_apply - Write TCP tuning parameters to /proc/sys.
 *
 * On Linux, the kernel exposes TCP tuning knobs as writable files
 * under /proc/sys/net/ipv4/. This function writes boolean flags
 * (tcp_timestamps, tcp_window_scaling, tcp_sack) and memory triplets
 * (tcp_rmem, tcp_wmem) to the corresponding files.
 *
 * Each boolean file accepts "0" or "1". The rmem/wmem files accept
 * three space-separated integers: "min default max".
 *
 * Returns 0 if all writes succeed, -1 on first failure.
 */
int net_tcp_tuning_apply(const tcp_tuning_config_t *tuning)
{
    if (!tuning) {
        return -1;
    }

#ifdef PLATFORM_LINUX
    int ret = 0;

    ret |= proc_write_int(PROC_TCP_TIMESTAMPS, tuning->tcp_timestamps ? 1 : 0);
    ret |= proc_write_int(PROC_TCP_WIN_SCALE,  tuning->tcp_window_scaling ? 1 : 0);
    ret |= proc_write_int(PROC_TCP_SACK,       tuning->tcp_sack ? 1 : 0);
    ret |= proc_write_u32_triple(PROC_TCP_RMEM, tuning->tcp_rmem);
    ret |= proc_write_u32_triple(PROC_TCP_WMEM, tuning->tcp_wmem);

    return (ret == 0) ? 0 : -1;

#else
    fprintf(stderr, "[net] net_tcp_tuning_apply: /proc/sys not available on this platform\n");
    return -1;
#endif
}

/**
 * net_tcp_tuning_read_current - Read current TCP tuning values from /proc/sys.
 *
 * Reads the same kernel files that net_tcp_tuning_apply() writes,
 * populating a tcp_tuning_config_t with the running values.
 *
 * Returns 0 on success, -1 on failure.
 */
int net_tcp_tuning_read_current(tcp_tuning_config_t *tuning)
{
    if (!tuning) {
        return -1;
    }

#ifdef PLATFORM_LINUX
    memset(tuning, 0, sizeof(tcp_tuning_config_t));

    int ts = 0, ws = 0, sack = 0;
    int ret = 0;

    ret |= proc_read_int(PROC_TCP_TIMESTAMPS, &ts);
    ret |= proc_read_int(PROC_TCP_WIN_SCALE,  &ws);
    ret |= proc_read_int(PROC_TCP_SACK,        &sack);

    tuning->tcp_timestamps     = (ts != 0);
    tuning->tcp_window_scaling = (ws != 0);
    tuning->tcp_sack           = (sack != 0);

    ret |= proc_read_u32_triple(PROC_TCP_RMEM, tuning->tcp_rmem);
    ret |= proc_read_u32_triple(PROC_TCP_WMEM, tuning->tcp_wmem);

    return (ret == 0) ? 0 : -1;

#else
    fprintf(stderr, "[net] net_tcp_tuning_read_current: /proc/sys not available on this platform\n");
    return -1;
#endif
}

/* ------------------------------------------------------------------ */
/*  Utility functions (cross-platform)                                 */
/* ------------------------------------------------------------------ */

/**
 * net_ip_from_string - Convert a dotted-decimal IPv4 string to a
 *                      uint32_t in network byte order.
 *
 * Uses inet_pton(AF_INET, ...) which is POSIX-standard and available
 * on both Linux and macOS.
 *
 * Returns the IP in network byte order, or 0 (INADDR_ANY) on failure.
 */
uint32_t net_ip_from_string(const char *str)
{
    if (!str) {
        return 0;
    }

    struct in_addr addr;
    /* inet_pton returns 1 on success, 0 for invalid format, -1 on error */
    if (inet_pton(AF_INET, str, &addr) != 1) {
        fprintf(stderr, "[net] net_ip_from_string: invalid address '%s'\n", str);
        return 0;
    }
    return addr.s_addr; /* already in network byte order */
}

/**
 * net_ip_to_string - Convert a uint32_t (network byte order) to a
 *                    dotted-decimal string.
 *
 * Uses inet_ntop(AF_INET, ...) which is POSIX-standard.
 * Writes the result into buf (up to buf_size bytes).
 */
void net_ip_to_string(uint32_t ip, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }

    struct in_addr addr;
    addr.s_addr = ip;

    if (inet_ntop(AF_INET, &addr, buf, (socklen_t)buf_size) == NULL) {
        strncpy(buf, "0.0.0.0", buf_size);
        buf[buf_size - 1] = '\0';
    }
}

/**
 * net_ip_is_valid - Basic sanity check on an IPv4 address.
 *
 * Rejects:
 *   - 0.0.0.0 (INADDR_ANY / unspecified)
 *   - 255.255.255.255 (INADDR_BROADCAST)
 *   - Addresses in 0.0.0.0/8 (current network)
 *   - Addresses in 240.0.0.0/4 (reserved / experimental)
 *
 * Returns true if the address passes all checks.
 */
bool net_ip_is_valid(uint32_t ip)
{
    /* Convert to host byte order for bitwise checks */
    uint32_t host_ip = ntohl(ip);

    /* Reject 0.0.0.0 */
    if (host_ip == 0) {
        return false;
    }

    /* Reject 255.255.255.255 (broadcast) */
    if (host_ip == 0xFFFFFFFF) {
        return false;
    }

    /* Reject 0.0.0.0/8 (this network) */
    if ((host_ip & 0xFF000000) == 0) {
        return false;
    }

    /* Reject 240.0.0.0/4 (reserved for future use / experimental) */
    if ((host_ip & 0xF0000000) == 0xF0000000) {
        return false;
    }

    return true;
}
