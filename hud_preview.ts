/**
 * Ghost Hive v2 Watch HUD preview.
 * Raster 48×24. No PSP, radio, events, or policies — draw only.
 */

export const HUD_COLS = 48;
export const HUD_ROWS = 24;

export type ScanAp = {
  ssid: string;
  rssi: number;
  channel: number;
  enc: string;
};

export type ScanSnapshot = {
  ibssOn: boolean;
  ackBdUsed: number;
  mines: number;
  replay: number;
  replayBlk: boolean;
  alerts: number;
  hbMiss: number;
  drift: string;
  workerSync: string;
  last: string;
  time: string;
  aps: ScanAp[];
};

export type PeerSnapshot = {
  focus: number;
  count: number;
  id: string;
  role: string;
  status: string;
  trust: number;
  last: string;
  age: string;
  drop: number;
  cap: number;
  tag: number;
  hbMiss: number;
  ram: string;
  cpu: string;
  gpu: string;
  traffic: string;
  battery: string;
  wifi: string;
};

export type KernelFlags = {
  ibssOn: boolean;
  running: boolean;
  arming: "locked" | "observe" | "armed";
  vaultN: number;
  safe: boolean;
  keys: boolean;
  auth: boolean;
  bind: boolean;
  downView: string;
  phase: string;
  snap: number;
  flush: string;
  kill: boolean;
  danger: boolean;
  policyOk: boolean;
  peek: boolean;
  hmacI: number;
};

let prevAlarm = false;

export type DangerSources = {
  blk?: boolean;
  replay?: boolean;
  twin?: boolean;
  hbmiss?: boolean;
  drop?: boolean;
  telemAbsent?: boolean;
  danger?: boolean;
};

export function dangerHeadline(src: DangerSources): boolean {
  return !!(src.blk || src.replay || src.twin || src.hbmiss || src.drop ||
            src.telemAbsent || src.danger);
}

export type UiSignal = {
  sound: boolean;
  led: "double-blink" | "off";
};

export function applyHeadlineAlarm(nowAlarm: boolean): UiSignal {
  const edge = nowAlarm && !prevAlarm;
  if (!nowAlarm) prevAlarm = false;
  else prevAlarm = true;
  if (edge && typeof process !== "undefined" && process.stdout) {
    process.stdout.write("\x07");
    process.stderr.write("LED double-blink\n");
  }
  return { sound: edge, led: edge ? "double-blink" : "off" };
}

function pad48(text: string): string {
  let s = text;
  if (s.length > HUD_COLS) s = s.slice(0, HUD_COLS);
  while (s.length < HUD_COLS) s += " ";
  return s;
}

function lab(name: string, val: string): string {
  return pad48(name + ": " + val);
}

function yn(v: boolean): string {
  return v ? "yes" : "no";
}

function hex16(n: number): string {
  const v = n & 0xffff;
  let h = v.toString(16);
  while (h.length < 4) h = "0" + h;
  return h;
}

function hex8(n: number): string {
  const v = n & 0xff;
  let h = v.toString(16);
  while (h.length < 2) h = "0" + h;
  return h;
}

function apText(ap: ScanAp | undefined, twin: boolean): string {
  if (ap === undefined) return "--";
  const ssid = ap.ssid.length > 12 ? ap.ssid.slice(0, 12) : ap.ssid;
  const t = twin && ap.ssid === "GHSTHIVE" ? "yes" : "no";
  return ssid + " r=" + String(ap.rssi) + " ch=" + String(ap.channel) + " " + ap.enc + " twin=" + t;
}

function hiveCount(aps: ScanAp[]): number {
  let n = 0;
  for (let i = 0; i < aps.length; i++) {
    if (aps[i].ssid === "GHSTHIVE") n++;
  }
  return n;
}

function foreignCount(aps: ScanAp[]): number {
  return aps.length - hiveCount(aps);
}

export class HudPreview {
  readonly rows: string[] = [];

  constructor() {
    this.clear();
  }

