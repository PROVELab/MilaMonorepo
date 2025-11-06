# Reverse Camera
This part of monorepo contains code that runs
on a raspberry pi near the rear of the vehicle which:
- Records data from a rear-facing camera
- Chunks it and sends over Ethernet (via UDP)

Furthermore there are additional scripts here that will
reconstruct it (as a demonstration of the protocol itself), though
in the actual Mila reconstruction is done by the dashboard code

