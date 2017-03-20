#include "Client.h"

Client::Client(sf::Uint32 id) {
	myID = id;

	timeToExit = false;
	myIp = sf::IpAddress::getPublicAddress(sf::seconds(10)).toString();
	sequence = 0;
	listenerPort = 0; //random port number

	logTimer.restart();
}

Client::~Client() {
	handleQuit();
}

//start the listener and give it a port
//try to connect to server
void Client::init() {
	std::cout << "Starting Client..." << "\n";

	readConfigFile();
	connectToPeers();

	if (listener.listen(listenerPort) != sf::Socket::Done){
		std::cout << "Could not bind socket. Please wait a short while before restarting\n";
		exit(1);
	}
	listenerPort = listener.getLocalPort();
	waiter.add(listener);

}

//begin both threads
void Client::go() {
	std::thread loopThread(&Client::incomingLoop, this);

	inputLoop();

	loopThread.join();
}

void Client::readConfigFile() {
	std::ifstream input("config");


	//read the list of peers with ids
	std::string line;
	while (std::getline(input, line)) {
		std::istringstream ss(line);
		sf::Uint32 id, port;
		std::string ip;
		if (!(ss >> id >> ip >> port)) {
			break;
		}
		if (id == -1) {
			break;
		}

		nodes[id] = Node{ip, port};

		if (id == myID) {
			listenerPort = port;
		}
	}

	//read all neighbors
	while (std::getline(input, line)) {
		std::istringstream ss(line);
		int id;

		if (!(ss >> id)) {
			break;
		}
		if (id != this->myID) {
			continue;
		}

		int peerId;
		while ((ss >> peerId)) {
			peers[peerId] = new Connection(nodes[peerId].ip, nodes[peerId].port);
		}
	}

	std::cout << "Config read\n";
}


//this is the loop that takes in user commands
void Client::inputLoop() {
	std::cout << "Ready for input.\n";
	while (!timeToExit) {
		std::string input;
		std::getline(std::cin, input);

		handleInput(input);
	}
}

//handle messages from peers
void Client::handleMessageFromNetwork(sf::Uint32 peerId) {
	Connection *peer = peers[peerId];

	sf::Packet packet;
	sf::Socket::Status status = peer->socket.receive(packet);
	if (status != sf::Socket::Done) {
		//TODO: do some error stuff
		return;
	}

	sf::Uint32 destId;
	sf::Uint32 sourceId;
	sf::Uint32 seq;
	sf::Uint32 ttl;
	packet >> destId;
	packet >> sourceId >> seq;
	packet >> ttl;

	sf::Int32 message_type;
	packet >> message_type;
	if (message_type == NOTIFY_PEER_DISCONNECT) {
		std::cout << "Peer: {" << peer->toString() << "} disconnected\n";

		waiter.remove(peer->socket);
		peer->socket.disconnect();

		for (auto &peer1 : peers) {
			if (peer1.second == peer) {
				peer1.second->socket.disconnect();
			}
		}
	} else if (message_type == QUERY_FILE_LOCATION) {
		std::string filename;
		packet >> filename;

		std::cout << "Received request for file: " << filename << "\n";

		if (searchFile(filename)) {
			sf::Packet response;
			response << sourceId << sourceId << seq << (sf::Uint32)0 << GIVE_FILE_LOCATION << filename << myID;

			peer->socket.send(response);

			std::cout << "Query hit for file: " << filename << ". Sending file location upstream\n";
		} else {
			if (ttl > 0 && logQuery(peerId, sourceId, seq, ttl)) {
				sf::Packet forward;
				forward << destId << sourceId << seq << ttl - 1 << QUERY_FILE_LOCATION << filename;

				broadcastQuery(forward, peerId);
				std::cout << "Forwarding query for file: " << filename << "\n";
			}

		}

	} else if (message_type == GIVE_FILE_LOCATION) {
		std::string filename;
		sf::Uint32 id;

		packet >> filename >> id;

		if (destId == myID) {
			if (pendingRequests.find(filename) != pendingRequests.end()) {
				pendingRequests.erase(filename);
				std::cout << "File found at node : " << id << "\n";

				Connection *newPeer = new Connection(nodes[id].ip, nodes[id].port);
				if (newPeer->socket.connect(newPeer->ip, newPeer->port) != sf::Socket::Done) {
					std::cout << "Failed to connect to {" << newPeer->toString() << "}\n";
				}
				connections.push_back(newPeer);
				waiter.add(newPeer->socket);

				sf::Packet message;
				message << REQUEST_FILE << filename;
				newPeer->socket.send(message);
			}
		} else {
			sf::Packet forward;
			forward << destId << sourceId << seq << (sf::Uint32)0 << GIVE_FILE_LOCATION << filename << id;

			std::cout << "Passing file location upstream\n";

			sendUpstream(forward, sourceId, seq);
		}
	} else if (message_type == TEST_QUERY) {
		if (destId == myID) {
			sf::Packet response;
			response << sourceId << sourceId << seq << (sf::Uint32)0 << TEST_RESPONSE;

			peer->socket.send(response);

		} else {
			sf::Packet forward;
			forward << destId << sourceId << seq << ttl - 1 << TEST_QUERY;
			broadcastQuery(forward, peerId);
		}
	} else if (message_type == TEST_RESPONSE) {
		if (destId == myID) {
			--pendingResponses;
			if (pendingResponses == 0){
				std::cout << "Response time: " << timer.getElapsedTime().asMilliseconds() << " ms\n";
			}
		} else {
			sf::Packet forward;
			forward << sourceId << sourceId << seq << (sf::Uint32)0 << TEST_RESPONSE;
			broadcastQuery(forward, peerId);
		}
	} else if (message_type == INVALIDATE) {
		std::string filename;
		sf::Int32 version;
		packet >> filename >> version;
		if (ttl > 0 && logQuery(peerId, sourceId, seq, ttl)) {

			for (auto& f : copyIndex) {
				if (f.name == filename){
					f.masterVersion = version;
				}
			}

			sf::Packet forward;
			forward << destId << sourceId << seq << ttl - 1 << INVALIDATE << filename << version;

			broadcastQuery(forward, peerId);
			std::cout << "File Invaildated & Forwarding invalidation for file: " << filename << "\n";
		}
	} else {
		std::cout << "Received unknown message type from network peer: {" << peer->toString() << "} header: " << message_type
				  << "\n";
	}
}