  clear(): void {
    this.rows.length = 0;
    for (let i = 0; i < HUD_ROWS; i++) this.rows.push(pad48(""));
  }

  drawHeader(text: string): void {
    this.rows[0] = pad48(text);
    this.rows[1] = pad48("------------------------------------------------");
  }

  setHeadlineAlarm(on: boolean): UiSignal {
    return applyHeadlineAlarm(on);
  }

  drawLine(y: number, text: string): void {
    if (y < 0 || y >= HUD_ROWS) return;
    this.rows[y] = pad48(text);
  }

  drawTwinAlert(twin: boolean): void {
    this.drawLine(this.nextBody(), lab("Twin", twin ? "yes" : "no"));
  }

  drawAP(rssi: number, channel: number, enc: string): void {
    let n = 0;
    for (let y = 2; y < HUD_ROWS; y++) {
      const t = this.rows[y].trim();
      if (t.indexOf("Ap1:") === 0 || t.indexOf("Ap2:") === 0) n++;
    }
    const label = n === 0 ? "Ap1" : "Ap2";
    this.drawLine(this.nextBody(), lab(label, "r=" + String(rssi) + " ch=" + String(channel) + " " + enc));
  }

  drawPeerCaps(cap: number, tag: number): void {
    this.drawLine(this.nextBody(), lab("Caps", hex16(cap) + " tag=" + hex8(tag)));
  }

  drawHBMiss(miss: number): void {
    this.drawLine(this.nextBody(), lab("HB", String(miss)));
  }

  renderHivePage(): void {
    this.clear();
    this.drawHeader("[GHv2] PAGE:Hive STATE:Watch");
    this.setHeadlineAlarm(dangerHeadline({
      blk: true,
      hbmiss: true,
      drop: true,
      danger: true,
    }));
    const lines: string[] = [
      lab("Hive", "ok"),
      lab("Peers", "7/32 ok=5 pend=1 blk=1 hbmiss=3 drop=12"),
      lab("Roles", "W1 P1 S1 R1 N1 M2"),
      lab("Alert", "danger 3 last=Mine 3"),
      lab("HMAC-I", "0"),
      lab("Ack", "1"),
      lab("HBMiss", "3"),
      lab("Time", "2026-08-29 21:30"),
      pad48("W        W ok age=2s hb=0 drop=0"),
      pad48("3        M dng age=1s hb=0 drop=0"),
    ];
    this.blitBody(lines);
  }

  renderKernelDownPage(): void {
    this.renderDownPage();
  }

  renderNetPage(scanSnapshot: ScanSnapshot): void {
    this.clear();
    this.drawHeader("[GHv2] PAGE:Net STATE:Watch");
    const twin = hiveCount(scanSnapshot.aps) >= 2;
    this.setHeadlineAlarm(dangerHeadline({
      twin: twin,
      replay: scanSnapshot.replayBlk,
      hbmiss: scanSnapshot.hbMiss > 0,
    }));
    const lines: string[] = [
      lab("IBSS", scanSnapshot.ibssOn ? "GHSTHIVE (ON)" : "GHSTHIVE (OFF)"),
      lab("UDP", "17471"),
      lab("Pin", "GHSTHIVE"),
      lab("ReplayW", "64"),
      lab("AckBd", String(scanSnapshot.ackBdUsed) + "/8"),
      lab("IP", "10.17.47.1"),
      lab("Scan", String(scanSnapshot.aps.length)),
      lab("Twin", twin ? "yes" : "no"),
      lab("XAP", String(foreignCount(scanSnapshot.aps))),
      lab("Ap1", apText(scanSnapshot.aps[0], twin)),
      lab("Ap2", apText(scanSnapshot.aps[1], twin)),
      lab("Replay", String(scanSnapshot.replay) + "/64 blk=" + (scanSnapshot.replayBlk ? "yes" : "no") + " mines=" + String(scanSnapshot.mines)),
      lab("Alerts", String(scanSnapshot.alerts)),
      lab("HBMiss", String(scanSnapshot.hbMiss)),
      lab("Sync", "drift=" + scanSnapshot.drift + " worker=" + scanSnapshot.workerSync),
      lab("Last", scanSnapshot.last),
      lab("Time", scanSnapshot.time),
    ];
    this.blitBody(lines);
  }

