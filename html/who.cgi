#!/bin/sh

RQUERY=~jeamland/pub/rquery
if [ ! -x $RQUERY ]; then
	RQUERY=rquery
fi

echo Content-type: text/html
echo
echo '<TITLE>Who Gateway</TITLE>'
echo '<H1>Who Gateway</H1>'

echo \<PRE\>
${RQUERY} 'who'

