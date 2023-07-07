Copyright: Dobrica Nicoleta-Adriana 321CAa

# TCP & UDP Client-server application

## Description:

- This homework implements a client-server application with TCP and UDP clients.
The server gets messages from UDP clients, and forwards these messages to the TCP
clients that have subscribed to certain topics.

- The TCP clients have two types of messages they can send to the server: they can either subscribe to a certain topic, or unsubscribe from it. They also have an option called store-and-forward, which can be either 0 or 1. If it's 1, then it is the server's job to forward to the client topics they have subscribed to and missed when they were disconnected.

- The UDP clients only send messages in a specific format: first we have the topic, the type, and the content.


## Implementation:

- I have created different structures to simplify the sending/receiving part of messages from both types of clients.

1. **Package structure**: when a TCP client wants to send a message to the server that they want to subscribe/unsubscribe; this structure contains a string and its length.

2. **Message structure**: I used `attribute packed` for this structure to avoid padding. This structure contains a string for topic, content and an uint8_t for the type. This is used for certain purposes: firstly, when the server receives a message from the UDP client. The server parses this message, and forwards it to a TCP client if they are subscribed to one of the topics the server received from the UDP client.

3. **MessageUDP structure**: this structure contains an array of type Message, and its size; used for store-and-forward functionality in order to store any received messages from UDP clients to forward them when a TCP client reconnects.

4. **client structure**: this one records any important information about a TCP client. The server keeps an array of this structure that it manipulates whenever a new client has connected to the server, when they disconnected or when sending messages. This structure memorises the client id, an array of strings for subscribed topics, the number of subscribed topics for the array of strings, the client file descriptor, 
a "semaphore"-like variable that memorises whether or not the client is currently
connected, and an array of string for the subscribed topics with SF = 1, along
with the number of SF subscribed topics.

5. **Other important functions used by both server and subscriber**:

	-> a handler for errors, which prints to stderr and exits;

	-> recv_all/send_all: these functions send/receive exactly a fixed amount
	of bytes


## Server:

- When the server starts, it first creates the sockets for the TCP and UDP 
connections, binding the server address to these sockets and also deactivates
Nagle's algorithm and makes the sockets reusable. 

- When the server is first run, it starts with three default clients in the
pollfd array used for multiplexing: the file descriptors for the TCP, UDP and
STDIN. The server iterates through these clients and responds accordingly:

	1. For the TCP client, the server accepts the new connection and adds the
	client's file descriptor to the array of polls;

	2. For the UDP client, the server receives the information and parses it;
	then the server iterates through the array of TCP clients, checks if the new
	message it receives has a topic that one of the clients, with SF = 1, has
	missed out on while being disconnected; if it did, then the server adds the
	new message into the MessageUDP array to forward it later when the client
	reconnects. Then, the server sends the received messages to the connected
	clients that have subscribed with the corresponding topic of the received
	message.

	3. For STDIN, the server parses a buffer and checks whether the buffer
	contains the "exit" command; if we have an exit command, then the server
	closes all the file descriptors and exits. Otherwise, we have an invalid
	command and a message gets printed in stderr.

	4. Otherwise, we have a message received from one of the connected TCP
	clients; if the client has just connected, then it means we have received
	its ID. The server checks then if the ID already exists in the array of
	clients. If it does, then a client with the same ID is already connected,
	and the server prints an error message. Otherwise, we have a new client, 
	so it gets added to the array of clients and the connected flag is set
	to 1. The server also sends any topics with SF = 1 that the client has
	missed out on if they have any subscribed topics. 
	
- If it's not a new connection, then it means that the client has sent
	a packet to the server. If the number of bytes returned is actually
	0, then it means that the client has disconnected, so the server 
	closes its file descriptor and removes the client from both the
	client array and the poll fd array. Otherwise, the server parses
	through the received packet and checks the message: if the client
	wants to subscribe to a certain topic, the server memorises the topic
	and its SF; if the client wants to unsubscribe from a topic, then the
	server removes the said topic from the client's respective array.


## Subscriber:

- After the subscriber has connected to the server, it also sends its ID. The
subscriber only receives from STDIN and the server.

- If the server has sent a message to the subscriber, then it parses through
it according to its type, and prints the message to stdout. The content can
either be a string, a float, a short real or an integer. 

- Otherwise, the subscriber reads from stdin and parses the buffer. If the
buffer contains "subscribe" or "unsubscribe" messages, then the subscriber
sends these to the server, otherwise an error message is printed to stderr.