  renderPeerPage(peerSnapshot: PeerSnapshot): void {
    this.clear();
    this.drawHeader("[GHv2] PAGE:Peer STATE:Watch");
    if (peerSnapshot.count === 0) {
      this.setHeadlineAlarm(false);
      this.drawLine(2, lab("idx", "0/0"));
      return;
    }
    this.setHeadlineAlarm(dangerHeadline({
      hbmiss: peerSnapshot.hbMiss > 0,
      drop: peerSnapshot.drop > 0,
      telemAbsent: peerSnapshot.ram === "--" && peerSnapshot.cpu === "--" &&
        peerSnapshot.gpu === "--" && peerSnapshot.traffic === "--" &&
        peerSnapshot.battery === "--" && peerSnapshot.wifi === "--",
      danger: peerSnapshot.status === "danger",
    }));
    const n = peerSnapshot.focus + 1;
    const hb = peerSnapshot.hbMiss < 0 ? "--" : String(peerSnapshot.hbMiss);
    const lines: string[] = [
      lab("Dev" + String(n), "id=" + peerSnapshot.id + " role=" + peerSnapshot.role),
      lab("State", peerSnapshot.status + " trust=" + String(peerSnapshot.trust) + " hb=" + hb),
      lab("Last", peerSnapshot.last),
      lab("Age", peerSnapshot.age),
      lab("Drop", String(peerSnapshot.drop)),
      lab("Caps", hex16(peerSnapshot.cap) + " tag=" + hex8(peerSnapshot.tag)),
      lab("Telem", "ram=" + peerSnapshot.ram + " cpu=" + peerSnapshot.cpu + " gpu=" + peerSnapshot.gpu + " n=" + peerSnapshot.traffic + " b=" + peerSnapshot.battery + " w=" + peerSnapshot.wifi),
      lab("idx", String(n) + "/" + String(peerSnapshot.count)),
    ];
    this.blitBody(lines);
  }

  renderKernelPage(kernelFlags: KernelFlags): void {
    this.clear();
    this.drawHeader("[GHv2] PAGE:Kernel STATE:Watch");
    this.setHeadlineAlarm(dangerHeadline({ danger: kernelFlags.danger }));
    const kill = yn(kernelFlags.kill);
    const dng = kernelFlags.danger ? "on" : "off";
    const lines: string[] = [
      lab("IBSS", kernelFlags.ibssOn ? "GHSTHIVE (ON)" : "GHSTHIVE (OFF)"),
      lab("Radio", kernelFlags.ibssOn ? "on" : "off"),
      lab("Kernel", kernelFlags.running ? "active" : "inactive"),
      lab("Arming", kernelFlags.arming),
      lab("Down", kernelFlags.downView + " " + kernelFlags.phase + " kill=" + kill + " danger=" + dng),
      lab("Vault", String(kernelFlags.vaultN) + "/64 safe=" + yn(kernelFlags.safe)),
      lab("Bind", yn(kernelFlags.bind) + " auth=" + yn(kernelFlags.auth) + " keys=" + yn(kernelFlags.keys)),
      lab("Snap", String(kernelFlags.snap)),
      lab("Flush", kernelFlags.flush),
      lab("Policy", kernelFlags.policyOk ? "ok" : "error"),
      lab("Peek", kernelFlags.peek ? "on" : "off"),
      lab("HMAC-I", String(kernelFlags.hmacI)),
    ];
    this.blitBody(lines);
  }

