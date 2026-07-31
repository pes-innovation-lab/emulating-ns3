"""Plotting for the Protocol Evaluation Bench.

- `normal_distribution.png` (per metric): real world as a normal curve with
  the sim's output on it (point-bar if deterministic, second gaussian if
  stochastic); the tolerance band [mu_real ± epsilon] is shaded gray - inside
  it the sim is indistinguishable from the real mean.
- `strip.png` (per metric): raw per-run values as dots.
- `overview.png` (per protocol run): scorecard, one bar per metric colored by
  band with x annotated.
"""

import numpy as np
import matplotlib.pyplot as plt

REAL_COLOR = "#3498db"
SIM_COLOR = "#2ecc71"
TOLERANCE_COLOR = "#bdc3c7"
NA_COLOR = "#95a5a6"

GREEN = "#27ae60"
AMBER = "#f39c12"
RED = "#e74c3c"


def _gaussian_pdf(x, mean, std):
    std = max(std, 1e-9)
    return np.exp(-0.5 * ((x - mean) / std) ** 2) / (std * np.sqrt(2 * np.pi))


def _value_axis_limits(mean_r, disp_std, sim, max_value):
    """Data-adaptive axis for the distribution plots: real curve visible,
    sim kept in frame when close, shown off-scale otherwise (the score is
    annotated either way)."""
    lo = mean_r - 4 * disp_std
    hi = mean_r + 4 * disp_std
    if lo <= sim <= hi:
        lo = min(lo, sim - disp_std)
        hi = max(hi, sim + disp_std)
    if max_value:
        lo, hi = max(lo, 0), min(hi, max_value)
    return lo, hi


def _x_reading(m):
    """Plain-language reading of what x means for this metric."""
    if m["std_real"] < m["epsilon_used"]:
        return "real measured deterministic; distance in resolution (epsilon) units"
    return f"sim sits {m['x']:.3g} real-world std-devs from the real mean"


def plot_normal_distribution(path, m, deterministic):
    """Real world as a normal curve with the sim's position on it. If
    sigma_real < epsilon the measured curve would be invisible, so it's drawn
    at epsilon width with a footnote (the score still uses max(sigma_real,
    epsilon))."""
    mean_r, std_r = m["mean_real"], m["std_real"]
    epsilon, x, score = m["epsilon_used"], m["x"], m["score"]
    sim, std_s = m["mean_sim"], m["std_sim"]

    disp_std = max(std_r, epsilon)
    measured_stable = std_r < epsilon
    lo, hi = _value_axis_limits(mean_r, disp_std, sim, m.get("max_value"))

    fig, ax = plt.subplots(figsize=(8, 5))
    xs = np.linspace(lo, hi, 400)
    ax.plot(xs, _gaussian_pdf(xs, mean_r, disp_std), color=REAL_COLOR, linewidth=2,
            label=f"Real-World N({mean_r:.3g}, {disp_std:.3g})")

    # Tolerance band: inside [mu_real - epsilon, mu_real + epsilon] the sim
    # is indistinguishable from the real mean.
    ax.axvspan(mean_r - epsilon, mean_r + epsilon, color=TOLERANCE_COLOR, alpha=0.35,
               label=f"resolution $\\epsilon$ = {epsilon:.3g}")
    ax.axvline(mean_r, color=REAL_COLOR, linestyle=":", alpha=0.8,
               label=f"$\\mu_{{real}}$ = {mean_r:.3g}")

    if deterministic:
        # Point-bar: where the deterministic sim output sits on the curve.
        on_scale = lo <= sim <= hi
        if on_scale:
            ax.axvline(sim, color=SIM_COLOR, linewidth=3, alpha=0.9,
                       label=f"Sim = {sim:.3g}")
            ax.scatter([sim], [ax.get_ylim()[1] * 0.95], color=SIM_COLOR, zorder=5,
                       marker="o", s=60)
        else:
            side = "left" if sim < lo else "right"
            ax.annotate(
                f"Sim = {sim:.3g} (off-scale)", xy=(lo if side == "left" else hi, 0.02),
                xytext=(0.05 if side == "left" else 0.95, 0.85),
                textcoords="axes fraction", ha="left" if side == "left" else "right",
                fontsize=10, color=SIM_COLOR,
                arrowprops=dict(arrowstyle="->", color=SIM_COLOR),
            )
    else:
        ax.plot(xs, _gaussian_pdf(xs, sim, max(std_s, 1e-9)), color=SIM_COLOR,
                linewidth=2, alpha=0.8,
                label=f"Sim N({sim:.3g}, {std_s:.3g})")

    if measured_stable:
        ax.text(
            0.02, 0.98, f"$\\sigma_{{real}}$ = {std_r:.3g} < $\\epsilon$; curve shown at $\\epsilon$",
            transform=ax.transAxes, ha="left", va="top", fontsize=8.5, color="dimgrey",
        )

    ax.set_xlim(lo, hi)
    ax.set_ylabel("Density", fontsize=11)
    ax.set_xlabel(f"{m['unit']}", fontsize=10)
    ax.set_title(
        f"Real-World Distribution vs Simulation Output (x = {x:.3g} → {score}/10)\n"
        f"{_x_reading(m)}",
        fontsize=12, pad=14,
    )
    ax.grid(linestyle="--", alpha=0.4)
    ax.legend(fontsize=9, loc="upper right")
    plt.tight_layout()
    plt.savefig(path)
    plt.close(fig)


