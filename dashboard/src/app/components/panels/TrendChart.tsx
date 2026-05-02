"use client";

import { useMemo, useRef } from "react";
import {
  Chart as ChartJS,
  LinearScale,
  PointElement,
  LineElement,
  Tooltip as ChartTooltip,
  Legend as ChartLegend,
  ScatterController,
  LineController
} from 'chart.js';
import { Chart } from 'react-chartjs-2';
import zoomPlugin from 'chartjs-plugin-zoom';
import type { FieldTrendResponse } from "../../types/telemetry";

ChartJS.register(
  LinearScale,
  PointElement,
  LineElement,
  ChartTooltip,
  ChartLegend,
  ScatterController,
  LineController,
  zoomPlugin
);

function formatMinutes(minutes: number): string {
  if (Math.abs(minutes) < 0.001) return "Now";
  if (minutes > 0) return `+${Math.round(minutes)}m`;
  return `${Math.round(minutes)}m`;
}

export default function TrendChart({ trend }: { trend: FieldTrendResponse }) {
  const chartRef = useRef<ChartJS>(null);

  const chartData = useMemo(() => {
    return {
      datasets: [
        {
          type: 'scatter' as const,
          label: 'Raw Sample',
          data: trend.rawPoints.map((p: any) => ({ x: p.minutesFromNow, y: p.value })),
          backgroundColor: '#f2a65a',
          pointRadius: 4,
          pointHoverRadius: 6,
        },
        {
          type: 'line' as const,
          label: 'Linear Trend',
          data: trend.fitPoints.map((p: any) => ({ x: p.minutesFromNow, y: p.value })),
          borderColor: '#8bd8ca',
          borderWidth: 2,
          pointRadius: 0,
          pointHoverRadius: 0,
          fill: false,
        }
      ]
    };
  }, [trend]);

  const options = useMemo(() => ({
    responsive: true,
    maintainAspectRatio: false,
    interaction: {
      mode: 'nearest' as const,
      intersect: false,
    },
    plugins: {
      legend: {
        labels: {
          color: 'rgba(244, 242, 234, 0.62)'
        }
      },
      tooltip: {
        callbacks: {
          title: (items: any[]) => {
            if (!items.length) return '';
            return formatMinutes(items[0].parsed.x);
          },
          label: (item: any) => {
            return `${item.dataset.label}: ${item.parsed.y.toFixed(2)}`;
          }
        }
      },
      zoom: {
        pan: {
          enabled: true,
          mode: 'x' as const,
        },
        zoom: {
          wheel: {
            enabled: true,
          },
          pinch: {
            enabled: true
          },
          mode: 'x' as const,
        }
      }
    },
    scales: {
      x: {
        type: 'linear' as const,
        grid: {
          color: 'rgba(255,255,255,0.08)'
        },
        ticks: {
          color: 'rgba(255,255,255,0.68)',
          callback: (value: number | string) => formatMinutes(Number(value))
        }
      },
      y: {
        grid: {
          color: 'rgba(255,255,255,0.08)'
        },
        ticks: {
          color: 'rgba(255,255,255,0.68)'
        }
      }
    }
  }), []);

  if (!trend.rawPoints.length && !trend.fitPoints.length) {
    return <div className="trend-modal__empty">No points available</div>;
  }

  const handleResetZoom = () => {
    if (chartRef.current) {
      chartRef.current.resetZoom();
    }
  };

  return (
    <div className="trend-chart" style={{ position: "relative" }}>
      <button
        onClick={handleResetZoom}
        style={{
          position: "absolute",
          top: 0,
          right: 0,
          zIndex: 10,
          background: "transparent",
          border: "1px solid var(--line-strong)",
          color: "var(--foreground)",
          borderRadius: "4px",
          padding: "2px 8px",
          cursor: "pointer",
          fontSize: "12px",
        }}
      >
        Reset Zoom
      </button>
      <Chart ref={chartRef as any} type="scatter" data={chartData} options={options} />
    </div>
  );
}
