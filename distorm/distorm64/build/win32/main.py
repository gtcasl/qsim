import distorm
import binascii
import random
import time

def decode16(hexcode):
	print "-----------------------Decode16Bits-----------------------"
	lines = distorm.Decode(0x100, binascii.unhexlify(hexcode), distorm.Decode16Bits)
	for i in lines:
		print "0x%08x (%02x) %-20s '%s'" % (i[0], i[1], i[3], i[2])
	print

def decode32(hexcode):
	print "-----------------------Decode32Bits-----------------------"
	lines = distorm.Decode(0x100, binascii.unhexlify(hexcode), distorm.Decode32Bits)
	for i in lines:
		print "0x%08x (%02x) %-20s '%s'" % (i[0], i[1], i[3], i[2])
	print

def decode64(hexcode):
	print "-----------------------Decode64Bits-----------------------"
	lines = distorm.Decode(0x100, binascii.unhexlify(hexcode), distorm.Decode64Bits)
	for i in lines:
		print "0x%08x (%02x) %-20s '%s'" % (i[0], i[1], i[3], i[2])
	print

def main():
	print distorm.info

	hexcode = "658d00"
	decode16(hexcode)
	decode32(hexcode)
	decode64(hexcode)

def main_old():
	print distorm.info
	hexcode = ""
	random.seed(9879877)
	print "start generating: %s" % time.asctime(time.localtime())
	for i in xrange(1024):
		hexcode += chr(random.randint(0, 255))
	print "start unpacking: %s" % time.asctime(time.localtime())
	hexcode *= (1024*5)
	print "start decoding: %s" % time.asctime(time.localtime())
	lines = distorm.Decode(0x55551, hexcode, distorm.Decode32Bits)
	print "end: %s" % time.asctime(time.localtime())
	for i in lines:
		print "0x%08x (%02x) %-20s '%s'" % (i[0], i[1], i[3], i[2])

#main_old()
main()
raw_input()
