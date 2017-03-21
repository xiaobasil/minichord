#include <iostream>
#include <cstdio>
#include <sha.h>
#include <memory>
#include <thread>
#include "Chord.h"
#include "Node.h"
#include "utils/csapp.h"
#include <sstream>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <time.h>
using namespace std;

shared_ptr<Chord> Chord::myInstance = nullptr;

void Chord::Listen() {
	assert(myKey != 0);

	myListenFD = Open_listenfd(myListenPort);
	if (myListenFD < 0) {
		exit(-1);
	}

	int optval = 1;
	setsockopt(myListenFD, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int));

	cout << "Chord listening at " << myIPAddress << ":" << myListenPort << endl;

	socklen_t client_len;
	struct sockaddr_in client_addr;

	int newConnection;

	vector<thread> threads;

	//thread monitorThread(&Chord::monitorLinks, this);
	thread stabilizeThread(&Chord::stabilize, this);
	while (true) {
		client_len = sizeof(client_addr);
		newConnection = Accept(myListenFD, (sockaddr*) &client_addr, &client_len);

        #ifdef DEBUG
		cout << "New Connection from " << inet_ntoa(client_addr.sin_addr) << ":" << dec << ntohs(client_addr.sin_port) << endl;
        #endif

		threads.push_back(thread(&Chord::handleRequest, this, newConnection, client_addr));
	}

}

Chord::Chord()
{
	myListenPort=0,
	myListenFD=0;
	myIPAddress = Chord::getLocalIPAddress();
}

Chord::~Chord() {
	Close(myListenFD);
}
void Chord::initFingerTable(Node &node){
	myFingerTable[0].finger_node=Successors[0];
	chord_key max=0-1;
	for(size_t i=0;i<MAX_FTE_NUM-1;i++)
	{
		if(Chord::inRange(myKey,myFingerTable[i].finger_node->getKey(),myFingerTable[i+1].start,true,true))//include interval begin and end
		{
			myFingerTable[i+1].finger_node=myFingerTable[i].finger_node;
		}
		else
		{
			myFingerTable[i+1].finger_node=node.SearchSuccessor(myFingerTable[i+1].start);
		}
	}

}
void Chord::JoinRing(std::string entry_ip, int entry_port) {
	Node entryNode = Node(entry_ip, entry_port);
	if (entryNode.Connect()) {
		cout << "Joining the Chord ring." << endl;
		cout << "Your position is " << hex << myKey << "." << endl;

		// get the successor to connect to
		auto successor = entryNode.SearchSuccessor(myKey);

		cout << "Your successor is " << successor->toString();
		auto pred = successor->getPredecessor();
		cout << "Your predecessor is " << pred->toString();

		// this needs to happen all the time
		// works for one in the node, or infinite
		insertSuccessor(1, successor);
		insertPredecessor(1, pred);

		// if pred != succ, then we can do this
		if (successor->getKey() != pred->getKey()) {
			insertSuccessor(2, successor->getSuccessor(1));
			insertPredecessor(2, pred->getPredecessor(1));
		}
		initFingerTable(entryNode);
	} else {
		cout << "Could not connect to Node." << endl;
		exit(-1);
	}
}

