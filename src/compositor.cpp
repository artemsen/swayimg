// SPDX-License-Identifier: MIT
// Integration with Wayland compositors (Sway and Hyprland only).
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "compositor.hpp"

#include "log.hpp"

#include <json/json.hpp>

#include <sys/socket.h>
#include <sys/un.h>

#include <cstring>
#include <format>

using json = nlohmann::json;

/** UNIX socket. */
class UnixSocket {
public:
    /**
     * Destructor: close UNIX socket.
     */
    ~UnixSocket()
    {
        if (fd != -1) {
            close(fd);
        }
    }

    /**
     * Check if socket opened.
     * @return true if socket opened.
     */
    bool valid() const { return fd != -1; }

    /**
     * Connect to UNIX socket.
     * @param path path to the socket file
     * @return true if connection established
     */
    bool connect(const char* path)
    {
        struct sockaddr_un sa = {};

        const size_t len = path ? strlen(path) + 1 : 0;
        if (len == 0 || len > sizeof(sa.sun_path)) {
            Log::error("Invalid IPC socket path {}", path);
            return false;
        }

        sa.sun_family = AF_UNIX;
        memcpy(sa.sun_path, path, len);

        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd == -1) {
            Log::error(errno, "Failed to open IPC socket {}", path);
            return false;
        }

        if (::connect(fd, reinterpret_cast<struct sockaddr*>(&sa),
                      sizeof(sa)) == -1) {
            Log::error(errno, "Failed to connect IPC socket {}", path);
            close(fd);
            fd = -1;
            return false;
        }

        return true;
    }

    /**
     * Read data from socket connection.
     * @param buf buffer for destination data
     * @param size max number of bytes to read
     * @return number of bytes read
     */
    size_t read(void* buffer, size_t size) const
    {
        assert(fd != -1);

        size_t read = 0;

        while (read < size) {
            const ssize_t rc =
                recv(fd, ((uint8_t*)buffer) + read, size - read, 0);
            if (rc == 0) {
                break;
            }
            if (rc == -1) {
                Log::error(errno, "Unable to read IPC socket");
                break;
            }
            read += rc;
        }

        return read;
    }

    /**
     * Write data to the socket.
     * @param buf buffer of data of send
     * @param len number of bytes to write
     * @return true if operation completed successfully
     */
    bool write(const void* buf, size_t len) const
    {
        assert(fd != -1);

        while (len) {
            const ssize_t written = ::write(fd, buf, len);
            if (written == -1) {
                Log::error(errno, "Unable to write IPC socket");
                return false;
            }
            len -= written;
            buf = ((uint8_t*)buf) + written;
        }
        return true;
    }

private:
    int fd = -1;
};

class Sway : public UnixSocket {
private:
    /** Sway IPC env variable name. */
    static constexpr const char* ipc_env_path = "SWAYSOCK";

    /** Sway IPC magic header value. */
    static constexpr const uint8_t ipc_magic[] = {
        'i', '3', '-', 'i', 'p', 'c'
    };

    /** Sway IPC message type. */
    enum IpcType : uint8_t {
        RunCommand = 0,
        GetTree = 4,
    };

    /** Sway IPC message header. */
    struct __attribute__((__packed__)) IpcHeader {
        uint8_t magic[sizeof(ipc_magic)];
        uint32_t payload_sz;
        uint32_t type;
    };

    /**
     * Sway IPC message exchange.
     * @param type message type
     * @param payload payload data
     * @return response as JSON object
     */
    json request(IpcType mt, const std::string& payload) const
    {
        IpcHeader header;
        std::memcpy(header.magic, ipc_magic, sizeof(ipc_magic));
        header.type = mt;
        header.payload_sz = payload.length();

        std::vector<uint8_t> message;
        message.reserve(sizeof(IpcHeader) + payload.length());
        message.insert(message.end(), reinterpret_cast<const uint8_t*>(&header),
                       reinterpret_cast<const uint8_t*>(&header) +
                           sizeof(header));
        if (!payload.empty()) {
            message.insert(message.end(), payload.data(),
                           payload.data() + payload.length());
        }

        // message exchange: send request and get response header
        if (!write(message.data(), message.size()) ||
            !read(&header, sizeof(header))) {
            return {};
        }

        // get response payload
        message.resize(header.payload_sz);
        if (!read(message.data(), message.size())) {
            return {};
        }

        return json::parse(message, nullptr, false);
    }

    /**
     * Get currently focused window node.
     * @param node parent JSON node
     * @return pointer to focused window node
     */
    const json find_focused(const json& node) const
    {
        if (node.contains("focused")) {
            const auto& focused = node["focused"];
            if (focused.is_boolean() && focused.get<bool>()) {
                return node;
            }
        }

        for (const char* name : { "nodes", "floating_nodes" }) {
            if (!node.contains(name)) {
                continue;
            }
            const auto& nodes = node[name];
            if (!nodes.is_array()) {
                continue;
            }
            for (auto& subnode : nodes) {
                const auto& result = find_focused(subnode);
                if (!result.empty()) {
                    return result;
                }
            }
        }

        return {};
    }

    /**
     * Get rectangle description from json node.
     * @param node JSON node
     * @return rectangle geometry
     */
    Rectangle get_rect(const json& node) const
    {
        Rectangle rect;

        if (node.contains("x") && node.contains("y") &&
            node.contains("width") && node.contains("height")) {
            const json& x = node["x"];
            const json& y = node["y"];
            const json& width = node["width"];
            const json& height = node["height"];
            if (x.is_number() && y.is_number() && width.is_number() &&
                height.is_number()) {
                rect.x = x.get<ssize_t>();
                rect.y = y.get<ssize_t>();
                rect.width = width.get<size_t>();
                rect.height = height.get<size_t>();
            }
        }

        return rect;
    }