  renderDownPage(): void {
    this.clear();
    this.drawHeader("[GHv2] PAGE:Kernel STATE:Down");
    this.setHeadlineAlarm(false);
    const lines: string[] = [
      lab("Down", "active freeze kill=no danger=off"),
      lab("Timer", "0"),
      lab("Arming", "locked"),
      lab("Vault", "3/64 safe=no"),
      lab("Snap", "1"),
      lab("Flush", "pending"),
      lab("Policy", "ok"),
      lab("GameLook", "off"),
      lab("Replay", "12 ack=0/8"),
      lab("Hive", "down"),
      lab("Alert", "down last=GDStart kernel"),
    ];
    this.blitBody(lines);
  }

  toString(): string {
    return this.rows.join("\n");
  }

  private blitBody(lines: string[]): void {
    let y = 2;
    for (let i = 0; i < lines.length && y < HUD_ROWS; i++) {
      this.rows[y] = lines[i];
      y++;
    }
  }

  private nextBody(): number {
    for (let y = 2; y < HUD_ROWS; y++) {
      if (this.rows[y].trim() === "") return y;
    }
    return HUD_ROWS - 1;
  }
}

export const hud = new HudPreview();

export function drawHeader(text: string): void {
  hud.drawHeader(text);
}

export function drawLine(y: number, text: string): void {
  hud.drawLine(y, text);
}

export function drawTwinAlert(twin: boolean): void {
  hud.drawTwinAlert(twin);
}

export function drawAP(rssi: number, channel: number, enc: string): void {
  hud.drawAP(rssi, channel, enc);
}

export function drawPeerCaps(cap: number, tag: number): void {
  hud.drawPeerCaps(cap, tag);
}

export function drawHBMiss(miss: number): void {
  hud.drawHBMiss(miss);
}

export function renderHivePage(): void {
  hud.renderHivePage();
}

export function renderKernelDownPage(): void {
  hud.renderKernelDownPage();
}

export function renderNetPage(scanSnapshot: ScanSnapshot): void {
  hud.renderNetPage(scanSnapshot);
}

export function renderPeerPage(peerSnapshot: PeerSnapshot): void {
  hud.renderPeerPage(peerSnapshot);
}

export function renderKernelPage(kernelFlags: KernelFlags): void {
  hud.renderKernelPage(kernelFlags);
}

export function renderDownPage(): void {
  hud.renderDownPage();
}

/** Sample frames for a local look — still no I/O besides return value. */
export function previewDemo(): string {
  const hud = new HudPreview();
  const parts: string[] = [];

  hud.renderHivePage();
  parts.push(hud.toString());

  hud.renderNetPage({
    ibssOn: true,
    ackBdUsed: 2,
    mines: 3,
    replay: 12,
    replayBlk: true,
    alerts: 0,
    hbMiss: 0,
    drift: "+2s",
    workerSync: "trust>=2",
    last: "Telem W",
    time: "2026-08-29 21:30",
    aps: [
      { ssid: "GHSTHIVE", rssi: -40, channel: 6, enc: "open" },
      { ssid: "GHSTHIVE", rssi: -70, channel: 11, enc: "wpa2" },
    ],
  });
  parts.push(hud.toString());

  hud.renderPeerPage({
    focus: 0,
    count: 3,
    id: "W",
    role: "Worker",
    status: "ok",
    trust: 3,
    last: "21:30",
    age: "2s",
    drop: 0,
    cap: 0x0003,
    tag: 0x0a,
    hbMiss: 0,
    ram: "512",
    cpu: "12",
    gpu: "--",
    traffic: "0",
    battery: "88",
    wifi: "54",
  });
  parts.push(hud.toString());

  hud.renderKernelPage({
    ibssOn: true,
    running: true,
    arming: "locked",
    vaultN: 3,
    safe: false,
    keys: true,
    auth: true,
    bind: true,
    downView: "locked",
    phase: "idle",
    snap: 0,
    flush: "pending",
    kill: false,
    danger: false,
    policyOk: true,
    peek: false,
    hmacI: 0,
  });
  parts.push(hud.toString());

  hud.renderDownPage();
  parts.push(hud.toString());

  return parts.join("\n\n");
}