bool Client::handleMessage(Connection *peer) {
	sf::Packet packet;
	sf::Socket::Status status = peer->socket.receive(packet);
	if (status != sf::Socket::Done) {
		//TODO: do some error stuff
		return false;
	}

	sf::Int32 message_type;
	packet >> message_type;
	if (message_type == CONNECT_AS_NEIGHBOR) {
		sf::Uint32 id;
		packet >> id;

		replacePeer(peer, id);

		std::cout << "Moved connection to network peer\n";
		return true;
	} else if (message_type == GIVE_FILE_PORTION) {
		std::string filename;
		packet >> filename;

		File *file = findIncompleteFile(filename);

		if (file) {
			std::cout << "Downloading file: \"" << filename << "\" Pieces Completed: " << file->getCompletedPieceCount()
					  << " / " << file->getPieceCount() << "  " << file->getCompletionPercentage() * 100 << "%\n";

			file->takeIncoming(packet);
			if (file->isComplete()) {
				std::cout << "File: \"" << filename << "\" download complete. Writing to disk...\n";
				std::cout << "Display file: \"" << filename << "\"\n";
				file->writeToDisk();
				removeIncompleteFile(filename);

				std::cout << "File: \"" << filename << "\" written to disk\n";
			}
		}
	} else if (message_type == REQUEST_FILE) {
		std::string filename;
		packet >> filename;

		File file;
		file.initFromDisk(filename);

		FileInfo *fileInfo = getFileInfo(filename);

		sf::Packet response;
		response << NOTIFY_STARTING_TRANSFER;
		response << filename << (sf::Uint32) file.size << fileInfo->originServer << fileInfo->version ;
		peer->socket.send(response);

		file.send(peer);

		std::cout << "Starting upload file: \"" << filename << "\" from {" << peer->toString() << "}\n";
	} else if (message_type == NOTIFY_STARTING_TRANSFER) {
		std::string filename;
		sf::Uint32 size;
		sf::Uint32 originServer;
		sf::Int32 version;
		packet >> filename >> size >> originServer >> version;

		FileInfo f;
		f.version = version;
		f.masterVersion = version;
		f.name = filename;
		f.isValid = true;
		f.originServer = originServer;
		copyIndex.push_back(f);

		File file;
		file.init(filename, size);

		incompleteFiles.push_back(file);

		std::cout << "Beginning download of file: \"" << filename << "\" from {" << peer->toString() << "}\n";
	} else {
		std::cout << "Received unknown message type from peer: {" << peer->toString() << "} header: " << message_type
				  << "\n";
	}

	return false;
}