std::shared_ptr<Node> Chord::searchSuccessorLocal(chord_key key)
{
	// get best option we know of
	auto node =findSuccessor(key);
	while (true) {
		// check this entry for validity
		// pred is less than key
		auto pred = node->getPredecessor();
		// check if key between predecessor and provided successor
		if (Chord::inRange(pred->getKey(), node->getKey(), key)) {
			// if so, then this is the successor!
			return node;
			break;
		} else {
			//cout << "We have " << hex << node->getKey() << ". We are looking for " << hex << key << endl;
			// get the next best attempt
			node = node->FindSuccessor(key);
		}
	}
}
void Chord::fixFinger()
{
	srand(time(NULL));
	unsigned int index=rand()%MAX_FTE_NUM;//choose an index randomly
	auto node=searchSuccessorLocal(myFingerTable[index].start);
	myFingerTable[index].finger_node=node;//update finger table entry
}
void Chord::handleRequest(int socket_fd, sockaddr_in sockaddr) {
	// cout << "Processing connection." << endl;

	shared_ptr<RIOBuffered> connection(new RIOBuffered(socket_fd));

	stringstream msg;
	msg << "My ID is " <<  hex << myKey << endl;

	connection->writeLine(&(Chord::WELCOME_MESSAGE));

	string s = msg.str();
	RIO::writeString(socket_fd, &s);

	string message = connection->readLine();
	stringstream parse(message);

	string command;
	parse >> command;

	if (command.compare("Node") == 0) {
		// Node identification message: Node Key IP:Port

		shared_ptr<Node> node = Node::createFromInfo(parse);
		if (node == nullptr) {
			string msg("You are using an invalid key.\n");
			RIO::writeString(socket_fd, &msg);
		} else {
			node->processCommunication(connection);
		}
	} else if (command.find("Query") == 0) {
		// queries are just nodes that have wrong ip addresses
		// qu
	} else {
		RIO::writeString(socket_fd, &(Chord::ERROR_GOODBYE_MESSAGE));
		cout << "Unknown client connected." << endl;
	}

	shutdown(socket_fd, 0);
	// don't care about double closing
	close(socket_fd);
}

chord_key Chord::hashKey(std::string value) {
	byte output[CryptoPP::SHA1::DIGESTSIZE];
	CryptoPP::SHA1().CalculateDigest(output, (byte*) value.c_str(), value.length());

	chord_key resultKey = 0;
	// 20 byte SHA key
	// turn it into a 4 byte key
	for (unsigned int i = 0; i < sizeof(output); i += 4) {
		chord_key current;
		memcpy(&current, (void*) (output + i), 4);
		resultKey = resultKey ^ current;
	}

	return resultKey;
}

std::shared_ptr<Chord> Chord::getInstance() {
	if (myInstance == nullptr) {
		myInstance = shared_ptr<Chord>(new Chord());
	}
	return myInstance;
}

void Chord::init(int port) {
	myListenPort = port;
	myKey = Chord::hashKey(myIPAddress + ":" + to_string(myListenPort));
	NodeInfo = shared_ptr<Node>(new Node(myIPAddress, myListenPort));
	chord_key max=0-1;
	for(size_t i=0;i<MAX_FTE_NUM;i++)
	{
		chord_key start=(myKey+1<<i)%max; //check overflow,myFingerTable[i+1]'s start
		myFingerTable.push_back(FingerEntry(start,NodeInfo));
	}
	/// init the succ and pred
	Successors.insert(Successors.begin(), NUM_PRED_SUCC, NodeInfo);
	Predecessors.insert(Predecessors.begin(), NUM_PRED_SUCC, NodeInfo);
}

std::string Chord::getLocalIPAddress() {
	struct ifaddrs *ifaddr, *ifa;
	int n, s;
	int family;
	char host[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		exit(EXIT_FAILURE);
	}

	for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
		if (ifa->ifa_addr == nullptr) {
			continue;
		}

		family = ifa->ifa_addr->sa_family;
		if (family == AF_INET) {
			if (strcmp(ifa->ifa_name, "lo") != 0) {
				s = getnameinfo(ifa->ifa_addr, (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL,
						0, NI_NUMERICHOST);
				if (s != 0) {
					printf("getnameinfo() failed: %s\n", gai_strerror(s));
					exit(EXIT_FAILURE);
				}
			}
		}

	}
	freeifaddrs(ifaddr);
	return string(host);
}

std::string Chord::toString() {
	stringstream result;
	result << hex << myKey << " " << myIPAddress << ":" << dec << myListenPort << endl;
	return result.str();
}

std::shared_ptr<Node> Chord::findSuccessor(chord_key key) {//find the successor of the closest predecessor of key
	shared_ptr<Node> n;
	n = findPredecessor(key);
	return n->getSuccessor();
}

std::shared_ptr<Node> Chord::findPredecessor(chord_key key) {//find the closest predecessor of key
	shared_ptr<Node> n = NodeInfo;
	/*// TODO: iterate over the finger table instead of the successor
	for (int i = Successors.size() - 1; i >= 0; --i) {
		// key in (current, mySuccessors[i])
		if (Chord::inRange(myKey, key, Successors[i]->getKey(), false)) {
			n = Successors[i];
			return n;
		}
	}*/
	for (int i = myFingerTable.size() - 1; i >= 0; --i) {
		if(Chord::inRange(myKey,key,myFingerTable[i].finger_node->getKey(),false))
		{
			n=myFingerTable[i].finger_node;
			return n;
		}
	}
	return n;
}

