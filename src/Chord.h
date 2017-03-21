
#ifndef CHORD_H_
#define CHORD_H_
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <memory>
#include <tuple>
#include "Node.h"


typedef unsigned char byte;
typedef unsigned int chord_key;

void showUsageMessage(std::string procname);

struct FingerEntry    //represent a Finger Table entry
{
	FingerEntry(chord_key _start,std::shared_ptr<Node> _finger_node):start(_start),finger_node(_finger_node){}
	chord_key start;//
	std::shared_ptr<Node> finger_node;
};

class Chord {
private:
	static std::shared_ptr<Chord> myInstance;

	const size_t MONITOR_PERIOD = 5000;

	const size_t NUM_PRED_SUCC = 2;
	const std::string WELCOME_MESSAGE = "Welcome! Who are you?\n";
	const std::string ERROR_GOODBYE_MESSAGE = "I don't know who you are. Please leave.\n";
    const size_t MAX_FTE_NUM=32;//max finger table entry number

	int myListenFD;
	int myListenPort;
	std::string myIPAddress;
	chord_key myKey;
	std::vector<FingerEntry> myFingerTable;

	void handleRequest(int socket_fd, sockaddr_in sockaddr);

	Chord();

	void insertNode(size_t index, std::vector<std::shared_ptr<Node>>& items, std::shared_ptr<Node> node);
	void replaceNode(size_t index, std::vector<std::shared_ptr<Node>>& items, std::shared_ptr<Node> node);
	std::shared_ptr<Node> searchSuccessorLocal(chord_key key);
	//void monitorLinks();

public:
	std::shared_ptr<Node> NodeInfo;
	std::vector<std::shared_ptr<Node>> Predecessors;
	std::vector<std::shared_ptr<Node>> Successors;

	static std::shared_ptr<Chord> getInstance();

	void init(int port);

	virtual ~Chord();

	void Listen();

	void JoinRing(std::string entry_ip, int entry_port);
	void LeaveRing();

	static chord_key hashKey(std::string value);
	static void parseIPPort(std::string message, std::string* ip, int* port);

	static std::string getLocalIPAddress();

	std::string toString();

	std::shared_ptr<Node> findSuccessor(chord_key key);
	std::shared_ptr<Node> findPredecessor(chord_key key);

	void insertSuccessor(size_t index, std::shared_ptr<Node> node, bool setupOther = true);
	void insertPredecessor(size_t index, std::shared_ptr<Node> node, bool setupOther = true);

	void replaceSuccessor(size_t index, std::shared_ptr<Node> node, bool setupOther = true);
	void replacePredecessor(size_t index, std::shared_ptr<Node> node, bool setupOther = true);

	void initFingerTable(Node &node);
	void stabilize();
    void fixFinger();
	/**
	 * Remove a successor that is not responding to us anymore. We don't want to talk to him anymore.
	 * We only clean up ourselves, not anybody else who may talk to it. They will clean it up themselves later on.
	 */
	void spliceSuccessor(size_t index);
	void splicePredecessor(size_t index);

	std::tuple<chord_key, chord_key> getRange();
    //inRange--------->default (] interval
	static bool inRange(chord_key lower, chord_key upper, chord_key key, bool inclusiveEnd = true,bool inclusiveBegin=false);

};

#endif /* CHORD_H_ */
