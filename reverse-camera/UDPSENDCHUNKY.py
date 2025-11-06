import socket, struct, math
from pathlib import Path

DEST_IP, DEST_PORT = "10.144.128.185", 5005
PAYLOAD = 1340                                    
FILE_PATH = Path(r"C:\Users\kolat\Downloads\Front of Tesla.jpg")

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)


file_id   = 1
blob      = FILE_PATH.read_bytes()
chunk_tot = math.ceil(len(blob) / PAYLOAD) # how many slices needed after cutting by PAYLOAD

for idx in range(chunk_tot):
    # Slice blob into parts using payload size
    part   = blob[idx*PAYLOAD : (idx+1)*PAYLOAD]

    # !I H H  (file_id, chunk_tot, idx)
    header = struct.pack("!IHH", file_id, chunk_tot, idx)
    sock.sendto(header + part, (DEST_IP, DEST_PORT))
    print(f"sent {idx+1}/{chunk_tot}")