void Chord::parseIPPort(std::string message, std::string* ip, int* port) {
	*port = std::stoi(message.substr(message.find(":") + 1));
	*ip = message.substr(0, message.find(":"));
}

std::tuple<chord_key, chord_key> Chord::getRange() {
	int lowerKey = myKey;
	if (Predecessors.size() > 0) {
		lowerKey = Predecessors[0]->getKey();
	}
	return tuple<chord_key, chord_key>(lowerKey, myKey);
}

void Chord::LeaveRing() {
	cout << "Leaving Ring" << endl;

	Successors[0]->replacePredecessor(Predecessors[0].get(), 1, true);
	// could tell myself to point to predecessor - wasteful
	if (Successors[1]->getKey() != myKey) {
		Successors[1]->replacePredecessor(Predecessors[0].get(), 2, true);
	}
	// could tell my next to point to me - bad!
	if (Predecessors[1]->getKey() != myKey) {
		Successors[0]->replacePredecessor(Predecessors[1].get(), 2, true);
	}

	// we're down - quit
	cout << "All set. Cheers Friends." << endl;
	exit(0);
}

void Chord::insertNode(size_t index, std::vector<std::shared_ptr<Node>>& items, std::shared_ptr<Node> node) {
	// index starts at 1
	if (index > NUM_PRED_SUCC || index == 0) {
		// we don't support this
		cerr << "Tried to insert a Node with index not supported: " << index << endl;
		return;
	}

	// set our pointer
	items.insert(items.begin() + index - 1, node);

	// trim the list to max size (don't want to store more)
	if (items.size() >= NUM_PRED_SUCC) {
		items.resize(NUM_PRED_SUCC);
	}
}

void Chord::replaceNode(size_t index, std::vector<std::shared_ptr<Node>>& items, std::shared_ptr<Node> node) {
	// index starts at 1
	if (index > NUM_PRED_SUCC || index == 0) {
		// we don't support this
		cerr << "Tried to replace a Node with index not supported: " << index << endl;
		return;
	}

	// set our pointer
	items[index - 1] = node;
}

void Chord::spliceSuccessor(size_t index) {
	if (index > NUM_PRED_SUCC || index == 0) {
		return;
	}
	// we can't talk to node.
	auto node = Successors[index - 1];
	if (node->getKey() == myKey) {
		// this doesn't make sense
		return;
	}
	// we need to get information from the successor that hasn't failed
	// what if they are already updated?
	// get both at the same time - check if value is already changed

	size_t otherIndex = NUM_PRED_SUCC - index; // ((index + NUM_PRED_SUCC - 1) % NUM_PRED_SUCC) + 1;
	auto other = Successors[otherIndex];

	std::shared_ptr<Node> newSuccessor;

	for (size_t i = 1; i <= NUM_PRED_SUCC; i++) {
		auto n = other->getSuccessor(i);

		// don't want to get the one we're removing of course
		if (n->getKey() != node->getKey()) {
			newSuccessor = n;
			break;
		}
	}

	if (newSuccessor.get() == nullptr) {
		cerr << "WHAT" << endl;
	}

	if (index == 1) {
		replaceNode(1, Successors, other);
		replaceNode(2, Successors, newSuccessor);
	} else if (index == 2) {
		replaceNode(2, Successors, newSuccessor);
	}
}

