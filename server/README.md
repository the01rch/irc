MPlexServer — Multiplexed TCP Server (epoll)

Overview
- MPlexServer is a tiny C++17 TCP server framework that uses non‑blocking sockets and epoll to handle many clients in a single thread.
- It provides a minimal API around: accepting connections, reading line‑based messages, writing responses, broadcasting, and event callbacks.
- The public API lives in `server/include/mplexserver.h` and is implemented in `server/src/mplexserver.cpp`.

Key features
- Single‑threaded, edge‑free design using `epoll` and non‑blocking I/O
- User‑defined event callbacks via `EventHandler`
- Line‑based message framing using CRLF (`"\r\n"`)
- Buffered, non‑blocking writes with automatic `EPOLLOUT` management
- Verbose logging with timestamps (3 levels)

Platform and requirements
- Linux (uses `epoll`, `<sys/epoll.h>`, `<arpa/inet.h>`, etc.)
- C++17 or newer toolchain

Constants
- `MAX_MSG_LEN = 512` — Maximum bytes read per recv call; IRC‑friendly line limit.
- `MAX_EPOLL_EVENTS = 10` — Number of events processed per `poll()` iteration.
- `VERBOSITY_MAX = 2` — Log level upper bound.

High‑level architecture
- One listening socket (`server_fd`), set to non‑blocking.
- An epoll instance (`epollfd`) monitors:
  - The listening socket for incoming connections (`EPOLLIN`).
  - Each client FD for readability (`EPOLLIN`) and remote hangups (`EPOLLRDHUP`).
  - Temporarily enables `EPOLLOUT` on a client when there is data buffered to send.
- Per‑connection state is tracked in maps by FD:
  - `client_map` (FD -> `Client`)
  - `recv_buffer` (FD -> partial text)
  - `send_buffer` (FD -> pending text)

Message framing
- Incoming bytes are appended to `recv_buffer[fd]`.
- When a CRLF delimiter (`"\r\n"`) is found, the substring up to and including CR is taken as one message and delivered to the handler as `Message`.
- The remaining bytes (after CRLF) stay in the buffer for the next message.
- Note: only the first complete line found per `recv_from_fd` call is dispatched (loop your `poll()` to flush more).

Public API (namespace `MPlexServer`)

Errors
- `class ServerError : std::runtime_error` — generic runtime errors (socket, bind, listen, epoll operations, send/recv failures not due to EAGAIN).
- `class ServerSettingsError : std::runtime_error` — configuration errors (e.g., invalid IPv4 address, invalid verbosity).

Utilities
- `void setNonBlocking(int fd);`
  - Sets a file descriptor to non‑blocking mode (throws on failure).

Data types
- `class Client`
  - Constructors: default, copy, `Client(int fd, sockaddr_in addr)`.
  - Accessors: `int getFd() const;`, `std::string getIpv4() const;`, `int getPort() const;`.
  - Represents a connected client by its socket FD and remote address.

- `class Message`
  - Constructors: default, copy, `Message(std::string msg, Client client)`.
  - Accessors: `const Client& getClient() const;`, `std::string getMessage() const;`.
  - Wraps a single line message (without trailing CRLF) and its origin client.

- `class EventHandler`
  - Interface you implement; not owned by `Server`.
  - Pure virtual callbacks:
    - `void onConnect(Client client);`
    - `void onDisconnect(Client client);`
    - `void onMessage(Message msg);`

- `enum class EventType { CONNECTED, DISCONNECTED, MESSAGE }` (internal dispatch enum)

Server
- `class Server`
  - Deleted default/copy/assign — must be constructed with a port and password or defaults to "6667" and "DaLeMa26".
  - `explicit Server(uint16_t port, std::string ipv4 = "", std::string password = "DaLeMa26");`
    - `port`: TCP port to listen on (fixed after construction).
    - `ipv4`: optional bind address; empty means all interfaces (`INADDR_ANY`).
    - `password`: optional server password; defaults to `DaLeMa26`.
  - `~Server();`
    - Automatically calls `deactivate()` if still active.

