class TCPsocket {
public:
	TCPsocket::TCPsocket(void);
	int attach(char *, char *);
	void detach(void);
	void serv_read(char *, int);
	void serv_write(char *, int);
	void serv_gets(char *);
	void serv_puts(char *);
	bool is_connected(void);
private:
	int serv_sock = (-1);
	int connectsock(char *, char *, char *);
	void timeout(int);
};

