#ifndef P2P_SHARE_CONST_H
#define P2P_SHARE_CONST_H

/* define message types for packet header, some might not be used
 * this is how we'll know kind of data each packet contains */
const sf::Int32 NULL_MESSAGE = 0;

const sf::Int32 CONNECT_AS_NEIGHBOR = 1;
const sf::Int32 NOTIFY_PEER_DISCONNECT = 2;
const sf::Int32 QUERY_FILE_LOCATION = 3;                 //query
const sf::Int32 GIVE_FILE_LOCATION = 4;                 //queryhit
const sf::Int32 REQUEST_FILE = 5;                    //obtain
const sf::Int32 NOTIFY_STARTING_TRANSFER = 6;
const sf::Int32 GIVE_FILE_PORTION = 7;
const sf::Int32 TEST_QUERY = 8;
const sf::Int32 TEST_RESPONSE = 9;
const sf::Int32 INVALIDATE = 10;
const sf::Int32 QUERY_VALID = 11;
const sf::Int32 RESPONSE_VALID = 12;


#endif //P2P_SHARE_CONST_H
