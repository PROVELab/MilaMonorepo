# Dashboard Backend

This backend is the Tauri side of the in-car dashboard.

It has two serial responsibilities:

- Read framed `VehicleStatusRegister` protobuf frames from the MCU.
- Write framed `DriveMode` protobuf frames back to the MCU.

The frame format intentionally matches the MCU UART code:

- byte 0: `0xA5`
- byte 1: `0x5A`
- bytes 2-3: little-endian payload length
- remaining bytes: protobuf payload

The code is split by responsibility:

- `lib.rs` owns Tauri command registration only.
- `state.rs` owns shared dashboard state and command entry points.
- `serial_link.rs` owns UART discovery, framing, reads, writes, and reconnects.
- `vsr_proto.rs` owns descriptor lookup, protobuf decode, command encode, and dynamic field formatting.
- `snapshot.rs` turns the latest decoded VSR into frontend-facing JSON.
- `field_trend.rs` builds linear history/trend data for a selected numeric VSR field.
- `trend_samples.rs` reads historical numeric samples from the active MCAP recording.
- `mcap_recorder.rs` records the raw inbound VSR payload stream.

Normal command flow:

1. Frontend invokes `send_motor_command`.
2. `state.rs` validates that serial is currently ready.
3. The command is handed to the serial worker through an in-process channel.
4. `serial_link.rs` protobuf-encodes, frames, and writes it.
5. The MCU mirrors accepted mode in the outbound VSR stream.
6. The frontend updates only from that VSR stream.

Commands are dropped, not retained, while serial is down.

On serial disconnect, the backend clears the ready flag, drops any pending outbound commands, and starts discovery again. That is the only runtime recovery path intended here.

Avoid adding simulated dashboard values in this layer. Values shown to the driver should come from decoded VSR fields or be absent.
