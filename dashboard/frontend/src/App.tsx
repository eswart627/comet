import { useState, useRef, useEffect, useCallback } from "react";
import {
  CometArena,
  SCENARIO_COALESCING,
  SCENARIO_REUSE,
} from "./utils/emulator";
import type {
  Block,
  CoalesceEvent,
  NeighborInfo,
} from "./utils/emulator";
import { formatBytes, formatHex } from "./utils/formatters";
import {
  Play,
  Pause,
  SkipForward,
  RotateCcw,
  Plus,
  Trash2,
  GitMerge,
  ArrowLeft,
  ArrowRight,
  Split,
  Layers,
} from "lucide-react";
import "./App.css";

type DemoMode = "coalesce" | "reuse" | "churn";

export default function App() {
  const arenaRef = useRef<CometArena>(new CometArena(4096));
  const [blocks, setBlocks] = useState<Block[]>([]);
  const [events, setEvents] = useState<CoalesceEvent[]>([]);
  const [selectedIdx, setSelectedIdx] = useState<number | null>(null);
  const [customSize, setCustomSize] = useState<number>(128);

  // Play / Pause / Step State
  const [activeMode, setActiveMode] = useState<DemoMode>("coalesce");
  const [isPlaying, setIsPlaying] = useState<boolean>(false);
  const [stepIndex, setStepIndex] = useState<number>(0);
  const [playbackSpeed, setPlaybackSpeed] = useState<number>(1500); // ms per step
  const timerRef = useRef<number | null>(null);

  // Sync state from arena
  const syncState = useCallback(() => {
    setBlocks([...arenaRef.current.blocks]);
    setEvents([...arenaRef.current.events]);
  }, []);

  // Initialize
  useEffect(() => {
    const arena = arenaRef.current;
    arena.reset();
    arena.allocate(128);
    arena.allocate(256);
    arena.allocate(128);
    syncState();
  }, [syncState]);

  // Execute a single step of the current scenario
  const executeCurrentScenarioStep = useCallback(() => {
    const arena = arenaRef.current;

    if (activeMode === "churn") {
      arena.randomStep();
      syncState();
      return true;
    }

    const steps = activeMode === "coalesce" ? SCENARIO_COALESCING : SCENARIO_REUSE;
    if (stepIndex >= steps.length) {
      setIsPlaying(false);
      return false;
    }

    steps[stepIndex].execute(arena);
    syncState();
    setStepIndex((prev) => prev + 1);

    if (stepIndex + 1 >= steps.length) {
      setIsPlaying(false);
      return false;
    }
    return true;
  }, [activeMode, stepIndex, syncState]);

  // Playback loop with Play / Pause support
  useEffect(() => {
    if (!isPlaying) {
      if (timerRef.current) {
        window.clearTimeout(timerRef.current);
        timerRef.current = null;
      }
      return;
    }

    const runLoop = () => {
      const canContinue = executeCurrentScenarioStep();
      if (canContinue) {
        timerRef.current = window.setTimeout(runLoop, playbackSpeed);
      } else {
        setIsPlaying(false);
      }
    };

    timerRef.current = window.setTimeout(runLoop, playbackSpeed);

    return () => {
      if (timerRef.current) {
        window.clearTimeout(timerRef.current);
        timerRef.current = null;
      }
    };
  }, [isPlaying, playbackSpeed, executeCurrentScenarioStep]);

  // Change scenario
  const handleSelectMode = (mode: DemoMode) => {
    setIsPlaying(false);
    setActiveMode(mode);
    setStepIndex(0);
    setSelectedIdx(null);
    const arena = arenaRef.current;
    arena.reset();

    if (mode === "coalesce") {
      SCENARIO_COALESCING[0].execute(arena);
      setStepIndex(1);
    } else if (mode === "reuse") {
      SCENARIO_REUSE[0].execute(arena);
      setStepIndex(1);
    } else {
      arena.allocate(128);
      arena.allocate(256);
    }
    syncState();
  };

  const handleTogglePlay = () => {
    if (isPlaying) {
      setIsPlaying(false);
    } else {
      // If reached the end of scenario, restart from step 0
      const steps = activeMode === "coalesce" ? SCENARIO_COALESCING : SCENARIO_REUSE;
      if (activeMode !== "churn" && stepIndex >= steps.length) {
        arenaRef.current.reset();
        steps[0].execute(arenaRef.current);
        syncState();
        setStepIndex(1);
      }
      setIsPlaying(true);
    }
  };

  const handleNextStep = () => {
    setIsPlaying(false);
    executeCurrentScenarioStep();
  };

  const handleReset = () => {
    setIsPlaying(false);
    setStepIndex(0);
    arenaRef.current.reset();
    syncState();
    setSelectedIdx(null);
  };

  const handleAllocate = (size: number) => {
    if (size <= 0) return;
    setIsPlaying(false);
    arenaRef.current.allocate(size);
    syncState();
  };

  const handleFree = (index: number) => {
    setIsPlaying(false);
    arenaRef.current.free(index);
    syncState();
    setSelectedIdx(null);
  };

  const stats = arenaRef.current.getStats();
  const selectedNeighbors: NeighborInfo | null =
    selectedIdx !== null ? arenaRef.current.getNeighbors(selectedIdx) : null;

  // Active step description for banner
  const currentScenarioSteps =
    activeMode === "coalesce"
      ? SCENARIO_COALESCING
      : activeMode === "reuse"
        ? SCENARIO_REUSE
        : null;

  const currentStepInfo =
    currentScenarioSteps && stepIndex > 0
      ? currentScenarioSteps[Math.min(stepIndex - 1, currentScenarioSteps.length - 1)]
      : null;

  return (
    <div className="clean-app-wrapper">
      {/* 1. Header */}
      <header className="clean-header">
        <div className="clean-brand">
          <div className="brand-dot" />
          <div>
            <h1>COMET</h1>
            <p>C++ Memory Allocator</p>
          </div>
        </div>

        {/* Demo Scenario Selector */}
        <div className="scenario-pills" role="radiogroup" aria-label="Demo Scenario">
          <button
            type="button"
            className={`scenario-pill ${activeMode === "coalesce" ? "active" : ""}`}
            onClick={() => handleSelectMode("coalesce")}
          >
            <GitMerge size={12} />
            <span>Block Merge Demo</span>
          </button>

          <button
            type="button"
            className={`scenario-pill ${activeMode === "reuse" ? "active" : ""}`}
            onClick={() => handleSelectMode("reuse")}
          >
            <Split size={12} />
            <span>Hole Reuse &amp; Splitting</span>
          </button>

          <button
            type="button"
            className={`scenario-pill ${activeMode === "churn" ? "active" : ""}`}
            onClick={() => handleSelectMode("churn")}
          >
            <Layers size={12} />
            <span>Continuous Churn</span>
          </button>
        </div>
      </header>

      {/* 2. Play / Pause / Step Controls Bar */}
      <section className="playback-bar" aria-label="Demonstration Controls">
        <div className="playback-controls">
          <button
            type="button"
            className={`ctrl-btn ${isPlaying ? "btn-pause" : "btn-play"}`}
            onClick={handleTogglePlay}
            title={isPlaying ? "Pause demonstration" : "Start / resume playback"}
          >
            {isPlaying ? <Pause size={14} /> : <Play size={14} fill="currentColor" />}
            <span>{isPlaying ? "PAUSE" : "PLAY"}</span>
          </button>

          <button
            type="button"
            className="ctrl-btn btn-secondary"
            onClick={handleNextStep}
            title="Step to next allocator operation"
          >
            <SkipForward size={14} />
            <span>STEP</span>
          </button>

          <button
            type="button"
            className="ctrl-btn btn-secondary"
            onClick={handleReset}
            title="Reset arena to initial 4 KB pool"
          >
            <RotateCcw size={14} />
            <span>RESET</span>
          </button>

          <div className="speed-toggle">
            <span className="speed-label">SPEED:</span>
            <button
              type="button"
              className={`speed-btn ${playbackSpeed === 1500 ? "active" : ""}`}
              onClick={() => setPlaybackSpeed(1500)}
            >
              1x
            </button>
            <button
              type="button"
              className={`speed-btn ${playbackSpeed === 2500 ? "active" : ""}`}
              onClick={() => setPlaybackSpeed(2500)}
            >
              0.5x
            </button>
          </div>
        </div>

        {/* Status Callout */}
        <div className="step-callout">
          {activeMode === "churn" ? (
            <span>
              <strong>Continuous Churn:</strong> {isPlaying ? "Running gentle allocations and frees. Press Pause to inspect." : "Paused. Press Play to resume."}
            </span>
          ) : currentStepInfo ? (
            <span>
              <strong>{currentStepInfo.title}:</strong> {currentStepInfo.explanation}
            </span>
          ) : (
            <span>Press Play or Step to begin the demonstration scenario.</span>
          )}
        </div>
      </section>

      {/* 3. At-a-Glance Stats Bar */}
      <section className="stats-bar" aria-label="Arena Summary">
        <div className="stat-item">
          <span className="stat-label">TOTAL POOL</span>
          <span className="stat-value font-mono">{formatBytes(stats.poolSize)}</span>
        </div>
        <div className="stat-item">
          <span className="stat-label">ALLOCATED</span>
          <span className="stat-value font-mono text-cyan">
            {formatBytes(stats.allocatedBytes)}
            <small className="stat-sub">({Math.round((stats.allocatedBytes / stats.poolSize) * 100)}%)</small>
          </span>
        </div>
        <div className="stat-item">
          <span className="stat-label">FREE MEMORY</span>
          <span className="stat-value font-mono text-green">
            {formatBytes(stats.freeBytes)}
          </span>
        </div>
        <div className="stat-item">
          <span className="stat-label">TOTAL BLOCKS</span>
          <span className="stat-value font-mono">
            {stats.allocatedCount} <small className="text-muted">alloc</small> • {stats.freeCount} <small className="text-muted">free</small>
          </span>
        </div>
      </section>

      {/* 4. Direct Action Controls */}
      <section className="controls-bar">
        <div className="action-group">
          <span className="group-label">QUICK ALLOCATE:</span>
          {[64, 128, 256, 512].map((sz) => (
            <button
              key={sz}
              type="button"
              className="chip-btn"
              onClick={() => handleAllocate(sz)}
            >
              <Plus size={11} />
              <span>{sz} B</span>
            </button>
          ))}
        </div>

        <div className="custom-alloc-group">
          <input
            type="number"
            min="16"
            max="2048"
            step="16"
            value={customSize}
            onChange={(e) => setCustomSize(Number(e.target.value))}
            className="custom-input font-mono"
          />
          <span className="input-unit">Bytes</span>
          <button
            type="button"
            className="alloc-btn"
            onClick={() => handleAllocate(customSize)}
            disabled={customSize <= 0}
          >
            Allocate
          </button>
        </div>
      </section>

      {/* 5. Memory Arena Visualization (The Star Component) */}
      <section className="arena-section">
        <div className="section-header">
          <h2>MEMORY ARENA TAPE</h2>
          <span className="hint-text">
            Click any block to inspect boundary tags or free it to trigger coalescing.
          </span>
        </div>

        <div className="arena-tape">
          {blocks.map((b, idx) => {
            const isSelected = selectedIdx === idx;
            const pct = Math.max((b.span / stats.poolSize) * 100, 3.5);

            return (
              <div
                key={b.id}
                className={`tape-block ${b.free ? "is-free" : "is-alloc"} ${
                  isSelected ? "is-selected" : ""
                } ${b.justCoalesced ? "is-coalesced-flash" : ""}`}
                style={{ flexBasis: `${pct}%`, flexGrow: b.span }}
                onClick={() => setSelectedIdx(idx)}
                title={[
                  `Block #${b.id}`,
                  `Status: ${b.free ? "FREE" : "ALLOCATED"}`,
                  `Span: ${b.span} B`,
                  b.free ? "" : `Payload: ${b.requestedSize} B`,
                  `Offset: ${formatHex(b.offset)}`,
                ].filter(Boolean).join(" • ")}
              >
                <div className="block-top">
                  <span className="block-id">#{b.id}</span>
                  <span className="block-tag">{b.free ? "FREE" : "ALLOC"}</span>
                </div>

                <div className="block-middle">
                  <span className="block-size font-mono">{b.span} B</span>
                </div>

                {!b.free && (
                  <button
                    type="button"
                    className="inline-free-btn"
                    onClick={(e) => {
                      e.stopPropagation();
                      handleFree(idx);
                    }}
                    title="Free this block and coalesce with neighbors"
                  >
                    Free
                  </button>
                )}
              </div>
            );
          })}
        </div>

        <div className="arena-legend">
          <div className="legend-item">
            <span className="legend-indicator alloc-bg" />
            <span>Allocated (Payload + 40 B Header)</span>
          </div>
          <div className="legend-item">
            <span className="legend-indicator free-bg" />
            <span>Free Space</span>
          </div>
        </div>
      </section>

      {/* 6. Selected Block Boundary Tag & Neighbor Linkage Inspector */}
      {selectedNeighbors && (
        <section className="inspector-card">
          <div className="inspector-title-row">
            <div className="inspector-tag">
              <span className="inspector-num">BLOCK #{selectedNeighbors.current.id}</span>
              <span
                className={`badge ${
                  selectedNeighbors.current.free ? "badge-free" : "badge-alloc"
                }`}
              >
                {selectedNeighbors.current.free ? "FREE LIST BLOCK" : "ACTIVE ALLOCATION"}
              </span>
            </div>
            {!selectedNeighbors.current.free && (
              <button
                type="button"
                className="action-free-btn"
                onClick={() => handleFree(selectedIdx!)}
              >
                <Trash2 size={13} />
                <span>Free Block</span>
              </button>
            )}
          </div>

          {/* Boundary Tag Neighbor Linkage (Left <-> Current <-> Right) */}
          <div className="neighbors-link-row">
            <div
              className={`neighbor-chip ${
                selectedNeighbors.prev
                  ? selectedNeighbors.prev.free
                    ? "neighbor-free"
                    : "neighbor-alloc"
                  : "neighbor-edge"
              }`}
            >
              <ArrowLeft size={12} />
              <span>
                PREV:{" "}
                {selectedNeighbors.prev
                  ? `${selectedNeighbors.prev.free ? "FREE" : "ALLOC"} (${selectedNeighbors.prev.span} B)`
                  : "START OF ARENA"}
              </span>
            </div>

            <div className="neighbor-middle-chip">
              <span className="middle-dot" />
              <span>CURRENT: {selectedNeighbors.current.span} B</span>
            </div>

            <div
              className={`neighbor-chip ${
                selectedNeighbors.next
                  ? selectedNeighbors.next.free
                    ? "neighbor-free"
                    : "neighbor-alloc"
                  : "neighbor-edge"
              }`}
            >
              <span>
                NEXT:{" "}
                {selectedNeighbors.next
                  ? `${selectedNeighbors.next.free ? "FREE" : "ALLOC"} (${selectedNeighbors.next.span} B)`
                  : "END OF ARENA"}
              </span>
              <ArrowRight size={12} />
            </div>
          </div>

          {/* Diagnostics Grid */}
          <div className="inspector-grid font-mono">
            <div className="inspector-cell">
              <span className="cell-label">BASE OFFSET</span>
              <span className="cell-val text-cyan">{formatHex(selectedNeighbors.current.offset)}</span>
            </div>
            <div className="inspector-cell">
              <span className="cell-label">TOTAL SPAN</span>
              <span className="cell-val">{selectedNeighbors.current.span} Bytes</span>
            </div>
            <div className="inspector-cell">
              <span className="cell-label">REQUESTED PAYLOAD</span>
              <span className="cell-val text-green">
                {selectedNeighbors.current.free ? "0 B" : `${selectedNeighbors.current.requestedSize} Bytes`}
              </span>
            </div>
            <div className="inspector-cell">
              <span className="cell-label">BOUNDARY TAG HEADER</span>
              <span className="cell-val text-muted">40 Bytes (prev, next, span, free)</span>
            </div>
          </div>

          <div className="coalesce-verdict">
            <span>
              <strong>Coalescing Eligibility:</strong>{" "}
              {selectedNeighbors.canMergeLeft && selectedNeighbors.canMergeRight
                ? "✨ Both Left & Right neighbors are free! Freeing this block merges 3 blocks into one."
                : selectedNeighbors.canMergeLeft
                  ? "⬅ Left neighbor is free. Will coalesce backward."
                  : selectedNeighbors.canMergeRight
                    ? "➡ Right neighbor is free. Will coalesce forward."
                    : "🔒 Neither neighbor is free. Block will become an isolated free hole until adjacent blocks are freed."}
            </span>
          </div>
        </section>
      )}

      {/* 7. Minimal Activity Feed (Last 4 Events) */}
      <section className="events-section">
        <div className="section-header">
          <h2>ACTIVITY LOG</h2>
          <span className="hint-text">Real-time trace of memory events and merges</span>
        </div>

        <div className="events-list">
          {events.slice(0, 5).map((ev) => (
            <div key={ev.id} className={`event-row event-${ev.type}`}>
              <span className="event-time font-mono">{ev.time}</span>
              <span className={`event-badge badge-${ev.type}`}>
                {ev.type === "coalesce" ? "MERGE" : ev.type.toUpperCase()}
              </span>
              <div className="event-text">
                <strong>{ev.description}</strong>
                {ev.details && <span className="event-details">{ev.details}</span>}
              </div>
            </div>
          ))}
        </div>
      </section>

      {/* 8. Footer */}
      <footer className="clean-footer">
        <span>Comet Allocator • Doubly-linked boundary tag free list with O(1) coalescing</span>
        <span className="font-mono">Linux x86_64 • 16-byte alignment</span>
      </footer>
    </div>
  );
}