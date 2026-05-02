use enumset::enum_set;
use std::fs;
use std::time::{SystemTime, UNIX_EPOCH};

use crate::field_trend::{FieldSample, SampleSource};
use crate::vsr_proto::{decode_vehicle_status, dynamic_field_as_f64};

const MCAP_VSR_TOPIC: &str = "/vsr";

pub fn collect_field_samples_from_mcap(
    mcap_path: &str,
    section_id: &str,
    field_key: &str,
) -> Result<Vec<FieldSample>, String> {
    let file_bytes =
        fs::read(mcap_path).map_err(|err| format!("failed reading {mcap_path}: {err}"))?;
    let stream = mcap::MessageStream::new_with_options(
        file_bytes.as_slice(),
        enum_set!(mcap::read::Options::IgnoreEndMagic),
    )
    .map_err(|err| format!("failed opening mcap stream {mcap_path}: {err}"))?;

    let mut samples = Vec::new();
    for message in stream {
        let message = match message {
            Ok(message) => message,
            Err(_) => break,
        };
        if message.channel.topic != MCAP_VSR_TOPIC {
            continue;
        }

        let Some(vsr) = decode_vehicle_status(message.data.as_ref())? else {
            continue;
        };
        let Some(value) = dynamic_field_as_f64(&vsr, section_id, field_key) else {
            continue;
        };

        samples.push(FieldSample {
            timestamp_ns: message.log_time,
            value,
            source: SampleSource::Mcap,
        });
    }

    Ok(samples)
}

pub fn epoch_nanos() -> u64 {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    u64::try_from(nanos).unwrap_or(u64::MAX)
}