def plot_strip(path, m):
    """Raw per-run values as individual dots - every measurement visible,
    with mean markers and run counts."""
    real_vals, sim_vals = m["real"], m["sim"]
    fig, ax = plt.subplots(figsize=(8, 3.5))

    def dots(vals, y, color):
        if not vals:
            return
        rng = np.random.default_rng(42)
        jitter = rng.uniform(-0.12, 0.12, size=len(vals))
        ax.scatter(vals, [y + j for j in jitter],
                   color=color, alpha=0.7, s=40, edgecolor="white", linewidth=0.4)
        ax.scatter([np.mean(vals)], [y], color=color, marker="D", s=70, zorder=5,
                   edgecolor="black", linewidth=0.6)

    dots(real_vals, 0, REAL_COLOR)
    dots(sim_vals, 1, SIM_COLOR)

    ax.set_yticks([0, 1])
    ax.set_yticklabels([f"Real-World (n={len(real_vals)})", f"ns-3 Sim (n={len(sim_vals)})"])
    ax.set_xlabel(f"{m['unit']}", fontsize=10)
    ax.set_title("Per-Run Measurements", fontsize=12, pad=12)
    ax.grid(axis="x", linestyle="--", alpha=0.4)
    ax.axhline(0, color="black", linewidth=0.5)
    ax.axhline(1, color="black", linewidth=0.5)
    plt.tight_layout()
    plt.savefig(path)
    plt.close(fig)


def plot_overview(path, protocol_name, metric_results):
    """Scorecard: one horizontal bar per metric, colored by band, x and
    score annotated; unavailable metrics shown as gray N/A rows."""
    names = list(metric_results.keys())
    fig, ax = plt.subplots(figsize=(9, max(3.2, 0.55 * len(names) + 1.2)))

    for i, name in enumerate(names):
        m = metric_results[name]
        y = len(names) - 1 - i
        if not m["available"]:
            ax.barh(y, 1.0, color=NA_COLOR, alpha=0.6)
            ax.text(1.02, y, "N/A", va="center", fontsize=9, color="dimgrey")
            ax.text(-0.02, y, f"{name}", va="center", ha="right", fontsize=10)
            continue
        score = m["score"]
        color = GREEN if score >= 8 else AMBER if score >= 5 else RED
        ax.barh(y, score / 10.0, color=color, alpha=0.85)
        ax.text(score / 10.0 + 0.015, y,
                f"{score}/10 (x = {m['x']:.3g})", va="center", fontsize=9)
        ax.text(-0.02, y, f"{name}", va="center", ha="right", fontsize=10)

    ax.set_xlim(0, 1.35)
    ax.set_yticks([])
    ax.set_xlabel("Score / 10", fontsize=11)
    ax.set_title(f"{protocol_name} — Evaluation Scorecard", fontsize=13, pad=14)
    ax.grid(axis="x", linestyle="--", alpha=0.4)
    plt.tight_layout()
    plt.savefig(path)
    plt.close(fig)
