"""Scoring for the Protocol Evaluation Bench.

One formula for both deterministic and stochastic simulations:

    epsilon = max(tolerance_abs, tolerance_ratio * |mu_real|)
    x = |mu_sim - mu_real| / max(sigma_real, epsilon)

x is the distance of the simulation's center from the real world's center,
in units of the real world's own spread (Glass's delta). The tolerance
epsilon keeps the denominator well-defined when the real side measures
deterministic (sigma_real = 0) or near-deterministic, making the score
graded rather than a binary pass/fail. The score is read off the metric's
scoring table - first band where x <= threshold - with the table in
[global] as the default and a per-metric override allowed.
"""

import numpy as np

DEFAULT_TOLERANCE_RATIO = 0.005
DEFAULT_TOLERANCE_ABS = 1e-9
DEFAULT_SCORING_TABLE = [
    [0.5, 10],
    [1.0, 9],
    [1.5, 8],
    [2.0, 7],
    [3.0, 5],
    [5.0, 3],
    [10.0, 1],
]
# Above this ratio the floor dominates the denominator for most metrics and
# scores stop discriminating - warn the user rather than silently comply.
HIGH_TOLERANCE_RATIO = 0.05
# Below this many real-world runs the sigma_real estimate is too noisy
# (>= 25% error) to trust band assignment near boundaries.
MIN_REAL_RUNS = 10


def tolerance_floor(mean_real, global_cfg=None):
    """epsilon = max(tolerance_abs, tolerance_ratio * |mean_real|)."""
    global_cfg = global_cfg or {}
    ratio = global_cfg.get("tolerance_ratio", DEFAULT_TOLERANCE_RATIO)
    abs_floor = global_cfg.get("tolerance_abs", DEFAULT_TOLERANCE_ABS)
    return max(abs_floor, ratio * abs(mean_real))


def calculate_score(x, scoring_table):
    """Score out of 10 from the band table: first band where x <= threshold."""
    for threshold, score in sorted(scoring_table, key=lambda row: row[0]):
        if x <= threshold:
            return score
    return 0


def resolve_scoring_table(metric_cfg, global_cfg=None):
    """Metric-level scoring_table overrides the [global] default."""
    global_cfg = global_cfg or {}
    return metric_cfg.get("scoring_table") or global_cfg.get(
        "scoring_table", DEFAULT_SCORING_TABLE
    )


def warn_config(global_cfg=None):
    """Warn on config values that make scores less meaningful."""
    global_cfg = global_cfg or {}
    ratio = global_cfg.get("tolerance_ratio", DEFAULT_TOLERANCE_RATIO)
    if ratio >= HIGH_TOLERANCE_RATIO:
        print(
            f"  WARNING: tolerance_ratio = {ratio} is high - the tolerance floor "
            f"dominates most denominators and scores become less meaningful"
        )


def warn_run_count(n_real):
    if n_real < MIN_REAL_RUNS:
        print(
            f"  WARNING: only {n_real} real-world run(s); below {MIN_REAL_RUNS} the "
            f"sigma_real estimate is too noisy to trust band assignment"
        )


def score_metrics(p_config, real_metrics_per_run, sim_metrics_per_run, global_cfg=None):
    """Scores every metric present in >= min_run_coverage of runs on both
    sides (default 80%); below that it's N/A rather than 0 - a missing
    measurement isn't a zero one. Returns (results, overall_score)."""
    global_cfg = global_cfg or {}
    min_coverage = global_cfg.get("min_run_coverage", 0.8)
    warn_config(global_cfg)
    warn_run_count(len(real_metrics_per_run))

    results = {}
    for metric_cfg in p_config.get("metrics", []):
        name = metric_cfg["name"]
        real_vals = [r[name] for r in real_metrics_per_run if name in r]
        sim_vals = [s[name] for s in sim_metrics_per_run if name in s]

        n_real = len(real_metrics_per_run)
        n_sim = len(sim_metrics_per_run)
        real_coverage = len(real_vals) / n_real if n_real else 0.0
        sim_coverage = len(sim_vals) / n_sim if n_sim else 0.0
        coverage_str = f"{len(real_vals)}/{n_real} real, {len(sim_vals)}/{n_sim} sim"

        available = (
            len(real_vals) > 0
            and len(sim_vals) > 0
            and real_coverage >= min_coverage
            and sim_coverage >= min_coverage
        )

        if not available:
            results[name] = {
                "available": False,
                "unit": metric_cfg.get("unit", ""),
                "coverage": coverage_str,
            }
            continue

        mean_real = float(np.mean(real_vals))
        std_real = float(np.std(real_vals))
        mean_sim = float(np.mean(sim_vals))
        std_sim = float(np.std(sim_vals))

        # The single scoring formula: Glass's delta with a tolerance floor.
        # sigma_real = 0 needs no special case - the floor keeps the
        # denominator positive, so the score stays graded ("within real
        # resolution") instead of a binary exact-match pass/fail.
        epsilon = tolerance_floor(mean_real, global_cfg)
        x = abs(mean_sim - mean_real) / max(std_real, epsilon)
        scoring_table = resolve_scoring_table(metric_cfg, global_cfg)
        score = calculate_score(x, scoring_table)

        results[name] = {
            "available": True,
            "unit": metric_cfg.get("unit", ""),
            "coverage": coverage_str,
            "real": real_vals,
            "sim": sim_vals,
            "mean_real": mean_real,
            "std_real": std_real,
            "mean_sim": mean_sim,
            "std_sim": std_sim,
            "epsilon_used": epsilon,
            "x": x,
            "score": score,
            "scoring_table": scoring_table,
            "max_value": metric_cfg.get("max_value"),
        }

    available_scores = [m["score"] for m in results.values() if m["available"]]
    overall_score = float(np.mean(available_scores)) if available_scores else None
    return results, overall_score
