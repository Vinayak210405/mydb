import socket, struct

def send_query(s, sql):
    msg = sql.encode()
    s.send(bytes([0x01]) + struct.pack('>I', len(msg)) + msg)
    t = s.recv(1)[0]
    length = struct.unpack('>I', s.recv(4))[0]
    response = s.recv(length).decode() if length > 0 else ''
    types = {2: 'RESULT', 3: 'OK', 4: 'ERROR'}
    print('[' + types.get(t,'?') + '] ' + sql[:45])
    if response: print(response)

s = socket.create_connection(('localhost', 5433))
send_query(s, "CREATE TABLE users (id INT, name VARCHAR, age INT)")
send_query(s, "INSERT INTO users VALUES (1, 'Alice', 30)")
send_query(s, "INSERT INTO users VALUES (2, 'Bob', 25)")
send_query(s, "INSERT INTO users VALUES (3, 'Charlie', 35)")
send_query(s, "CREATE INDEX ON users (id)")
send_query(s, "SELECT * FROM users WHERE id = 2")
send_query(s, "SELECT * FROM users WHERE id = 3")
s.close()
