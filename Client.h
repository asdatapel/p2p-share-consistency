#ifndef P2P_SHARE_CLIENT_H
#define P2P_SHARE_CLIENT_H

#include <iostream>
#include <vector>
#include <set>
#include <thread>
#include <mutex>
#include <string>
#include <sstream>
#include <algorithm>
#include <fstream>

#include <SFML/Network.hpp>

#include "Const.h"
#include "Connection.h"
#include "File.h"

struct LogItem {
	sf::Uint32 upstream;
	sf::Uint32 sourceId;
	sf::Uint32 sequence;
	sf::Time time;
};


struct Node {
	std::string ip;
	sf::Uint32 port;
};

struct FileInfo{
	sf::Uint32 originServer;
	std::string name;
	sf::Int32 version;
	sf::Int32 masterVersion;

	bool isValid;
};

class Client {
public:
	Client(sf::Uint32 id);
	~Client();

	void init();

	void go();
private:
	std::map<sf::Uint32, Connection *> peers; //static netowrk connections
	std::vector<Connection*> connections; //for direct downloads
	std::map<sf::Uint32, Node> nodes;
	sf::TcpListener listener;
	sf::SocketSelector waiter;

	std::mutex lock;

	std::vector<FileInfo> masterIndex;
	std::vector<FileInfo> copyIndex;
	std::set<std::string> pendingRequests;
	bool searchFile(std::string filename);

	FileInfo *getFileInfo(std::string filename);

	std::vector<File> incompleteFiles;
	File *findIncompleteFile(std::string filename);
	void removeIncompleteFile(std::string filename);

	std::list<LogItem> log;
	bool logQuery(sf::Uint32 peerId, sf::Uint32 sourceId, sf::Uint32 seq, sf::Uint32 ttl);

	void broadcastQuery(sf::Packet message, sf::Uint32 peerId);
	void sendUpstream(sf::Packet message, sf::Uint32 sourceId, sf::Uint32 seq);

	sf::Uint32 myID;
	std::string myIp;
	sf::Uint32 sequence;
	sf::Uint32 listenerPort;

	bool timeToExit;

	void incomingLoop();
	void inputLoop();
	void readConfigFile();

	void handleMessageFromNetwork(sf::Uint32 peerId);
	bool handleMessage(Connection* connection);
	void handleInput(std::string input);

	void handleQuit();

	void connectToPeers();
	void replacePeer(Connection *connection, sf::Uint32 id);
	Connection *findPeer(std::string ip, sf::Uint32 port);

	int pendingResponses;
	sf::Clock timer;

	sf::Clock logTimer;
	void flushLog();

};

#endif //P2P_SHARE_CLIENT_H
