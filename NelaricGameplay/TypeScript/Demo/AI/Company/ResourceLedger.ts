// Copyright (c) 2026 Nelaric Contributors

import { Claim, clone, TacticalArea } from "./CommandContracts";

/** One shared ledger per company world; no decisions and no force creation. */
export class ResourceLedger {
    private claims: Claim[] = [];
    constructor(private readonly areas: Map<string, TacticalArea>) {}
    reserve(group: Claim[], now: number): boolean {
        this.expire(now);
        const keys = new Set(group.map(c => c.key + "/" + c.owner));
        if (keys.size !== group.length || group.some(c => !c.owner || !Number.isFinite(c.amount) || c.amount <= 0 ||
            !Number.isFinite(c.expiresAt) || c.expiresAt <= now)) return false;
        const next = this.claims.filter(c => !keys.has(c.key + "/" + c.owner)).concat(clone(group));
        for (const key of new Set(next.map(c => c.key))) {
            const capacity = this.capacity(key, now);
            const local = next.filter(c => c.key === key);
            const transferred = new Set(local.map(c => c.replaces).filter(Boolean));
            if (capacity < local.filter(c => !transferred.has(c.owner)).reduce((sum, c) => sum + c.amount, 0)) return false;
        }
        this.claims = next;
        return true;
    }
    private capacity(key: string, now: number): number {
        const area = this.areas.get(key);
        if (area) return area.capacity;
        for (const a of this.areas.values()) {
            const p = a.passages.find(p => p.id === key);
            if (p) return p.available && now >= p.opensAt && (p.closesAt <= 0 || now < p.closesAt) ? p.capacity : 0;
        }
        return 0;
    }
    commit(owner: string, now: number, duration: number): void {
        for (const c of this.claims) if (c.owner === owner) { c.provisional = false; c.expiresAt = now + duration; }
    }
    release(owner: string): void { this.claims = this.claims.filter(c => c.owner !== owner); }
    renew(owner: string, now: number, duration: number): boolean {
        const current = this.claims.filter(c => c.owner === owner).map(c => ({ ...c, expiresAt: now + duration }));
        return current.length === 0 || this.reserve(current, now);
    }
    expire(now: number): void { this.claims = this.claims.filter(c => c.expiresAt > now); }
    snapshot(): Claim[] { return clone(this.claims); }
}
