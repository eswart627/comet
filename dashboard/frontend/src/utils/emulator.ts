export type Block = {
  id: number;
  offset: number;
  span: number;
  requestedSize: number;
  free: boolean;
  justCoalesced?: boolean;
};

export type CoalesceEvent = {
  id: number;
  time: string;
  type: "alloc" | "free" | "coalesce" | "split" | "reset";
  description: string;
  details?: string;
};

export type NeighborInfo = {
  prev: Block | null;
  current: Block;
  next: Block | null;
  canMergeLeft: boolean;
  canMergeRight: boolean;
};

export type DemoStep = {
  title: string;
  explanation: string;
  execute: (arena: CometArena) => void;
};

export class CometArena {
  public poolSize: number;
  public blocks: Block[] = [];
  public events: CoalesceEvent[] = [];
  private nextId = 1;
  public readonly headerSize = 40; // sizeof(Block) aligned boundary tag

  constructor(poolSize = 4096) {
    this.poolSize = poolSize;
    this.reset();
  }

  public reset() {
    this.blocks = [
      {
        id: this.nextId++,
        offset: 0,
        span: this.poolSize,
        requestedSize: 0,
        free: true,
      },
    ];
    this.events = [
      {
        id: Date.now(),
        time: new Date().toLocaleTimeString(),
        type: "reset",
        description: `Arena initialized with 1 continuous free block of ${this.poolSize} bytes`,
      },
    ];
  }

  public allocate(size: number): boolean {
    const alignedSize = Math.ceil(size / 8) * 8;
    const totalNeeded = alignedSize + this.headerSize;

    // First-fit search across free blocks
    const index = this.blocks.findIndex((b) => b.free && b.span >= totalNeeded);
    if (index === -1) {
      this.events.unshift({
        id: Date.now() + Math.random(),
        time: new Date().toLocaleTimeString(),
        type: "alloc",
        description: `Allocation failed: No free block >= ${totalNeeded} B (${size} B payload + 40 B header)`,
      });
      return false;
    }

    const target = this.blocks[index];
    const remainder = target.span - totalNeeded;

    // Clear previous coalesced highlights
    this.blocks.forEach((b) => (b.justCoalesced = false));

    if (remainder >= this.headerSize + 16) {
      // Split block in-place
      const allocatedBlock: Block = {
        id: this.nextId++,
        offset: target.offset,
        span: totalNeeded,
        requestedSize: size,
        free: false,
      };

      const freeRemainder: Block = {
        id: this.nextId++,
        offset: target.offset + totalNeeded,
        span: remainder,
        requestedSize: 0,
        free: true,
      };

      this.blocks.splice(index, 1, allocatedBlock, freeRemainder);

      this.events.unshift({
        id: Date.now() + Math.random(),
        time: new Date().toLocaleTimeString(),
        type: "split",
        description: `Allocated ${size} B (span: ${totalNeeded} B) at 0x${allocatedBlock.offset.toString(16).toUpperCase().padStart(4, "0")}`,
        details: `Split original free block into Block #${allocatedBlock.id} and free remainder of ${remainder} B`,
      });
    } else {
      // Consume whole block
      target.free = false;
      target.requestedSize = size;

      this.events.unshift({
        id: Date.now() + Math.random(),
        time: new Date().toLocaleTimeString(),
        type: "alloc",
        description: `Allocated ${size} B (exact fit, span: ${target.span} B) at 0x${target.offset.toString(16).toUpperCase().padStart(4, "0")}`,
      });
    }

    return true;
  }

  public free(index: number): boolean {
    if (index < 0 || index >= this.blocks.length || this.blocks[index].free) {
      return false;
    }

    // Clear previous highlights
    this.blocks.forEach((b) => (b.justCoalesced = false));

    const current = this.blocks[index];
    const freedSize = current.requestedSize;
    current.free = true;
    current.requestedSize = 0;

    const hadNextFree = index + 1 < this.blocks.length && this.blocks[index + 1].free;
    const hadPrevFree = index > 0 && this.blocks[index - 1].free;

    let mergedDetails = "";

    // Forward Coalescing (merge with next free block)
    if (hadNextFree) {
      const nextBlock = this.blocks[index + 1];
      current.span += nextBlock.span;
      this.blocks.splice(index + 1, 1);
      mergedDetails += `Merged forward with next free block (+${nextBlock.span} B). `;
    }

    // Backward Coalescing (merge with previous free block)
    let finalBlock = current;
    if (hadPrevFree) {
      const prevBlock = this.blocks[index - 1];
      prevBlock.span += current.span;
      this.blocks.splice(index, 1);
      finalBlock = prevBlock;
      mergedDetails += `Merged backward with previous free block (+${current.span} B). `;
    }

    finalBlock.justCoalesced = hadNextFree || hadPrevFree;

    if (hadNextFree || hadPrevFree) {
      this.events.unshift({
        id: Date.now() + Math.random(),
        time: new Date().toLocaleTimeString(),
        type: "coalesce",
        description: `Freed Block #${current.id} (${freedSize} B) and COALESCED into a single ${finalBlock.span} B free block!`,
        details: mergedDetails.trim(),
      });
    } else {
      this.events.unshift({
        id: Date.now() + Math.random(),
        time: new Date().toLocaleTimeString(),
        type: "free",
        description: `Freed Block #${current.id} (${freedSize} B) at 0x${current.offset.toString(16).toUpperCase().padStart(4, "0")}`,
        details: "No adjacent free blocks to coalesce with right now.",
      });
    }

    return true;
  }

