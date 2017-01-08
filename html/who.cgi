#!/bin/sh

RQUERY=~jeamland/pub/rquery

echo Content-type: text/html
echo
echo '<TITLE>Who Gateway</TITLE>'
echo '<H1>Who Gateway</H1>'

echo \<PRE\>
${RQUERY} 'who'

