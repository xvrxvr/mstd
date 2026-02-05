import sys

def load_segment_1(fstream):
    bt = fstream.read(1)
    while True:
        if not bt:
            return
        if bt[0] == 0:
            # Read zero string
            length = 1
            while True:
                bt = fstream.read(1)
                if not bt:
                    yield 0, length
                    return
                if bt[0] != 0:
                    yield 0, length
                    break
                length += 1
        else:
            result = bt[:]
            while True:
                bt = fstream.read(1)
                if not bt:
                    yield 1, result
                    return
                if bt[0] == 0:
                    yield 1, result
                    break
                result += bt

def load_segment_2(fstream):
    pending = b''
    for tp, data in load_segment_1(fstream):
        if tp == 1:
            pending += data
        elif data <= 3 and pending: # Append zeros, because generate segment for less than 3 zero is not effective
            pending += data * b'\0'
        else:
            if pending:
                yield 1, pending
                pending = b''
            yield 0, data
    if pending:
        yield 1, pending


def pack(fstream_in, fstream_out):
    last_type = 0
    for tp, data in load_segment_2(fstream_in):
        assert last_type != tp
        last_type = tp
        length = len(data) if tp else data
        if length >= 255:
            fstream_out.write(b'\xFF')
            fstream_out.write(length.to_bytes(2, byteorder='little'))
        else:
            fstream_out.write(length.to_bytes(1, byteorder='little'))
        if tp:
            fstream_out.write(data)


def unpack(fstream_in, fstream_out):
    cur_type = 1
    while True:
        b = fstream_in.read(1)
        if not b:
            return
        if b[0] == 255:
            b = fstream_in.read(2)
            length = int.from_bytes(b, byteorder='little')
        else:
            length = b[0]
        if cur_type:
            b = fstream_in.read(length)
            fstream_out.write(b)
        else:
            fstream_out.write(length * b'\0')
        cur_type = 1-cur_type


if len(sys.argv) > 1 and sys.argv[1] == '-u':
    do_unpack = True
    del sys.argv[1]
else:
    do_unpack = False

if len(sys.argv) < 3:
    print("""FPGA bit file packer. 
Usage: bitpk.py [-u] <in-file> <out-file>
    -u - Unpack <in-file> to <out-file>
         Otherwise pack <in-file> to <out-file>""")
else:
    with open(sys.argv[1], 'rb') as src, open(sys.argv[2], 'wb') as dst:
        if do_unpack:
            unpack(src, dst)
        else:
            pack(src, dst)