void Chord::splicePredecessor(size_t index) {
	if (index > NUM_PRED_SUCC || index == 0) {
		return;
	}
	// we can't talk to node.
	auto node = Predecessors[index - 1];
	if (node->getKey() == myKey) {
		// this doesn't make sense
		return;
	}
	// we need to get information from the successor that hasn't failed
	// what if they are already updated?
	// get both at the same time - check if value is already changed

	size_t otherIndex = NUM_PRED_SUCC - index; // ((index + NUM_PRED_SUCC - 1) % NUM_PRED_SUCC) + 1;
	auto other = Predecessors[otherIndex];

	std::shared_ptr<Node> newPred;

	for (size_t i = 1; i <= NUM_PRED_SUCC; i++) {
		auto n = other->getPredecessor(i);

		// don't want to get the one we're removing of course
		if (n->getKey() != node->getKey()) {
			newPred = n;
			break;
		}
	}

	if (newPred.get() == nullptr) {
		cerr << "WHAT" << endl;
	}

	if (index == 1) {
		replaceNode(1, Predecessors, other);
	}
	replaceNode(2, Predecessors, newPred);
}

void Chord::stabilize() {
	while (true) {
		// check successors alive or not
		for (size_t i = 0; i < Successors.size(); ++i) {
			auto node = Successors[i];
			if (node->getKey() == myKey)
				continue;

			if (!node->checkConnection()) {
				cout << "Error with Successor " << i + 1 << endl;
				spliceSuccessor(i + 1);
			}
		}

		// check predecessors alive or not
		for (size_t i = 0; i < Predecessors.size(); ++i) {
			auto node = Predecessors[i];
			if (node->getKey() == myKey)
				continue;
			if (!node->checkConnection()) {
				cout << "Error with Predecessor " << i + 1 << endl;
				splicePredecessor(i + 1);
			}
		}
       #ifdef DEBUG
		//cout<<"monitorLinks"<<endl;
       #endif


		if(Successors[0]->getKey()!=myKey)
				{
				shared_ptr<Node> temp=Successors[0]->getPredecessor(1);
				if(Chord::inRange(myKey,Successors[0]->getKey(),temp->getKey(),false,false))//(lower,upper) interval
				{
					Successors[0]=temp;//update direct successor
				}
				Successors[0]->notify(this->NodeInfo);//notify its successor
				fixFinger();//fix finger table

		        #ifdef DEBUG
				cout<<"stabilize"<<endl;
		         #endif
				}
        cout<<hex<<Predecessors[0]->getKey()<<"->"<<hex<<myKey<<"->"<<hex<<Successors[0]->getKey()<<endl;
		this_thread::sleep_for(std::chrono::milliseconds(MONITOR_PERIOD));
	}
}

bool Chord::inRange(chord_key lower, chord_key upper, chord_key key, bool inclusiveEnd,bool inclusiveBegin) {
	// need to check if the key is within the ranger lower -> upper
	// if lower > upper, then it wraps around
	chord_key max = 0 - 1;

	// we don't wrap around the circle
	if (upper > lower) {
		bool part = inclusiveEnd ? key <= upper : key < upper;
        bool begin=inclusiveBegin?lower <= key:lower<key;
		return begin && part;
	} else {
		if (lower > upper) {
			// lower less than key < max
			bool begin =inclusiveBegin?lower <= key:lower<key;
			bool above = begin&& key <= max;

			bool part = inclusiveEnd ? key <= upper : key < upper;
			bool wrapped = key >= 0 && part;

			return above || wrapped;
		} else {
			// equal each other, therefore, fits in here
			return true;
		}
	}
}

void Chord::insertSuccessor(size_t index, std::shared_ptr<Node> node, bool setupOther) {
	insertNode(index, Successors, node);

	// if we should set up a connection to the other side
	if (setupOther) {
		node->insertPredecessor(NodeInfo.get(), index);
	}
}

void Chord::insertPredecessor(size_t index, std::shared_ptr<Node> node, bool setupOther) {
	insertNode(index, Predecessors, node);

	// if we should set up a connection to the other side
	if (setupOther) {
		node->insertSuccessor(NodeInfo.get(), index);
	}
}

void Chord::replaceSuccessor(size_t index, std::shared_ptr<Node> node, bool setupOther) {
	replaceNode(index, Successors, node);

	// if we should set up a connection to the other side
	if (setupOther) {
		node->replacePredecessor(NodeInfo.get(), index);
	}
}

void Chord::replacePredecessor(size_t index, std::shared_ptr<Node> node, bool setupOther) {
	replaceNode(index, Predecessors, node);

	// if we should set up a connection to the other side
	if (setupOther) {
		node->replaceSuccessor(NodeInfo.get(), index);
	}
}
