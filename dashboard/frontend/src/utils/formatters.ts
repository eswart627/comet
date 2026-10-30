import type { MemoryBlock } from "../types";

export function formatBytes(bytes = 0, precision = 1): string {
  if (!Number.isFinite(bytes) || bytes < 0) return "0 B";
  if (bytes < 1024) return `${Math.round(bytes)} B`;

  const units = ["KB", "MB", "GB", "TB"];
  let value = bytes / 1024;
  let index = 0;

  while (value >= 1024 && index < units.length - 1) {
    value /= 1024;
    index++;
  }

  return `${value.toFixed(value >= 100 ? 0 : precision)} ${units[index]}`;
}

export function formatNumber(num = 0): string {
  if (!Number.isFinite(num)) return "0";
  return num.toLocaleString();
}

export function formatHex(value: number): string {
  return "0x" + Math.max(0, Math.floor(value)).toString(16).toUpperCase().padStart(6, "0");
}

export function formatRate(ops = 0): string {
  if (ops >= 1_000_000) return `${(ops / 1_000_000).toFixed(2)}M ops/s`;
  if (ops >= 1_000) return `${(ops / 1_000).toFixed(1)}k ops/s`;
  return `${Math.round(ops)} ops/s`;
}

export function formatTime(value?: number | string): string {
  if (value === undefined || value === null) return "--:--:--";

  const date =
    typeof value === "number"
      ? new Date(value > 1e12 ? value : value * 1000)
      : new Date(value);

  if (Number.isNaN(date.getTime())) return String(value);

  return date.toLocaleTimeString([], {
    hour12: false,
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

export function formatRelativeTime(timestampMs: number): string {
  const diffSec = Math.max(0, Math.floor((Date.now() - timestampMs) / 1000));
  if (diffSec < 2) return "just now";
  if (diffSec < 60) return `${diffSec}s ago`;
  const diffMin = Math.floor(diffSec / 60);
  if (diffMin < 60) return `${diffMin}m ago`;
  return `${Math.floor(diffMin / 60)}h ago`;
}

export function calculateFragmentation(blocks: MemoryBlock[] = []): {
  externalFragPercent: number;
  largestFreeSpan: number;
  totalFreeSpan: number;
  freeBlockCount: number;
  allocatedBlockCount: number;
} {
  const freeBlocks = blocks.filter((b) => b.free);
  const allocatedBlocks = blocks.filter((b) => !b.free);
  const totalFreeSpan = freeBlocks.reduce((acc, b) => acc + b.span, 0);
  const largestFreeSpan = freeBlocks.reduce((max, b) => Math.max(max, b.span), 0);

  // External fragmentation = 1 - (largest free block / total free bytes)
  const externalFragPercent =
    totalFreeSpan > 0 && largestFreeSpan > 0
      ? Math.max(0, Math.min(100, (1 - largestFreeSpan / totalFreeSpan) * 100))
      : 0;

  return {
    externalFragPercent,
    largestFreeSpan,
    totalFreeSpan,
    freeBlockCount: freeBlocks.length,
    allocatedBlockCount: allocatedBlocks.length,
  };
}
