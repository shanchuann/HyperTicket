import './SeatMini.css';

interface Props {
  seatLabel: string; // e.g. "A20", "B3"
  tier: string;
}

const TIER_COLOR: Record<string, string> = {
  VIP:      '#e8a020',
  Standard: '#3a7abf',
  Economy:  '#8aabb8',
};

// Parse "A20" → { row: 0, col: 19 }
const parse = (label: string) => {
  const match = label.match(/^([A-Z]+)(\d+)$/);
  if (!match) return { row: 0, col: 0 };
  const rowStr = match[1];
  let row = 0;
  for (let i = 0; i < rowStr.length; i++) {
    row = row * 26 + (rowStr.charCodeAt(i) - 64);
  }
  return { row: row - 1, col: parseInt(match[2]) - 1 };
};

const VISIBLE_ROWS = 7;   // rows shown in the mini map
const VISIBLE_COLS = 20;  // cols per row

const SeatMini = ({ seatLabel, tier }: Props) => {
  const { row, col } = parse(seatLabel);
  const color = TIER_COLOR[tier] || '#3a7abf';

  // Which rows to show: center the user's row
  const rowStart = Math.max(0, row - Math.floor(VISIBLE_ROWS / 2));
  const rowEnd = rowStart + VISIBLE_ROWS;
  const colStart = Math.max(0, col - Math.floor(VISIBLE_COLS / 2));
  const colEnd = colStart + VISIBLE_COLS;

  const rowName = (r: number) => {
    let s = '';
    let n = r + 1;
    do { s = String.fromCharCode(64 + n % 26 || 26) + s; n = Math.floor((n - 1) / 26); } while (n > 0);
    return s;
  };

  return (
    <div className="sm-root">
      {/* Stage */}
      <div className="sm-stage">舞台</div>

      {/* Grid */}
      <div className="sm-grid">
        {Array.from({ length: rowEnd - rowStart }, (_, ri) => {
          const r = rowStart + ri;
          return (
            <div key={r} className="sm-row">
              <span className="sm-row-label">{rowName(r)}</span>
              <div className="sm-seats">
                {Array.from({ length: colEnd - colStart }, (_, ci) => {
                  const c = colStart + ci;
                  const isMe = r === row && c === col;
                  return (
                    <div
                      key={c}
                      className={`sm-seat ${isMe ? 'mine' : ''}`}
                      style={isMe ? { background: color, borderColor: color } : {}}
                      title={isMe ? `${seatLabel} (你的座位)` : ''}
                    >
                      {isMe && <span className="sm-seat-dot" />}
                    </div>
                  );
                })}
              </div>
            </div>
          );
        })}
      </div>

      {/* Legend */}
      <div className="sm-legend">
        <span className="sm-legend-dot" style={{ background: color }} />
        <span>{seatLabel} · {tier}</span>
      </div>
    </div>
  );
};

export default SeatMini;