Lifecycle
- `void activate();`
  - Creates socket, sets `SO_REUSEADDR`, binds, listens, makes non‑blocking, and creates an epoll instance.
  - Adds the listening FD to epoll for `EPOLLIN`.
  - Logs "Server successfully activated" at verbosity ≥ 1.
- `void deactivate();`
  - Removes all client FDs from epoll, closes them, clears buffers/maps, closes epoll and listening sockets, resets state, and logs at verbosity ≥ 1.

Operation
- `void poll();`
  - Processes epoll events:
    - Accepts new clients when the listening FD is readable.
    - Reads from clients on `EPOLLIN`; on complete line CRLF, dispatches `onMessage`.
    - Writes pending bytes to clients on `EPOLLOUT` until buffer drains; then removes `EPOLLOUT`.
    - On `EPOLLRDHUP` or error, disconnects a client and dispatches `onDisconnect`.
- `void setEventHandler(EventHandler* handler);`
  - Assigns the callback target. The pointer must remain valid while the server is running; `Server` does not take ownership.

Messaging
- `void sendTo(const Client& c, std::string msg);`
  - Queues `msg` for the client; enables `EPOLLOUT` for that FD. Multiple calls append to the pending buffer.
  - Note: you should include your own line terminators (e.g., CRLF) if the protocol expects them.
- `void broadcast(std::string message);`
  - Sends `message` to all connected clients.
- `void multisend(const std::vector<Client>& clients, std::string message);`
  - Sends `message` to a subset of clients.
- `void disconnectClient(const Client& c);`
  - Closes the connection and removes the client; triggers `onDisconnect`.

Introspection and logging
- `int getConnectedClientsCount() const;` — number of currently connected clients.
- `void setVerbose(int level);`
  - Levels: 0 (critical only), 1 (connections/disconnections), 2 (I/O data + debug + level 1).
  - Throws `ServerSettingsError` if `level` is outside `[0, VERBOSITY_MAX]`.
- `int getVerbose() const;`

Behavioral notes and gotchas
- CRLF framing: only messages terminated by `"\r\n"` are dispatched. If your client sends LF‑only, you will not receive callbacks.
- Partial reads/writes: handled internally via per‑FD buffers; continue calling `poll()` regularly.
- Reentrancy: Callbacks run inside `poll()`; do not call `poll()` again from inside a callback.
- Thread safety: The server is not thread‑safe; use it from a single thread.
- Handler lifetime: You must set a valid `EventHandler*` via `setEventHandler()` before expecting callbacks; keep it alive until `deactivate()`.
- Send errors: On send/recv errors other than `EAGAIN`, the client is disconnected.
- Backpressure: Large `sendTo()` volume will buffer in memory per FD; design your application‑level flow control accordingly.

Quick start
```cpp
#include "server/include/mplexserver.h"
using namespace MPlexServer;

struct MyHandler : EventHandler {
    void onConnect(Client c) override {
        std::cout << "Client connected: " << c.getIpv4() << ":" << c.getPort() << "\n";
    }
    void onDisconnect(Client c) override {
        std::cout << "Client disconnected FD=" << c.getFd() << "\n";
    }
    void onMessage(Message m) override {
        auto &c = m.getClient();
        std::string line = m.getMessage();
        // Echo back the received line, ensure CRLF is present
        server->sendTo(c, line + "\r\n");
    }
    // Reference to the server to respond; set externally
    Server* server = nullptr;
};

int main() {
    Server srv(6667);           // Bind on all interfaces
    srv.setVerbose(2);          // Verbose logging

    MyHandler h; h.server = &srv;
    srv.setEventHandler(&h);

    srv.activate();
    while (true) {
        srv.poll();             // Drive the server
        // Do other periodic work here if needed
    }
}
```

Build notes
- The repository already contains Makefiles (`Makefile` at root and `server/Makefile`).
- Ensure you link with the static library or object files produced for the server, or include the sources directly.

Design limits
- IPv4 only (uses `sockaddr_in`, `AF_INET`).
- Accept backlog uses `SOMAXCONN`.
- Default `MAX_EPOLL_EVENTS` is 10 per iteration; adjust in the header if needed.

Changelog
- 2025‑11‑13: Initial README authored for `mplexserver.h` API.

License
- This README documents the MPlexServer API contained in this repository. See project root for licensing information, if any.
