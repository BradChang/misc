#!/bin/bash

#set -o errexit

SIZE="stat -c %s"
R=ring.$$
i=1
TOOL=../shrtool

if [ ! -f ${TOOL} ]; then echo "please make ${TOOL} first"; exit -1; fi

# should fail, incorrect options
${TOOL} -c 2>/dev/null
if [ $? -ne 0 ]; then echo passed; else echo failed; fi

# should fail, too small (-s 0 implied) 
RING=$R.$i; let i++
${TOOL} -c $RING 2>/dev/null
if [ \( $? -ne 0 \) ]; then echo passed; else echo failed; fi

# should succeed
RING=$R.$i; let i++
${TOOL} -c $RING -s 1k 2>/dev/null
if [ \( $? -eq 0 \) -a \( -f $RING \) ]; then echo passed; else echo failed; fi

# should fail, no filename
RING=$R.$i; let i++
${TOOL} -s 1k 2>/dev/null
if [ $? -ne 0 ]; then echo passed; else echo failed; fi

# should fail, size flag is only valid in create mode
RING=$R.$i; let i++
${TOOL} -s 1k $RING 2>/dev/null
if [ $? -eq 0 ]; then echo "failed"; else echo "passed"; fi

# should succeed, created new
RING=$R.$i; let i++
${TOOL} -s 1k -c $RING 2>/dev/null
if [ $? -eq 0 ]; then echo "passed"; else echo "failed"; fi

# should fail, flag mode "on" invalid 
RING=$R.$i; let i++
${TOOL} -s 1k -c -f on $RING 2>/dev/null
if [ $? -eq 0 ]; then echo "failed"; else echo "passed"; fi

# should succeed (k flag)
RING=$R.$i; let i++
${TOOL} -s 1k -c -f k $RING 2>/dev/null
if [ $? -eq 0 ]; then echo "passed"; else echo "failed"; fi

# should succeed (o flag)
RING=$R.$i; let i++
${TOOL} -s 1k -c -f o $RING 2>/dev/null
if [ $? -eq 0 ]; then echo "passed"; else echo "failed"; fi

# should succeed (l flag)
RING=$R.$i; let i++
${TOOL} -s 1k -c -f l $RING 2>/dev/null
if [ $? -eq 0 ]; then echo "passed"; else echo "failed"; fi

# should succeed (m flag)
RING=$R.$i; let i++
${TOOL} -s 1k -c -f m $RING 2>/dev/null
if [ $? -eq 0 ]; then echo "passed"; else echo "failed"; fi

# should succeed (olm flags)
RING=$R.$i; let i++
${TOOL} -s 1k -c -f olm $RING 2>/dev/null
if [ $? -eq 0 ]; then echo "passed"; else echo "failed"; fi

# should succeed (klm flags)
RING=$R.$i; let i++
${TOOL} -s 1k -c -f klm $RING 2>/dev/null
if [ $? -eq 0 ]; then echo "passed"; else echo "failed"; fi

# should fail (ko are incompatible flags)
RING=$R.$i; let i++
${TOOL} -s 1k -c -f ko $RING 2>/dev/null
if [ $? -ne 0 ]; then echo "passed"; else echo "failed"; fi

# cleanup temporary files
rm $R.*;

