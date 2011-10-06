#!/bin/bash

#
# instpython.sh, Install .so into Python Library
#

#
# Check for libdistorm64.so in given Directory
#

if [ ! -f libdistorm64.so ]; then
	echo "*** Error: Can't find libdistorm64.so!"
	exit 0
fi

#
# Lookup 'python'
#

PYTHONDIRS=`whereis python | awk -F ": " '{ print $2 }'`

if [ -z "$PYTHONDIRS" ]; then
	echo "*** Error: Can't find Python at all!"
	exit 0
fi


#
# Go through result, looking for 'lib'
#

FOUND=0

for PDIR in $PYTHONDIRS; do

	echo $PDIR | grep "lib" > /dev/null

	#
	# Found lib Directory!
	#

	if [ $? == 0 ]; then

		#
		# Check if it include 'lib-dynload/' sub-directory
		#

		if [ -d $PDIR/lib-dynload/ ]; then
			FOUND=1
			break
		fi
	fi

done	

#
# Found it?
#

if [ $FOUND == 0 ]; then
	echo "*** Error: Can't find Python 'lib' or 'lib/lib-dynload/' directory!"
	exit 0
fi

#
# Copy it
#

cp libdistorm64.so $PDIR/lib-dynload/distorm.so 2> /dev/null

#
# Everything went well?
#

if [ $? == 1 ]; then
	echo "*** Error: Unable to copy libdistorm64.so to $PDIR/lib-dynload, Permission denied?"
	exit 0
fi

#
# Done.
#

echo "* Done!"
exit 1
