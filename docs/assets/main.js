(function () {
  const canvas = document.getElementById("wave-canvas");
  if (!canvas) return;

  const ctx = canvas.getContext("2d");
  let width = 0;
  let height = 0;
  let t = 0;

  function resize() {
    const rect = canvas.getBoundingClientRect();
    width = Math.floor(rect.width * devicePixelRatio);
    height = Math.floor(rect.height * devicePixelRatio);
    canvas.width = width;
    canvas.height = height;
  }

  function drawWave() {
    ctx.clearRect(0, 0, width, height);
    const mid = height * 0.55;
    const amp = height * 0.22;

    // subtle grid
    ctx.strokeStyle = "rgba(255,255,255,0.04)";
    ctx.lineWidth = 1;
    for (let y = 0; y < height; y += height / 6) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(width, y);
      ctx.stroke();
    }

    // markers
    const markers = [
      { start: 0.18, end: 0.28, color: "rgba(251, 191, 36, 0.22)" },
      { start: 0.42, end: 0.44, color: "rgba(251, 113, 133, 0.28)" },
      { start: 0.61, end: 0.66, color: "rgba(110, 231, 255, 0.22)" },
      { start: 0.82, end: 0.86, color: "rgba(251, 191, 36, 0.18)" },
    ];
    for (const m of markers) {
      ctx.fillStyle = m.color;
      ctx.fillRect(m.start * width, height * 0.12, (m.end - m.start) * width, height * 0.76);
    }

    // waveform
    ctx.beginPath();
    for (let x = 0; x < width; x++) {
      const p = x / width;
      const env = 0.35 + 0.65 * Math.abs(Math.sin(p * Math.PI * 1.2 + t * 0.4));
      const speech = Math.sin(p * 42 + t) * 0.35 + Math.sin(p * 17 - t * 0.6) * 0.2;
      const click = p > 0.415 && p < 0.435 ? Math.sin((p - 0.415) * 600) * 0.9 : 0;
      const y = mid - (speech * env + click) * amp;
      if (x === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = "#6ee7ff";
    ctx.lineWidth = 2 * devicePixelRatio;
    ctx.stroke();

  // playhead
    const head = (0.35 + (Math.sin(t * 0.25) + 1) * 0.3) * width;
    ctx.strokeStyle = "rgba(255,255,255,0.55)";
    ctx.lineWidth = 1.5 * devicePixelRatio;
    ctx.beginPath();
    ctx.moveTo(head, 0);
    ctx.lineTo(head, height);
    ctx.stroke();

    t += 0.018;
    requestAnimationFrame(drawWave);
  }

  window.addEventListener("resize", resize);
  resize();
  drawWave();

  document.querySelectorAll("[data-copy]").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const target = document.querySelector(btn.dataset.copy);
      if (!target) return;
      const text = target.textContent.trim();
      try {
        await navigator.clipboard.writeText(text);
        const prev = btn.textContent;
        btn.textContent = "Copied";
        setTimeout(() => { btn.textContent = prev; }, 1200);
      } catch (_) {
        /* ignore */
      }
    });
  });
})();
