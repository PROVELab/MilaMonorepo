use serde::Serialize;

const PLOT_MAX_POINTS: usize = 400;
const FIT_MAX_POINTS: usize = 2_000;
const RETENTION_MAX_POINTS: usize = 8_000;
const FIT_LINE_POINTS: usize = 220;
const FUTURE_HORIZON_MINUTES: [u32; 3] = [5, 15, 30];

#[derive(Clone, Copy)]
pub enum SampleSource {
    Mcap,
    Live,
}

impl SampleSource {
    fn as_str(self) -> &'static str {
        match self {
            Self::Mcap => "mcap",
            Self::Live => "live",
        }
    }
}

#[derive(Clone, Copy)]
pub struct FieldSample {
    pub timestamp_ns: u64,
    pub value: f64,
    pub source: SampleSource,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct FieldTrendPoint {
    pub minutes_from_now: f64,
    pub value: f64,
    pub source: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct FieldPrediction {
    pub horizon_minutes: u32,
    pub value: f64,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct FieldTrendResponse {
    pub section_id: String,
    pub field_key: String,
    pub label: String,
    pub unit: Option<String>,
    pub fit_degree: usize,
    pub raw_points: Vec<FieldTrendPoint>,
    pub fit_points: Vec<FieldTrendPoint>,
    pub predictions: Vec<FieldPrediction>,
}

#[derive(Clone, Copy)]
struct RelativeSample {
    minutes_from_now: f64,
    value: f64,
    source: SampleSource,
}

pub fn build_field_trend(
    section_id: String,
    field_key: String,
    label: String,
    unit: Option<String>,
    mut samples: Vec<FieldSample>,
) -> Result<FieldTrendResponse, String> {
    if samples.is_empty() {
        return Err(format!(
            "no numeric samples available for {section_id}.{field_key}"
        ));
    }

    samples.sort_by_key(|sample| sample.timestamp_ns);
    if samples.len() > RETENTION_MAX_POINTS {
        let start = samples.len() - RETENTION_MAX_POINTS;
        samples.drain(..start);
    }

    let now_ns = samples
        .last()
        .map(|sample| sample.timestamp_ns)
        .ok_or_else(|| format!("no numeric samples available for {section_id}.{field_key}"))?;

    let relative = samples
        .iter()
        .map(|sample| RelativeSample {
            minutes_from_now: (sample.timestamp_ns as f64 - now_ns as f64) / 60_000_000_000.0,
            value: sample.value,
            source: sample.source,
        })
        .collect::<Vec<_>>();

    let fit_input = if relative.len() > FIT_MAX_POINTS {
        relative[relative.len() - FIT_MAX_POINTS..].to_vec()
    } else {
        relative.clone()
    };

    let fit = fit_linear(
        &fit_input
            .iter()
            .map(|sample| (sample.minutes_from_now, sample.value))
            .collect::<Vec<_>>(),
    )
    .ok_or_else(|| format!("linear trend fit failed for {section_id}.{field_key}"))?;

    let raw_points = downsample_relative(&relative, PLOT_MAX_POINTS)
        .into_iter()
        .map(|sample| FieldTrendPoint {
            minutes_from_now: sample.minutes_from_now,
            value: sample.value,
            source: sample.source.as_str().to_string(),
        })
        .collect::<Vec<_>>();

    let min_x = relative
        .first()
        .map(|sample| sample.minutes_from_now)
        .unwrap_or(-1.0)
        .min(-1.0);
    let max_x = FUTURE_HORIZON_MINUTES
        .last()
        .copied()
        .map(f64::from)
        .unwrap_or(30.0);

    let fit_points = evenly_spaced(min_x, max_x, FIT_LINE_POINTS)
        .into_iter()
        .map(|minutes_from_now| FieldTrendPoint {
            minutes_from_now,
            value: eval_linear(fit, minutes_from_now),
            source: "fit".to_string(),
        })
        .collect::<Vec<_>>();

    let predictions = FUTURE_HORIZON_MINUTES
        .iter()
        .copied()
        .map(|horizon_minutes| FieldPrediction {
            horizon_minutes,
            value: eval_linear(fit, f64::from(horizon_minutes)),
        })
        .collect::<Vec<_>>();

    Ok(FieldTrendResponse {
        section_id,
        field_key,
        label,
        unit,
        fit_degree: 1,
        raw_points,
        fit_points,
        predictions,
    })
}

fn fit_linear(points: &[(f64, f64)]) -> Option<(f64, f64)> {
    if points.is_empty() {
        return None;
    }
    if points.len() == 1 {
        return Some((points[0].1, 0.0));
    }

    let n = points.len() as f64;
    let sum_x = points.iter().map(|(x, _)| *x).sum::<f64>();
    let sum_y = points.iter().map(|(_, y)| *y).sum::<f64>();
    let sum_xx = points.iter().map(|(x, _)| x * x).sum::<f64>();
    let sum_xy = points.iter().map(|(x, y)| x * y).sum::<f64>();
    let denom = n * sum_xx - sum_x * sum_x;
    if denom.abs() < 1e-12 {
        return Some((sum_y / n, 0.0));
    }

    let slope = (n * sum_xy - sum_x * sum_y) / denom;
    let intercept = (sum_y - slope * sum_x) / n;
    Some((intercept, slope))
}

fn eval_linear((intercept, slope): (f64, f64), x: f64) -> f64 {
    intercept + slope * x
}

fn downsample_relative(samples: &[RelativeSample], max_points: usize) -> Vec<RelativeSample> {
    if samples.len() <= max_points || max_points == 0 {
        return samples.to_vec();
    }

    let step = (samples.len() as f64 - 1.0) / (max_points as f64 - 1.0);
    let mut out = Vec::with_capacity(max_points);
    let mut last_idx = usize::MAX;

    for i in 0..max_points {
        let mut idx = (i as f64 * step).round() as usize;
        if idx >= samples.len() {
            idx = samples.len() - 1;
        }
        if idx != last_idx {
            out.push(samples[idx]);
            last_idx = idx;
        }
    }

    out
}

fn evenly_spaced(start: f64, end: f64, n: usize) -> Vec<f64> {
    if n <= 1 {
        return vec![end];
    }

    let step = (end - start) / (n - 1) as f64;
    (0..n).map(|i| start + i as f64 * step).collect()
}
