# MCU Documentation

## Building MCU firmware

Building the MCU is unfortunately a little more complex
than some of the other targets.

A normal esp32 build needs to just be built via `pio run -e mcu` 
to build it. However the MCU also needs: the vehicle state register
(VSR) generated as well as the private PROVE submodule for the h300
code (The Motor Controller interface code). 

First, get the submodule synced up via 
```bash
git submodule sync --recursive && git submodule  update --init --recursive
```

(run it twice for funsies; I've aliased this in my .gitconfig)

If you don't have permission to do this, ask an admin of the repo.

Then you can build the firmware via:

```bash
just build_embedded mcu
```

Note: this runs the autogenerate logic as well. For that, you need
rust, protoc, nanopb, etc. You can install these without much difficulty
or follow the nix tutorial to get them easily!

### Flashing the MCU firmware

You can further flash the MCU firmware by running:
```bash
just flash_embedded mcu
```

## SITL 
- Run the dashboard without an ESP!
TODO

#### VSR Autogeneration

The core of the MCU is the Vehicle State Register. It is a 
hierarchical struct looking like:
```
/
  /accel_pedaltimestamp
  /accel_pedalmutex
  /accel_pedal/
    /raw_value
    /percentage
    ...
```

There are read/write helpers to automatically acquire/set 
the mutex/timestamp for certain substructs witihn the VSR.

Check vsr.h or parts of the VSR code for examples!

##### Benefits of VSR
All vehicle (or at least MCU) state is in one place: The VSR!
The VSR gets streamed at 10 Hz to the Dashboard (where it is 
logged to an .mcap file). At any point in time we can check it
and learn the exact data from which the code is operating on.

What we see logged at a certain timepoint (give or take) is 
the exact state of the vehicle at all time.


##### MCU Linux Dependencies
 
- [Rust](https://doc.rust-lang.org/cargo/getting-started/installation.html)
- [Just](https://github.com/casey/just)
- ProtoC (Install from your package manager)
- NanoPB (Embedded protobuf) - Package manager, pip install (flaky), etc.
I added to deps so it may just work out of the box for you nw.

All of these should be in the Nix Devshell if you want a simple easy
way of just getting them all immediately.