//parse cmd input and handle it
void Client::handleInput(std::string input) {
	std::vector<std::string> commandParts;

	//parse the input, should probably be in own function
	std::replace(input.begin(), input.end(), '\n', ' ');
	std::stringstream stream(input);
	std::string part;
	int i = 0;
	while (std::getline(stream, part, ' ')) {
		commandParts.push_back(part);
		++i;
	}

	lock.lock();

	if (commandParts[0] == "exit") {
		handleQuit();
	} else if (commandParts[0] == "getfile") {
		sf::Packet message;
		message << (sf::Uint32) 0 << myID << sequence << (sf::Uint32) 10 << QUERY_FILE_LOCATION << commandParts[1];
		++sequence;
		pendingRequests.insert(commandParts[1]);

		broadcastQuery(message, myID);

	} else if (commandParts[0] == "addfile") {
		FileInfo f;
		f.version = 0;
		f.masterVersion = 0;
		f.name = commandParts[1];
		f.isValid = true;
		f.originServer = myID;
		masterIndex.push_back(f);
		std::cout << "File added to master index: " << commandParts[1] << "\n";
	} else if (commandParts[0] == "testresponse") { //test response time
		int n = std::stoi(commandParts[2]);
		pendingResponses = n;

		std::cout << "Testing Server Response time with " << n << " queries\n";
		timer.restart();

		lock.unlock();

		for (int i = 0; i < n; ++i) {
			sf::Packet message;
			message << (sf::Uint32)std::stoi(commandParts[1]) << myID << sequence << (sf::Uint32) 10 << TEST_QUERY;
			++sequence;

			broadcastQuery(message, myID);
		}

		lock.lock();

	} else if (commandParts[0] == "modifyfile") {
		std::string filename = commandParts[1];
		FileInfo *f = getFileInfo(filename);

		f->version++;
		f->masterVersion++;

		sf::Packet message;
		message << myID << myID << ++sequence << (sf::Uint32)20 << INVALIDATE << filename << f->masterVersion;
		broadcastQuery(message, myID);

		std::cout << "Modified file\n";

	} else if (commandParts[0] == "printfiles") {
		std::cout << "Master files:\n";
		for (auto& f : masterIndex){
			std::cout << f.name << ", " << f.masterVersion << "\n";
		}
		std::cout << "\nCopy files:\n";
		for (auto& f : copyIndex){
			std::cout << f.name << ", " << f.masterVersion << "\n";
		}

	} else {
		std::cout << "Sorry, unknown command\n";
	}

	lock.unlock();
}


//this is the loop that waits for messages and passes them to handler functions
//also takes care of new incoming connections
void Client::incomingLoop() {
	while (true) {
		bool anythingReady = waiter.wait(sf::seconds(2)); //wait for message to come in or new connection

		lock.lock();

		flushLog();

		if (!anythingReady) { // check if something actually came in
			if (timeToExit) {
				lock.unlock();
				return;
			}
		} else {
			//check if new connection
			if (waiter.isReady(listener)) {
				Connection *newPeer = new Connection();
				listener.accept(newPeer->socket);

				newPeer->ip = newPeer->socket.getRemoteAddress().toString();
				newPeer->port = newPeer->socket.getRemotePort();

				waiter.add(newPeer->socket);

				connections.push_back(newPeer);

				std::cout << "Connected to {" << newPeer->toString() << "}\n";
			}

			//check if something from a network peer
			for (auto &peer : peers) {
				if (waiter.isReady(peer.second->socket)) {
					handleMessageFromNetwork(peer.first);
				}
			}

			//check if something from a non-network peer
			for (int i = 0; i < connections.size(); ++i) {
				if (waiter.isReady(connections[i]->socket)) {
					if (handleMessage(connections[i])) {
						connections.erase(connections.begin() + i);
						--i;
					}
				}
			}

		}
		lock.unlock();
	}
}

