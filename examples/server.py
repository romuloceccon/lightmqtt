#! /usr/bin/env python

import sys
import socket

ADDRESS = '127.0.0.1'
PORT = 4000

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind((ADDRESS, PORT))
s.listen(1)

while True:
    conn = None
    try:
        conn, addr = s.accept()
        print('Client: %s' % (conn.getpeername(),))

        data = ""
        while True:
            b = conn.recv(1)
            if not b:
                conn.close()
                break

            data += b
            if len(data) >= 20:
                print('  CONNECT')
                data = ""
                conn.send("\x20\x02\x00\x00")
            elif len(data) >= 2 and data[0] == '\xc0':
                print('  PINGREQ')
                data = ""
                conn.send("\xd0\x00")
            elif len(data) >= 2 and data[0] == '\xe0':
                print('  DISCONNECT')
                data = ""
                conn.close()
                break
    except KeyboardInterrupt:
        if conn:
            conn.close()
        s.close()
        sys.exit(0)
