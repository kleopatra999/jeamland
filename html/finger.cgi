#!/bin/sh

RQUERY=~jeamland/pub/rquery

echo Content-type: text/html
echo
echo '<TITLE>Finger Gateway</TITLE>'
echo '<H1>Finger Gateway</H1>'

if [ $# = 0 ]; then
	cat << EOM

<ISINDEX>

This is a gateway to "finger". Type the name of the user you wish to
finger in your browser's search dialog.<P>
EOM
else
	echo \<PRE\>
	${RQUERY} "finger $*"
fi