    /**
     * Get currently focused window position and size.
     * @return currently focused window geometry
     */
    Rectangle get_focus() const
    {
        Rectangle wnd;

        const json response = request(GetTree, {});
        const json focus = find_focused(response);

        if (focus.contains("rect") && focus.contains("window_rect")) {
            Rectangle rect = get_rect(focus["rect"]);
            Rectangle wrect = get_rect(focus["window_rect"]);
            if (rect && wrect) {
                wnd = rect;
                wnd.x += wrect.x;
                wnd.y += wrect.y;
            }
        }

        return wnd;
    }

public:
    /**
     * Set rules to create overlay window.
     * @param wnd geometry of app's window, if not valid - use parent window
     * @return true if operation completed successfully
     */
    static bool setup_overlay(Rectangle& wnd)
    {
        const char* sock_path = getenv("SWAYSOCK");
        if (!sock_path || !*sock_path) {
            return false;
        }

        Sway sway;
        if (!sway.connect(sock_path)) {
            return false;
        }

        if (!wnd) {
            wnd = sway.get_focus();
            if (!wnd) {
                return false;
            }
        }

        // add rules
        const std::string rules[] = {
            std::format("for_window [pid={}] floating enable", getpid()),
            std::format("for_window [pid={}] move absolute position {} {}",
                        getpid(), wnd.x, wnd.y),
        };
        for (const auto& it : rules) {
            sway.request(RunCommand, it);
        }

        return true;
    }
};

class Hyprland : public UnixSocket {
private:
    /**
     * Hyprland IPC message exchange.
     * @param req request to send
     * @return response as JSON object
     */
    static json request(const std::string& req)
    {
        const char* his = getenv("HYPRLAND_INSTANCE_SIGNATURE");
        if (!his || !*his) {
            return false;
        }
        const char* xrd = getenv("XDG_RUNTIME_DIR");
        if (!xrd || !*xrd) {
            return false;
        }
        const std::string sock_path =
            std::format("{}/hypr/{}/.socket.sock", xrd, his);

        Hyprland hl;
        if (!hl.connect(sock_path.c_str())) {
            return false;
        }

        // message exchange: send request and get response header
        char response[4096] = { 0 };
        if (!hl.write(req.c_str(), req.length()) ||
            !hl.read(response, sizeof(response))) {
            return {};
        }

        return json::parse(response, nullptr, false);
    }

    /**
     * Get currently focused window position and size.
     * @return currently focused window geometry
     */
    static Rectangle get_focus()
    {
        Rectangle wnd;
        int monitor_id;

        // get clients list
        const json clients = Hyprland::request("j/clients");
        if (!clients.contains("focusHistoryID")) {
            return {};
        }
        for (const auto& it : clients) {
            if (!it.contains("focusHistoryID") || !it.contains("at") ||
                !it.contains("size") || !it.contains("monitor")) {
                continue;
            }
            const json& focus_id = it["focusHistoryID"];
            if (!focus_id.is_number() || focus_id.get<size_t>() != 0) {
                continue;
            }
            const json& coords = it["at"];
            if (!coords.is_array() || coords.size() != 2 ||
                !coords.at(0).is_number() || !coords.at(1).is_number()) {
                continue;
            }
            const json& size = it["size"];
            if (!size.is_array() || size.size() != 2 ||
                !size.at(0).is_number() || !size.at(1).is_number()) {
                continue;
            }
            const json& monitor = it["monitor"];
            if (!monitor.is_number()) {
                continue;
            }
            monitor_id = monitor.get<int>();
            wnd.x = coords.at(0).get<ssize_t>();
            wnd.y = coords.at(1).get<ssize_t>();
            wnd.width = size.at(0).get<size_t>();
            wnd.height = size.at(1).get<size_t>();
            break;
        }
        if (!wnd) {
            return {};
        }

        // get monitors list
        const json monitors = Hyprland::request("j/monitors");
        if (monitors.empty()) {
            return {};
        }
        for (const auto& it : monitors) {
            if (!it.contains("id") || !it.contains("x") || !it.contains("y")) {
                continue;
            }
            const json& id = it["id"];
            if (!id.is_number() || id.get<int>() != monitor_id) {
                continue;
            }
            const json& x = it["x"];
            if (!x.is_number()) {
                continue;
            }
            const json& y = it["y"];
            if (!y.is_number()) {
                continue;
            }
            wnd.x -= x.get<ssize_t>();
            wnd.y -= y.get<ssize_t>();
            break;
        }

        return wnd;
    }

public:
    /**
     * Set rules to create overlay window.
     * @param wnd geometry of app's window, if not valid - use parent window
     * @param app_id application id (window class)
     * @return true if operation completed successfully
     */
    static bool setup_overlay(Rectangle& wnd, std::string& app_id)
    {
        if (!wnd) {
            wnd = Hyprland::get_focus();
            if (!wnd) {
                return false;
            }
        }

        // hyprland doesn't support "pid:" in window rules, so we have to use
        // dynamic app id
        app_id += '_';
        app_id += std::to_string(getpid());

        // add rules
        Hyprland::request(
            std::format("keyword windowrule float on, match:class {}", app_id));
        Hyprland::request(
            std::format("keyword windowrule move {} {}, match:class {}", wnd.x,
                        wnd.y, app_id));
        Hyprland::request(
            std::format("keyword windowrule size {} {}, match:class {}",
                        wnd.width, wnd.height, app_id));

        return true;
    }
};

bool Compositor::setup_overlay(Rectangle& wnd, std::string& app_id)
{
    bool rc = Sway::setup_overlay(wnd);
    if (!rc) {
        rc = Hyprland::setup_overlay(wnd, app_id);
    }
    return rc;
}
