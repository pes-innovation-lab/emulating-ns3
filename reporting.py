"""Report generation for the Protocol Evaluation Bench.

Lays out one run directory:

    {output_dir}/{protocol}_{timestamp}/
        data.json
        report.md
        overview.png
        metric/{name}/{normal_distribution,strip}.png
"""

import datetime
import json
import os

from plotting import plot_normal_distribution, plot_overview, plot_strip


def generate_report(
    protocol,
    p_config,
    metric_results,
    overall_score,
    output_dir,
    min_run_coverage=0.8,
    deterministic=True,
):
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir = os.path.join(output_dir, f"{protocol}_{timestamp}")
    metric_dir = os.path.join(run_dir, "metric")
    os.makedirs(metric_dir, exist_ok=True)

    available = {k: v for k, v in metric_results.items() if v["available"]}
    unavailable = [k for k, v in metric_results.items() if not v["available"]]

    # JSON data
    data = {
        "protocol": protocol,
        "timestamp": timestamp,
        "deterministic": deterministic,
        "metrics": metric_results,
        "overall_score": overall_score,
    }
    json_path = os.path.join(run_dir, "data.json")
    with open(json_path, "w") as f:
        json.dump(data, f, indent=4)
    print(f"Saved raw JSON data to: {json_path}")

    # Plots: scorecard + two images per available metric
    plot_paths = {}
    for name, m in available.items():
        subdir = os.path.join(metric_dir, name)
        os.makedirs(subdir, exist_ok=True)
        plot_paths[name] = {}
        for kind, fn, needs_det in [
            ("normal_distribution", plot_normal_distribution, True),
            ("strip", plot_strip, False),
        ]:
            # Relative to run_dir: report.md lives inside it, so image
            # refs must resolve from there.
            rel = os.path.join("metric", name, f"{kind}.png")
            path = os.path.join(run_dir, rel)
            if needs_det:
                fn(path, m, deterministic)
            else:
                fn(path, m)
            plot_paths[name][kind] = rel

    overview_path = os.path.join(run_dir, "overview.png")
    plot_overview(overview_path, p_config["name"], metric_results)
    print(f"Saved {1 + 2 * len(available)} plots to: {run_dir}")

    # Markdown report
    md_path = os.path.join(run_dir, "report.md")

    metrics_table = ""
    for name, m in metric_results.items():
        if m["available"]:
            metrics_table += (
                f"| {name} | {m['unit']} | {m['mean_real']:.4f} ± {m['std_real']:.4f} | "
                f"{m['mean_sim']:.4f} ± {m['std_sim']:.4f} | {m['x']:.4f} | "
                f"{m['score']}/10 | {m['coverage']} |\n"
            )
        else:
            metrics_table += (
                f"| {name} | {m['unit']} | N/A | N/A | N/A | N/A | {m['coverage']} |\n"
            )

    overall_str = f"{overall_score:.2f}/10" if overall_score is not None else "N/A"
    min_cov_pct = min_run_coverage * 100
    sim_mode = "deterministic (single run)" if deterministic else "stochastic (mean of runs)"

    md_content = f"""# Protocol Evaluation Report: {p_config["name"]}

**Timestamp:** {datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
**Overall Score:** {overall_str}
**Simulation mode:** {sim_mode}

## Overview

![Scorecard](overview.png)

A metric is scored only when present in at least {min_cov_pct:.0f}% of
recorded runs on both the real-world and ns-3 sides; otherwise it's
reported N/A rather than guessed.

| Metric | Unit | Real-World (mean ± std) | ns-3 Sim (mean ± std) | x | Score | Coverage |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
{metrics_table}
"""
    if unavailable:
        md_content += f"\n**Not scored (below run-coverage threshold on at least one side):** {', '.join(unavailable)}\n"

    md_content += """
## Scoring

- **x:** `|mu_sim - mu_real| / max(sigma_real, epsilon)`, where
  `epsilon = max(tolerance_abs, tolerance_ratio * |mu_real|)` - the
  standardized distance of the sim's center from the real center, in
  real-world spread units (Glass's delta). The epsilon floor keeps the
  denominator well-defined when the real side measures deterministic
  (sigma_real = 0) or near-deterministic; a deterministic real baseline
  scores graded (distance in epsilon units), not as a binary pass/fail.
  Both tolerances may be overridden per metric in `config.toml`.
- **Score:** the metric's scoring table read at x (first band where
  x <= threshold).
- **Overall score:** mean of per-metric scores.
"""
    for name, m in available.items():
        md_content += (
            f"\n### {name}\n\n"
            f"![Real-world distribution vs sim output]({plot_paths[name]['normal_distribution']})\n\n"
            f"![Per-run measurements]({plot_paths[name]['strip']})\n\n"
        )
        md_content += f"**Scoring table**\n| Max x | Score |\n| :--- | :--- |\n"
        for threshold, scr in sorted(m["scoring_table"], key=lambda row: row[0]):
            mark = " (Selected)" if m["x"] <= threshold and m["score"] == scr else ""
            md_content += f"| $\\le$ {threshold} | {scr}/10{mark} |\n"

    with open(md_path, "w") as f:
        f.write(md_content)
    print(f"Saved Markdown report to: {md_path}")

    print("\n" + "=" * 50)
    print(f"EVALUATION SUMMARY: {p_config['name']}")
    for name, m in metric_results.items():
        if m["available"]:
            print(
                f"{name}: real {m['mean_real']:.4f} ± {m['std_real']:.4f} {m['unit']} | "
                f"sim {m['mean_sim']:.4f} ± {m['std_sim']:.4f} {m['unit']} | "
                f"x={m['x']:.4f} | score {m['score']}/10"
            )
        else:
            print(f"{name}: N/A (coverage {m['coverage']}, below threshold)")
    print(f"Overall Score: {overall_str}")
    print("=" * 50 + "\n")

    return run_dir