  public getNeighbors(index: number): NeighborInfo | null {
    if (index < 0 || index >= this.blocks.length) return null;
    const current = this.blocks[index];
    const prev = index > 0 ? this.blocks[index - 1] : null;
    const next = index + 1 < this.blocks.length ? this.blocks[index + 1] : null;

    return {
      prev,
      current,
      next,
      canMergeLeft: prev !== null && prev.free,
      canMergeRight: next !== null && next.free,
    };
  }

  public randomStep(): boolean {
    const allocatedIndices: number[] = [];
    this.blocks.forEach((b, i) => {
      if (!b.free) allocatedIndices.push(i);
    });

    // 50% chance free an allocated block if we have >= 2
    if (allocatedIndices.length >= 2 && Math.random() > 0.45) {
      const victim = allocatedIndices[Math.floor(Math.random() * allocatedIndices.length)];
      return this.free(victim);
    } else {
      // Allocate random typical size
      const sizes = [64, 128, 256, 512];
      const sz = sizes[Math.floor(Math.random() * sizes.length)];
      return this.allocate(sz);
    }
  }

  public getAllocatedBytes(): number {
    return this.blocks
      .filter((b) => !b.free)
      .reduce((sum, b) => sum + b.requestedSize, 0);
  }

  public getStats() {
    const allocatedBlocks = this.blocks.filter((b) => !b.free);
    const freeBlocks = this.blocks.filter((b) => b.free);
    const totalFree = freeBlocks.reduce((sum, b) => sum + b.span, 0);
    const largestFree = freeBlocks.reduce((max, b) => Math.max(max, b.span), 0);

    const externalFrag =
      totalFree > 0 && largestFree > 0
        ? Math.round((1 - largestFree / totalFree) * 100)
        : 0;

    return {
      poolSize: this.poolSize,
      allocatedBytes: this.getAllocatedBytes(),
      freeBytes: totalFree,
      allocatedCount: allocatedBlocks.length,
      freeCount: freeBlocks.length,
      totalBlocks: this.blocks.length,
      largestFree,
      externalFrag,
    };
  }
}

export const SCENARIO_COALESCING: DemoStep[] = [
  {
    title: "1. Fresh Setup",
    explanation: "Pool initialized to 4,096 B. Allocating Block A (128 B)...",
    execute: (arena) => {
      arena.reset();
      arena.allocate(128);
    },
  },
  {
    title: "2. Allocate Block B (256 B)",
    explanation: "Allocating Block B directly adjacent to Block A.",
    execute: (arena) => {
      arena.allocate(256);
    },
  },
  {
    title: "3. Allocate Block C (128 B)",
    explanation: "Now we have 3 consecutive allocated blocks: [A: 128B] [B: 256B] [C: 128B].",
    execute: (arena) => {
      arena.allocate(128);
    },
  },
  {
    title: "4. Free Middle Block B",
    explanation: "Freed middle Block B. It forms an isolated free hole between A and C.",
    execute: (arena) => {
      arena.free(1);
    },
  },
  {
    title: "5. Free Block A (Forward Coalesce)",
    explanation: "Freeing Block A. Comet detects neighbor B is free and merges forward in O(1) time!",
    execute: (arena) => {
      arena.free(0);
    },
  },
  {
    title: "6. Free Block C (Backward Coalesce)",
    explanation: "Freeing Block C. It merges backward with the AB block into one continuous free span with 0% fragmentation!",
    execute: (arena) => {
      arena.free(1);
    },
  },
];

export const SCENARIO_REUSE: DemoStep[] = [
  {
    title: "1. Initial Allocations",
    explanation: "Allocating Block A (256 B) and Block B (128 B)...",
    execute: (arena) => {
      arena.reset();
      arena.allocate(256);
      arena.allocate(128);
    },
  },
  {
    title: "2. Free Block A (Hole Created)",
    explanation: "Freed Block A (256 B requested, 296 B span). Creates a free hole at pool offset 0x0000.",
    execute: (arena) => {
      arena.free(0);
    },
  },
  {
    title: "3. First-Fit Reclaims Hole & Splits",
    explanation: "Allocating new Block C (128 B). Comet finds Block A's hole, fits 128B+40B, and splits the rest into an 88 B free block!",
    execute: (arena) => {
      arena.allocate(128);
    },
  },
  {
    title: "4. Cleanup & Full Merge",
    explanation: "Freeing all blocks. All fragments and split remainders immediately coalesce back into the main free pool.",
    execute: (arena) => {
      // Free Block C and B
      arena.free(0);
      arena.free(arena.blocks.findIndex((b) => !b.free));
    },
  },
];
