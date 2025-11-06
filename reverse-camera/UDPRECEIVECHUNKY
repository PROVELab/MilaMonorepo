import socket, struct, time
from pathlib import Path

PORT = 5005
TIMEOUT = 10      # seconds

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", PORT))

buffers = {}      # file_id = (chunk_list, first_seen, chunk_tot)

print(f"Listening on {PORT} …")
while True:
    # Receive packet max size 1500 bytes
    pkt, _ = sock.recvfrom(1500)

    # Parse header to get file_id, chunk_tot, idx
    fid, tot, idx = struct.unpack("!IHH", pkt[:8])
    
    # Extract chunk data
    chunk = pkt[8:]

    # Tries to get existing buffer or create new one if it doesn't exist
    buf, t0, _ = buffers.get(fid, ([None]*tot, time.time(), tot))
    buf[idx] = chunk # store chunk in right slot
    buffers[fid] = (buf, t0, tot) # saves updated tray back into dict

    # Check if file is complete
    if None not in buf:                               
        fname = Path(f"NEWFILE_{fid}.jpg")
        fname.write_bytes(b"".join(buf)) # concatenate all chunks in order to one file
        print(f"Wrote {fname} ({sum(len(c) for c in buf)} B)")
        buffers.pop(fid)

    # Delete buffers if file hasn't finished within TIMEOUT
    now = time.time()
    for key, (b, t0, _) in list(buffers.items()):
        if now - t0 > TIMEOUT:
            print(f"Timeout file_id {key}, discard")
            buffers.pop(key)
