## SoF2Plus Multiprotocol

This project aims to provide multiprotocol support to SoF2Plus engine.

Support will base on retail versions of the game - namely SoF2 Silver (1.00, protocol 2002; legacy protocol) and SoF2 Gold (1.03, protocol 2004).
Engine differentiates between both protocol players by having 2 simultaneous sockets open - one hosting the game on 2002 protocol, another on 2004 protocol. 

Game module will have minimal changes to support multiprotocol, as most of the changes will be done on the message transfer side itself. So most changes revolve around msg.c file, where the 2004 protocol is mapped to 2002 protocol for legacy clients and the ucmd back from the client will be translated back to the 2004 protocol.
Thanks to supporting both games, some features which came with protocol 2004 will have to be dropped - that includes +use functionality (used in DEM gametype) and new guns (Silver Talon, MP5).
This is done to avoid downloading additional cgame modules for the clients, ensuring that playing the game requires no changes on the client side.

Currently, legacy clients seem to be able to parse most of the information relayed onto them, main challenge is getting the weapon information corrected - at the moment when the weapon fields are different between the 2 protocols (which happens after M1911A1), then there seem to be issues on the legacy side on handling these guns (both ammo wise and also usage wise).

After finalizing the engine side, full focus will be put onto the game side, as SoF2Plus engine is not compatible at all with QVM's and the syscalls are also different between the original engine vs this.
