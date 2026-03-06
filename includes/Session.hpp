#ifndef SESSION_HPP
#define SESSION_HPP

#include <string>

class Session {
    private:
        int         _sockFd;
        std::string _nick;
        std::string _user;
        std::string _recvBuf;
        std::string _outBuf;
        bool        _passOk;
        bool        _welcomed;

        // Non-copyable
        Session(const Session&);
        Session& operator=(const Session&);

    public:
        Session(int fd);
        ~Session();

        int         getSocket() const;
        std::string getNick() const;
        std::string getUser() const;
        std::string getRecvBuf() const;
        bool        hasValidPass() const;
        bool        isWelcomed() const;

        void setNick(const std::string& nick);
        void setUser(const std::string& user);
        void markPassOk(bool ok);
        void markWelcomed(bool w);

        void feedRecvBuf(const std::string& chunk);
        void resetRecvBuf();

        void pushToOutBuf(const std::string& data);
        const std::string& getOutBuf() const;
        void drainOutBuf(size_t bytes);
        bool hasQueuedData() const;
};

#endif
