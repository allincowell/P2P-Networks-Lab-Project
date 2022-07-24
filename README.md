This is a peer-to-peer chat application.

A peer-to-peer (P2P) application is an application primitive, where there are no dedicated central server nodes. Every node in the network can connect with each other and transfer data among themselves.

In this P2P chat application, we consider a close group of friends who want to chat with each other. The chat application uses TCP as the underlying transport layer protocol. For P2P implementation, every instance of the chat application runs a TCP server (we call this as peer-chat server) where the chat application listens for incoming connections. The protocol details are as follows. The same process of the application also runs one or multiple TCP clients (based on the number of peers) to connect to the other users and transfer messages.

Details with functionalities and design diagram can be found in "Design.pdf".
Implementation details can be found in "Implementation.pdf".

The steps to run the P2P Chat application:

1. To compile use the make command in the same directory as the C++ file. A makefile has been provided.

2. To run the executable use ./P2P command on each peer.

3. Edit the file peer_details.txt based on your peers and corresponding IP addresses.

4. A copy of this process and file must be present on each peer, who needs to run it separately.