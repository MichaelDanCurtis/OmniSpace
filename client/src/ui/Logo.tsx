export function Logo({ size = 38 }: { size?: number }) {
  return (
    <svg className="brand-mark" width={size} height={size} viewBox="0 0 40 40" style={{ width: size, height: size }}>
      <defs>
        <linearGradient id="lg" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0" stopColor="#e8f4ff" />
          <stop offset="1" stopColor="#7fb8e8" />
        </linearGradient>
      </defs>
      <path d="M20 3 L36 36 L28.5 36 L20 17 L11.5 36 L4 36 Z" fill="url(#lg)" />
      <path d="M20 23 L24 32 L16 32 Z" fill="#8fd0ff" opacity="0.6" />
    </svg>
  )
}
