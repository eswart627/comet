export type AllocatorState = {
  running: boolean;
  pool_size: number;
  allocated_bytes: number;
  free_bytes: number;
  live_allocations: number;
  total_allocations: number;
  total_deallocations: number;
  failed_allocations: number;
  allocation_size: number;
  thread_count: number;
  deallocation_probability: number;
};

export type Sample = {
  timestamp_ms: number;
  allocated_bytes: number;
  free_bytes: number;
  live_allocations: number;
  ops_sec?: number;
};

export type MemoryBlock = {
  offset: number;
  span: number;
  requested_size: number;
  free: boolean;
};

export type MemoryMap = {
  pool_size: number;
  blocks: MemoryBlock[];
};

export type TelemetryEvent = {
  id?: number | string;
  timestamp_ms?: number;
  timestamp?: string;
  type?: string;
  event?: string;
  message?: string;
  size?: number;
  thread_id?: number;
};

export type ScenarioPreset = {
  id: string;
  name: string;
  desc: string;
  allocation_size: number;
  thread_count: number;
  deallocation_probability: number;
  badge: string;
  icon: string;
};

export type BenchmarkRun = {
  threads: number;
  allocation_size: number;
  iterations: number;
  comet_ms: number;
  system_ms: number;
  comet_ops: number;
  system_ops: number;
};

export type ConnectionMode = "websocket" | "polling" | "simulation";
