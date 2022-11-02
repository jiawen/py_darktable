import struct


def float_from_hex(s):
    return struct.unpack('f', bytes.fromhex(s))


def int_from_hex(s):
    return struct.unpack('i', bytes.fromhex(s))