//send disconnect messages to peers, then exit
void Client::handleQuit() {
	waiter.remove(listener);
	listener.close();

	sf::Packet message;
	message << (sf::Uint32) 0 << myID << sequence << (sf::Uint32) 5 << NOTIFY_PEER_DISCONNECT;
	for (auto &peer : peers) {
		peer.second->socket.send(message);
		waiter.remove(peer.second->socket);
		peer.second->socket.disconnect();
		delete peer.second;
	}

	peers.clear();

	timeToExit = true;
}

//search downloading files
File *Client::findIncompleteFile(std::string filename) {
	for (int i = 0; i < incompleteFiles.size(); ++i) {
		if (incompleteFiles[i].filename == filename) {
			return &incompleteFiles[i];
		}
	}

	return nullptr;
}

//remove a pending file, usually because its complete
void Client::removeIncompleteFile(std::string filename) {
	for (int i = 0; i < incompleteFiles.size(); ++i) {
		if (incompleteFiles[i].filename == filename) {
			incompleteFiles.erase(incompleteFiles.begin() + i);
		}
	}
}

void Client::connectToPeers() {
	for (auto &peer : peers) {
		std::cout << "Attempting to connect to peer {" << peer.second->toString() << "}...\n";

		if (peer.second->ip == myIp) {
			peer.second->ip = sf::IpAddress::LocalHost.toString();
		}
		if (peer.second->socket.connect(peer.second->ip, peer.second->port) != sf::Socket::Done) {
			std::cout << "Failed to connect to {" << peer.second->toString() << "}\n";
			continue;
		}

		sf::Packet message;
		message << CONNECT_AS_NEIGHBOR << myID;
		peer.second->socket.send(message);

		waiter.add(peer.second->socket);
		std::cout << "Connected to {" << peer.second->toString() << "}\n";
	}
}

void Client::replacePeer(Connection *connection, sf::Uint32 id) {
	delete peers[id];
	peers[id] = connection;
}

Connection *Client::findPeer(std::string ip, sf::Uint32 port) {
	for (auto &peer:peers) {
		if (peer.second->ip == ip && peer.second->port == port) {
			return peer.second;
		}
	}
	return nullptr;
}

bool Client::searchFile(std::string filename) {
	for (int i = 0; i < masterIndex.size(); ++i) {
		if (filename == masterIndex[i].name) {
			return true;
		}
	}
	for (int i = 0; i < copyIndex.size(); ++i) {
		if (filename == copyIndex[i].name && copyIndex[i].version == copyIndex[i].masterVersion) {
			return true;
		}
	}
	return false;
}

bool Client::logQuery(sf::Uint32 peerId, sf::Uint32 sourceId, sf::Uint32 seq, sf::Uint32 ttl) {
	if (sourceId == myID){
		return false;
	}
	for (auto &logitem : log) {
		if (seq == logitem.sequence && sourceId == logitem.sourceId) {
			return false;
		}
	}

	LogItem l{peerId, sourceId, seq, logTimer.getElapsedTime()};
	log.push_back(l);

	return true;
}

void Client::broadcastQuery(sf::Packet message, sf::Uint32 peerId) {
	for (auto &peer : peers) {
		if (peer.first != peerId) { // make sure not to resend backwards
			peer.second->socket.send(message);
		}
	}
}

void Client::sendUpstream(sf::Packet message, sf::Uint32 sourceId, sf::Uint32 seq) {
	for (auto &logitem : log) {
		if (seq == logitem.sequence && sourceId == logitem.sourceId) {
			peers[logitem.upstream]->socket.send(message);
		}
	}
}

void Client::flushLog() {
	std::list<LogItem>::iterator i = log.begin();
	while (i != log.end()) {
		if (logTimer.getElapsedTime().asSeconds() > i->time.asSeconds() + 20.0) {
			log.erase(i++);
		}else{
			i++;
		}

	}
}
FileInfo* Client::getFileInfo(std::string filename) {
	for (int i = 0; i < masterIndex.size(); ++i) {
		if (filename == masterIndex[i].name) {
			return &masterIndex[i];
		}
	}
	for (int i = 0; i < copyIndex.size(); ++i) {
		if (filename == copyIndex[i].name) {
			return &copyIndex[i];
		}
	}
	return nullptr;
}

