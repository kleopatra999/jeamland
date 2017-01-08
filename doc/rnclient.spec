
	RFC:	A protocol for JeamLand Remote Notification
	---------------------------------------------------

On startup, the rnclient MUST send the following UDP packet to the talker's
RN port (normal login port - 4):

  ### <version> : <id> : <user identifier> : <port> : <status> : <free text>

Where:
	<version> is the version number of the protocol the client supports.
	<id> is an integer which will be included in all future incoming
	    UDP messages from the talker - can be used as a form of
	    authentication.
	<user identifier> is a string which identifies the local user
	    (e.g. the user's login name); it may be a string such as 'hidden'.
	<port> is the port number on which the client will accept incoming
	    UDP messages from the talker.
	<free text> is any identifier the client wants to give to the server.
	    It is intended that this be used to transmit the client version
	    number although it could be a copyright message etc.
	    (Maximum length is 22 characters)

And <status> is:
	0 for client shutdown.
	1 for client startup.
	2 for client resync request (equivalent to a shutdown, startup).

Currently, the protocol version is 15

One of these packets must reach the talker every 15 minutes or the client will
be removed from the broadcast list. Due to the unreliability of UDP, I would
recommend that one of these packets be sent every 4 minutes or so then if by
chance one does not get through, the next one will still be within the 15
minute time period.

NB: The client must NOT send more than one of these packets in any two minute
    period or it will be instructed to close down by the talker.

On shutdown, the client MUST send the same packet but with a 0 in place of
the 1. The client MUST NOT send more than one shutdown packet.

***

The client should expect to receive messages of the following form:

All incoming packets will start with

	### <talker name> : <packet id> : <id> :

where <id> is the id integer which was sent to the talker in the most recent
ping packet and <packet id> is a unique packet id allocated by the talker.
The <id> field can be used for authentication purposes to ensure that the
received packet has really originated from the talker.

NB:	Once a client has received a packet with a particular <packet id>,
	it MUST disregard any further packets received with the same
	<packet id>. The talker may choose to send any packet several times
	to better ensure that at least one packet reaches its target.
	For this purpose, it is recommended that the client keeps a history
	of the last 50 packet id's received from each talker.

<packet id> and <id> are both 32-bit integers.

For users logging in and out of the talker, these packets will look like:

	### <talker name> : <packet id> : <id> : <user name> : <code>

Where <code> is a string prefixed with a + or - character. The currently
defined codes are as follows:

	Code			Meaning
	----			-------
	+startup		Already logged on (used at startup and after
						   resync requests.)
	-login			Logged off.
	+login			Logged on.
	+lostconn		Lost connection.
	+afk			Went AFK.
	-afk			Returned from AFK.

A client should be able to handle unknown codes although it may choose to
ignore them - new codes may be added at any time.

NB: If <code> does not start with a + or - character, or if <code> contains a :
    character, then the client should assume that this is in fact an arbitrary
    message as documented below.

The talker may also send messages and commands to the rnclient which will
be constructed as follows:

The general packet is:

	### <talker name> : <packet id> : <id> : <message>

If <message> begins with a $ character, then this is an informational message
and no action is required.

If <message> begins with a ! character, then this is an alert message, the
client should highlight it in some way.

If <message> does not begin with one of the above characters, then it is a
command. The commands which the server currently sends are:

    die		- The talker no longer wishes to talk to the client and no
		  more ping packets should be sent. In this case, the client
		  MUST NOT send a shutdown ping.
    shutdown	- The talker is shutting down, the client may choose to exit
		  or to try and re-establish contact after a short time.

All packets are terminated by a newline.

