#include <iostream>
#include <thread>
#include <string>

#include <SFML/Network.hpp>

#include "Const.h"

#include "Client.h"

int main(int argc, char *argv[]) {
	int id;
	if (argc < 2) {
		id = 0;
	} else {
		id = atoi(argv[1]);
	}

	ConsistencyMode c = Push;
	if (argc >= 3) {
		std::string arg2(argv[2]);
		if (arg2 == "push") {
			c = Push;
		} else if (arg2 == "pull") {
			c = Pull;
		} else {
			std::cout << "Use push or pull for the second argument\n";
			exit(1);
		}
	}


	Client client(id, c);
	client.
			init();

	client.
			go();

}