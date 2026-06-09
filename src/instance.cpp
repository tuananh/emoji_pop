#include "instance.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace {

int g_server_fd = -1;
std::string g_socket_path;

std::string SocketPath() {
    if (const char* runtime = std::getenv("XDG_RUNTIME_DIR"))
        return std::string(runtime) + "/emoji_pop.sock";
    return "/tmp/emoji_pop-" + std::to_string(getuid()) + ".sock";
}

bool SendShow() {
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return false;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, g_socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return false;
    }

    static const char kShow[] = "SHOW";
    (void)!write(fd, kShow, sizeof(kShow) - 1);
    close(fd);
    return true;
}

bool BindServer() {
    g_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_server_fd < 0)
        return false;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, g_socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(g_server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        return false;

    if (listen(g_server_fd, 4) != 0) {
        close(g_server_fd);
        g_server_fd = -1;
        return false;
    }

    const int flags = fcntl(g_server_fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(g_server_fd, F_SETFL, flags | O_NONBLOCK);
    return true;
}

} // namespace

bool AcquireInstance() {
    g_socket_path = SocketPath();

    if (BindServer())
        return true;

    if (errno == EADDRINUSE) {
        if (SendShow())
            return false;
        if (unlink(g_socket_path.c_str()) != 0)
            return false;
        return BindServer();
    }

    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }

    std::fprintf(stderr, "instance: bind failed: %s\n", std::strerror(errno));
    return false;
}

void PollInstanceServer(const std::function<void()>& on_show) {
    if (g_server_fd < 0)
        return;

    const int client = accept(g_server_fd, nullptr, nullptr);
    if (client < 0)
        return;

    char buf[16] = {};
    (void)!read(client, buf, sizeof(buf) - 1);
    close(client);

    if (std::strncmp(buf, "SHOW", 4) == 0 && on_show)
        on_show();
}

void ReleaseInstance() {
    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }
    if (!g_socket_path.empty())
        unlink(g_socket_path.c_str());
}
