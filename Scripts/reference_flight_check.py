#!/usr/bin/env python3
"""Dependency-free physics envelope smoke check for Disc Golf Tour.

This is intentionally not an authoritative duplicate of Unreal collision/physics.
It mirrors the major aerodynamic + precession equations closely enough to catch
catastrophic sign errors, NaNs, impossible carry, and handedness regressions.
"""
from __future__ import annotations

from dataclasses import dataclass
import math
import sys
from typing import Tuple

# Enforcement below is written as `assert`, and CPython strips every one of them
# under -O, -OO or PYTHONOPTIMIZE. Without this refusal the script prints its OK
# banner and exits 0 no matter what the solver does: a copy carrying the per-step
# spin-decay bug this file exists to catch reports "12 throws converge, baseline
# carry spread=24.679m" and succeeds. Because Unreal is not installed on a source
# checkout, this script is one of only two tests that run at all, so a false green
# here is worse than no test. Fail closed instead.
if not __debug__:
    raise SystemExit(
        "reference_flight_check requires assertions: re-run without "
        "-O/-OO/PYTHONOPTIMIZE. Every check in this file is an assert and the "
        "optimiser removes them, which would report success unconditionally."
    )

class EnvelopeFailure(AssertionError):
    """An envelope violation, raised explicitly so -O cannot remove it."""


def require(condition, message: str) -> None:
    """Enforce an envelope rule. Unlike assert, a raise survives the optimiser."""
    if not condition:
        raise EnvelopeFailure(message)


V = Tuple[float, float, float]
DT = 1.0 / 240.0
INTEGRATION_LIMIT_S = 30.0
RHO = 1.225
G = 9.80665
RELEASE_PERFECT_ERROR = 0.12


def add(a: V, b: V) -> V: return (a[0]+b[0], a[1]+b[1], a[2]+b[2])
def sub(a: V, b: V) -> V: return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def mul(a: V, s: float) -> V: return (a[0]*s, a[1]*s, a[2]*s)
def dot(a: V, b: V) -> float: return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]
def cross(a: V, b: V) -> V: return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def mag(a: V) -> float: return math.sqrt(dot(a, a))
def norm(a: V, fallback: V=(1.0,0.0,0.0)) -> V:
    m = mag(a)
    return fallback if m < 1e-12 else mul(a, 1.0/m)
def clamp(v: float, lo: float, hi: float) -> float: return max(lo, min(hi, v))


def rotate(v: V, axis: V, angle_rad: float) -> V:
    """Rodrigues rotation in the script's right-handed math frame."""
    k = norm(axis)
    c, s = math.cos(angle_rad), math.sin(angle_rad)
    return add(add(mul(v, c), mul(cross(k, v), s)), mul(k, dot(k, v)*(1.0-c)))


@dataclass
class Aero:
    mass: float = 0.175
    diameter: float = 0.211
    area: float = 0.03496
    inertia_axial: float = 0.000974
    cl0: float = 0.31       # Apex fallback: 0.22 + 0.018*5
    cla: float = 1.97       # Apex fallback: 1.72 + 0.05*5
    cd0: float = 0.080      # speed 12
    cda: float = 0.62
    turn_moment: float = 0.0035
    fade_moment: float = 0.0075
    turn_above: float = 20.0
    fade_below: float = 17.0
    spin_decay: float = 0.032


@dataclass
class Result:
    carry_m: float
    lateral_m: float
    peak_m: float
    flight_s: float
    final_speed_mps: float
    # Why integration stopped. "landed" is the only outcome that describes a
    # complete flight; the other two return a truncated trajectory that looks
    # like a landing unless the caller checks. See INTEGRATION_LIMIT_S.
    termination: str = "landed"


@dataclass
class Release:
    quality: float
    speed_multiplier: float
    spin_multiplier: float
    speed_mps: float
    spin_rpm: float
    aim_offset_deg: float
    hyzer_offset_deg: float
    nose_offset_deg: float
    launch_offset_deg: float


@dataclass
class GroundProfile:
    restitution_scale: float = 1.0
    friction_scale: float = 1.0
    spin_retention: float = 0.88
    skip_min_speed: float = 7.5
    skip_max_incidence_deg: float = 19.0
    max_consecutive_skips: int = 3
    edge_min_angle_deg: float = 55.0
    edge_min_speed: float = 3.0
    edge_min_spin_rpm: float = 150.0
    settle_speed: float = 1.1


@dataclass
class GroundImpact:
    state: str
    velocity_mps: V
    spin_multiplier: float
    incidence_deg: float
    edge_angle_deg: float
    restitution: float
    friction: float


def throw_rotation_sign(*, throw_style: str, handedness: str) -> float:
    """Mirror DiscGolfMath::ThrowRotationSign without conflating style and hand."""
    if throw_style not in {"backhand", "forehand"}:
        raise AssertionError(f"unknown throw style: {throw_style}")
    if handedness not in {"right", "left"}:
        raise AssertionError(f"unknown handedness: {handedness}")
    style_sign = 1.0 if throw_style == "backhand" else -1.0
    handedness_sign = 1.0 if handedness == "right" else -1.0
    return style_sign * handedness_sign


def resolve_release(*, timing_error: float, power: float,
                    throw_style: str = "backhand", handedness: str = "right") -> Release:
    """Mirror the deterministic release-quality boundary used before Unreal flight."""
    rotation_sign = throw_rotation_sign(
        throw_style=throw_style, handedness=handedness)
    timing_error = clamp(timing_error, -1.0, 1.0)
    severity = clamp((abs(timing_error)-RELEASE_PERFECT_ERROR)/(1.0-RELEASE_PERFECT_ERROR), 0.0, 1.0)
    signed_severity = math.copysign(severity, timing_error) if severity > 0.0 else 0.0
    speed_multiplier = 1.0-0.16*severity**1.25
    spin_multiplier = 1.0-0.22*severity**1.15
    min_speed = 8.5 if throw_style == "backhand" else 8.0
    max_speed = 30.5 if throw_style == "backhand" else 27.5
    max_spin = 1050.0 if throw_style == "backhand" else 900.0
    power = clamp(power, 0.0, 1.0)
    return Release(
        quality=1.0-severity,
        speed_multiplier=speed_multiplier,
        spin_multiplier=spin_multiplier,
        speed_mps=(min_speed+(max_speed-min_speed)*power)*speed_multiplier,
        spin_rpm=(300.0+(max_spin-300.0)*power)*spin_multiplier,
        aim_offset_deg=rotation_sign*signed_severity*6.0,
        hyzer_offset_deg=signed_severity*4.0,
        nose_offset_deg=signed_severity*3.0,
        launch_offset_deg=signed_severity*2.0,
    )


def ground_profile(surface: str) -> GroundProfile:
    if surface == "tee":
        return GroundProfile(1.10, 0.72, 0.92, 6.5, 21.0, 4, 54.0, 2.8, 140.0, 1.0)
    if surface == "rough":
        return GroundProfile(0.55, 1.65, 0.60, 12.0, 10.0, 1, 60.0, 4.5, 220.0, 1.5)
    if surface == "dirt":
        return GroundProfile(0.82, 0.90, 0.78, 9.0, 16.0, 2, 57.0, 3.5, 170.0, 1.2)
    if surface == "rock":
        return GroundProfile(1.55, 0.55, 0.93, 5.5, 26.0, 5, 50.0, 2.5, 110.0, 0.9)
    return GroundProfile()


def resolve_ground_impact(*, velocity: V, surface_normal: V, disc_normal: V,
                          spin_rpm: float, base_restitution: float,
                          base_friction: float, surface: str,
                          prior_consecutive_skips: int = 0) -> GroundImpact:
    """Mirror the deterministic impact classifier at the Unreal ground boundary."""
    profile = ground_profile(surface)
    normal = norm(surface_normal, (0.0, 0.0, 1.0))
    safe_disc_normal = norm(disc_normal, (0.0, 0.0, 1.0))
    signed_normal_speed = dot(velocity, normal)
    tangent_velocity = sub(velocity, mul(normal, signed_normal_speed))
    impact_speed = mag(velocity)
    approach_speed = max(-signed_normal_speed, 0.0)
    tangent_speed = mag(tangent_velocity)
    incidence = math.degrees(math.atan2(approach_speed, max(tangent_speed, 1e-4)))
    edge_angle = math.degrees(math.acos(clamp(abs(dot(safe_disc_normal, normal)), 0.0, 1.0)))
    restitution = clamp(base_restitution*profile.restitution_scale, 0.02, 0.72)
    friction = clamp(base_friction*profile.friction_scale, 0.05, 0.95)

    if impact_speed <= profile.settle_speed or tangent_speed <= profile.settle_speed*0.55:
        return GroundImpact("settled", (0.0, 0.0, 0.0), 0.0, incidence, edge_angle, restitution, friction)

    tangent_dir = norm(tangent_velocity)
    if (edge_angle >= profile.edge_min_angle_deg and tangent_speed >= profile.edge_min_speed
            and abs(spin_rpm) >= profile.edge_min_spin_rpm):
        retention = clamp(1.0-friction*0.10, 0.72, 0.98)
        return GroundImpact("edge_roll", mul(tangent_dir, tangent_speed*retention),
                            profile.spin_retention*0.90, incidence, edge_angle, restitution, friction)

    restitution_bias = clamp((restitution-0.08)/0.20, 0.0, 1.0)
    plastic_skip_min = profile.skip_min_speed*(1.12+(0.88-1.12)*restitution_bias)
    has_skip_geometry = (approach_speed >= 0.35 and tangent_speed >= plastic_skip_min
                         and incidence <= profile.skip_max_incidence_deg)
    if has_skip_geometry and prior_consecutive_skips < profile.max_consecutive_skips:
        shallow_weight = 1.0-clamp(incidence/max(profile.skip_max_incidence_deg, 1.0), 0.0, 1.0)
        tangent_retention = clamp(1.0-friction*(0.09+0.04*(1.0-shallow_weight)), 0.68, 0.98)
        skip_lift = clamp(approach_speed*restitution+tangent_speed*0.075*shallow_weight, 0.45, 3.8)
        skip_velocity = add(mul(tangent_dir, tangent_speed*tangent_retention), mul(normal, skip_lift))
        return GroundImpact("skip", skip_velocity, profile.spin_retention*0.96,
                            incidence, edge_angle, restitution, friction)

    slide_retention = clamp(1.0-friction*0.22, 0.55, 0.96)
    if has_skip_geometry and prior_consecutive_skips >= profile.max_consecutive_skips:
        slide_retention *= 0.65
    return GroundImpact("slide", mul(tangent_dir, tangent_speed*slide_retention),
                        profile.spin_retention*0.82, incidence, edge_angle, restitution, friction)


def simulate(*, throw_style: str = "backhand", handedness: str = "right",
             hyzer_deg: float=3.0, power: float=0.82,
             nose_deg: float=1.0, launch_deg: float=7.0, timing_error: float=0.0,
             wind: V=(0.0,0.0,0.0), dt: float=0.0, aero: Aero=None) -> Result:
    # dt is explicit so the fixed-step contract can be exercised directly, and
    # resolves at call time rather than binding DT at definition time so a caller
    # that overrides DT still gets the step it asked for. Exactly 0.0 is the
    # documented sentinel for "use the module step"; anything else non-positive
    # or non-finite is rejected rather than substituted. Silently swapping in DT
    # would let a sign slip in a rate table turn the step sweep into six
    # identical runs of the default step -- which reports convergence while
    # testing nothing, and passes even on a solver carrying the exact
    # frame-rate bug the sweep exists to catch. The authoritative solver
    # band-limits its own step the same way.
    if dt == 0.0:
        dt = DT
    elif not math.isfinite(dt) or dt <= 0.0:
        raise EnvelopeFailure(
            f"simulate() needs a positive finite step; got dt={dt!r}. "
            "Pass 0.0 to use the module step."
        )
    a = aero if aero is not None else Aero()
    rotation_sign = throw_rotation_sign(
        throw_style=throw_style, handedness=handedness)
    release = resolve_release(
        timing_error=timing_error,
        power=power,
        throw_style=throw_style,
        handedness=handedness,
    )
    spin = rotation_sign * release.spin_rpm * 2.0*math.pi/60.0

    # Script frame: +X down-fairway, +Y player-right, +Z up.
    aim_rad = math.radians(release.aim_offset_deg)
    effective_launch_deg = clamp(launch_deg+release.launch_offset_deg, -5.0, 35.0)
    effective_nose_deg = clamp(nose_deg+release.nose_offset_deg, -7.0, 11.0)
    effective_hyzer_deg = clamp(hyzer_deg+release.hyzer_offset_deg, -34.0, 34.0)
    flat = (math.cos(aim_rad), math.sin(aim_rad), 0.0)
    right = norm(cross((0.0,0.0,1.0), flat), (0.0,1.0,0.0))
    launch_rad = math.radians(effective_launch_deg)
    launch = norm((math.cos(launch_rad)*flat[0], math.cos(launch_rad)*flat[1], math.sin(launch_rad)))
    forward = norm(rotate(launch, right, -math.radians(effective_nose_deg)))
    normal = norm(cross(forward, right), (0.0,0.0,1.0))
    normal = norm(rotate(
        normal, forward, rotation_sign*math.radians(effective_hyzer_deg)), normal)

    pos = (0.0, 0.0, 1.5)
    vel = mul(launch, release.speed_mps)
    peak = pos[2]
    t = 0.0

    termination = "time_cap"
    for _ in range(int(INTEGRATION_LIMIT_S/dt)):
        air = sub(vel, wind)
        speed = mag(air)
        if speed < 0.05:
            termination = "stalled"
            break
        uvel = mul(air, 1.0/speed)
        nspd = dot(air, normal)
        vinplane = sub(air, mul(normal, nspd))
        plane_speed = max(mag(vinplane), 1e-4)
        alpha = -math.atan2(nspd, plane_speed)
        uplane = norm(vinplane, forward)
        ulateral = norm(cross(normal, uplane), (0.0,1.0,0.0))
        lift_dir = norm(cross(uvel, ulateral), (0.0,0.0,1.0))

        alpha_eq = -a.cl0/max(a.cla, 1e-4)
        cl = clamp(a.cl0+a.cla*alpha, -0.8, 1.7)
        cd = clamp(a.cd0+a.cda*(alpha-alpha_eq)**2, 0.02, 1.2)
        qarea = 0.5*RHO*a.area*speed*speed
        aero_force = add(mul(lift_dir, cl*qarea), mul(uvel, -cd*qarea))
        accel = add(mul(aero_force, 1.0/a.mass), (0.0,0.0,-G))
        vel = add(vel, mul(accel, dt))

        turn_w = clamp((speed-a.turn_above)/8.0, 0.0, 1.0)
        fade_w = clamp((a.fade_below-speed)/8.0, 0.0, 1.0)
        stability_moment = a.turn_moment*turn_w - a.fade_moment*fade_w
        torque = mul(ulateral, stability_moment)

        angular_momentum = a.inertia_axial*spin
        if abs(angular_momentum) > 1e-5:
            torque_perp = sub(torque, mul(normal, dot(torque, normal)))
            normal_rate = mul(torque_perp, 1.0/angular_momentum)
            rate = mag(normal_rate)
            if rate > 2.5:
                normal_rate = mul(normal_rate, 2.5/rate)
            normal = norm(add(normal, mul(normal_rate, dt)), normal)

        spin *= math.exp(-a.spin_decay*dt)
        preferred_forward = sub(vel, mul(normal, dot(vel, normal)))
        forward = norm(preferred_forward, forward)
        pos = add(pos, mul(vel, dt))
        peak = max(peak, pos[2])
        t += dt

        if not all(math.isfinite(x) for x in (*pos, *vel, *normal, spin)):
            raise AssertionError("non-finite state encountered")
        if pos[2] <= 0.0 and t > 0.2:
            termination = "landed"
            break

    return Result(
        carry_m=max(pos[0], 0.0),
        termination=termination,
        lateral_m=pos[1],
        peak_m=peak,
        flight_s=t,
        final_speed_mps=mag(vel),
    )


# Fixed-step rates the guard sweeps. 30 and 60 bracket the frame rates the
# engine-side regression suite runs at; the finer steps establish the limit the
# coarser ones must be converging toward.
CONVERGENCE_RATES_HZ = (30, 60, 120, 240, 480, 960)

# Bounds are relative to the throw. Short throws converge to a smaller absolute
# spread than long ones, so a single metre-valued limit is either too loose for a
# 22 m pitch or too tight for a 90 m tailwind drive. Measured worst case across
# the swept configurations is 1.28% of carry; 2% with a 1 m floor clears every one
# of them by at least 1.5x while staying far below the metre-scale divergence a
# step-dependent term produces.
CONVERGENCE_CARRY_TOLERANCE_FRACTION = 0.02
CONVERGENCE_CARRY_FLOOR_M = 1.0
CONVERGENCE_LATERAL_TOLERANCE_M = 0.75

# Lateral sign is only meaningful for a throw with a real lateral tendency. A
# throw that finishes near the centreline may cross it at any step size without
# anything being wrong.
CONVERGENCE_LATERAL_SIGN_MIN_M = 1.0

# Convergence is only testable when the coarsest step actually has error to shed.
# Below this the throw is already converged at 30 Hz, and the ratio between two
# sub-centimetre deltas is quantisation noise rather than signal -- an earlier
# version of this guard compared every consecutive pair and fired on 11 of 25
# legitimate configurations for exactly that reason.
CONVERGENCE_FIRST_DELTA_FLOOR_M = 0.20
CONVERGENCE_TAIL_FRACTION = 0.5


# Configurations the step-independence guard sweeps. Each engages a term the
# baseline does not: forehand mirrors the spin sign, the hyzer extremes drive the
# precession torque hardest, low power sits in the fade regime for most of the
# flight while full power sits in the turn regime, and wind enters the relative
# air velocity rather than the integration.
STEP_INDEPENDENCE_CASES = (
    ("Apex RHBH 82% / 3 deg hyzer", dict(power=0.82, hyzer_deg=3.0)),
    ("RHFH 82%", dict(throw_style="forehand", power=0.82, hyzer_deg=3.0)),
    ("LHBH 82%", dict(handedness="left", power=0.82, hyzer_deg=3.0)),
    ("RHBH 34 deg hyzer", dict(power=0.82, hyzer_deg=34.0)),
    ("RHBH 34 deg anhyzer", dict(power=0.82, hyzer_deg=-34.0)),
    ("RHBH nose down 7 deg", dict(power=0.82, nose_deg=-7.0)),
    ("RHBH 20% power", dict(power=0.2)),
    ("RHBH 100% power", dict(power=1.0)),
    ("RHBH 10 m/s tailwind", dict(power=0.82, wind=(10.0, 0.0, 0.0))),
    ("RHBH 10 m/s headwind", dict(power=0.82, wind=(-10.0, 0.0, 0.0))),
    ("RHBH 10 m/s crosswind", dict(power=0.82, wind=(0.0, 10.0, 0.0))),
    ("RHBH worst-case timing miss", dict(power=0.82, timing_error=1.0)),
)


def fixed_step_convergence(*, aero_for_rate=None, **throw):
    """Simulate one throw across the fixed-step sweep, coarsest first.

    aero_for_rate lets a caller vary the disc profile per step size, which is how
    the self-test reproduces a step-dependent solver using the real integrator.
    """
    samples = []
    for hz in CONVERGENCE_RATES_HZ:
        dt = 1.0/hz
        aero = aero_for_rate(dt) if aero_for_rate is not None else None
        samples.append((hz, simulate(dt=dt, aero=aero, **throw)))
    return samples


def assert_step_independent(samples, label: str) -> None:
    """Assert the throw converges as the step shrinks, rather than merely varying.

    AGENTS.md forbids frame-rate-dependent flight. A correctly time-scaled
    integrator answers the same question at every step size, so halving the step
    must move the result by less than the previous halving did and the whole
    sweep must stay inside a narrow band. A term applied per step instead of per
    second passes neither test: its contribution scales with the step count, so
    the spread grows and the deltas never shrink.
    """
    for hz, result in samples:
        require(result.termination == "landed",
                f"{label} at {hz} Hz did not land: "
                f"termination={result.termination}. A truncated flight cannot "
                "be compared against a completed one.")
    carries = [r.carry_m for _, r in samples]
    laterals = [r.lateral_m for _, r in samples]

    spread = max(carries) - min(carries)
    limit = max(CONVERGENCE_CARRY_FLOOR_M,
                CONVERGENCE_CARRY_TOLERANCE_FRACTION*carries[-1])
    require(spread <= limit,
            f"{label} carry varies {spread:.3f} m across "
            f"{CONVERGENCE_RATES_HZ[0]}-{CONVERGENCE_RATES_HZ[-1]} Hz "
            f"(limit {limit:.3f} m): flight depends on step size")

    lateral_spread = max(laterals) - min(laterals)
    require(lateral_spread <= CONVERGENCE_LATERAL_TOLERANCE_M,
            f"{label} lateral varies {lateral_spread:.3f} m across the step "
            f"sweep (limit {CONVERGENCE_LATERAL_TOLERANCE_M} m)")
    mean_lateral = sum(laterals)/len(laterals)
    if abs(mean_lateral) > CONVERGENCE_LATERAL_SIGN_MIN_M:
        require(len({l < 0.0 for l in laterals}) == 1,
                f"{label} lateral tendency changed sign with step size: "
                f"{laterals}")

    # Halving the step must shed error, not merely move the answer. Comparing the
    # finest halving against the coarsest is robust where a pairwise ratio is not:
    # it needs no noise floor of its own and cannot be fooled by two adjacent
    # deltas that are both already negligible.
    deltas = [abs(carries[i+1]-carries[i]) for i in range(len(carries)-1)]
    if deltas[0] >= CONVERGENCE_FIRST_DELTA_FLOOR_M:
        require(deltas[-1] <= CONVERGENCE_TAIL_FRACTION*deltas[0],
                f"{label} is not converging: the finest halving still moved "
                f"carry {deltas[-1]:.4f} m against {deltas[0]:.4f} m for the "
                f"coarsest (limit {CONVERGENCE_TAIL_FRACTION:g} of it). Deltas: "
                + ", ".join(f"{d:.4f}" for d in deltas))



# Handedness is a reflection of the whole solver through the fairway centreline,
# not a lateral sign flip applied to the result. Mirroring the crosswind with the
# hand is what makes it exact: the reflected problem needs a reflected wind. The
# existing flat-mirror assertions are all taken at hyzer_deg 0, where the attitude
# rotation is the identity, so a handedness fork in the attitude path is invisible
# to them.
REFLECTION_CASES = (
    ("RHBH baseline", dict(power=0.82, hyzer_deg=3.0)),
    ("RHFH baseline", dict(throw_style="forehand", power=0.82, hyzer_deg=3.0)),
    ("hyzer clamp", dict(power=0.82, hyzer_deg=34.0)),
    ("anhyzer clamp", dict(power=0.82, hyzer_deg=-34.0)),
    ("forehand anhyzer", dict(throw_style="forehand", power=1.0, hyzer_deg=-34.0)),
    ("20% power", dict(power=0.2, hyzer_deg=3.0)),
    ("crosswind", dict(power=0.82, hyzer_deg=3.0, wind=(0.0, 10.0, 0.0))),
    ("headwind", dict(power=0.82, hyzer_deg=3.0, wind=(-10.0, 0.0, 0.0))),
    ("oblique wind", dict(power=0.82, hyzer_deg=-12.0, wind=(-8.0, 4.0, 2.0))),
    ("worst timing miss", dict(power=0.82, hyzer_deg=3.0, timing_error=1.0)),
)

# Deviation is exactly zero on every field today, so this is entirely headroom
# against a future reordering of the floating-point arithmetic.
REFLECTION_TOLERANCE = 1e-9


def assert_handedness_reflection() -> None:
    for label, throw in REFLECTION_CASES:
        wind = throw.get("wind", (0.0, 0.0, 0.0))
        mirrored = dict(throw)
        mirrored["wind"] = (wind[0], -wind[1], wind[2])
        right = simulate(handedness="right", **throw)
        left = simulate(handedness="left", **mirrored)
        require(right.termination == left.termination,
                f"{label} terminated differently by hand: "
                f"{right.termination} vs {left.termination}")
        for field, a, b in (
                ("carry", right.carry_m, left.carry_m),
                ("peak", right.peak_m, left.peak_m),
                ("flight time", right.flight_s, left.flight_s),
                ("final speed", right.final_speed_mps, left.final_speed_mps),
        ):
            require(abs(a-b) <= REFLECTION_TOLERANCE,
                    f"{label} {field} changed with handedness: {a} vs {b}. "
                    "Handedness must mirror through spin, not alter the throw.")
        require(abs(right.lateral_m + left.lateral_m) <= REFLECTION_TOLERANCE,
                f"{label} lateral did not mirror: {right.lateral_m} vs "
                f"{left.lateral_m}; they must sum to zero")


# Turn and fade must oppose. Isolating each against a neutral run and requiring
# the two lateral deviations to have non-positive product is a statement about
# sign, so there is no tolerance to choose. The <= form is deliberate: an
# overstable recalibration whose turn window sits above the release ceiling
# contributes exactly zero turn, which must stay legal.
OPPOSITION_CASES = (
    ("RHBH full power flat", dict(power=1.0, hyzer_deg=0.0)),
    ("Apex RHBH baseline", dict(power=0.82, hyzer_deg=3.0)),
    ("RHFH full power flat", dict(throw_style="forehand", power=1.0, hyzer_deg=0.0)),
    ("LHBH full power flat", dict(handedness="left", power=1.0, hyzer_deg=0.0)),
)


def assert_turn_and_fade_oppose() -> None:
    seed = Aero()
    for label, throw in OPPOSITION_CASES:
        neutral = simulate(aero=Aero(turn_moment=0.0, fade_moment=0.0), **throw)
        turn_only = simulate(aero=Aero(fade_moment=0.0), **throw)
        fade_only = simulate(aero=Aero(turn_moment=0.0), **throw)
        turn_delta = turn_only.lateral_m - neutral.lateral_m
        fade_delta = fade_only.lateral_m - neutral.lateral_m
        require(turn_delta*fade_delta <= 0.0,
                f"{label}: turn and fade push the same way "
                f"(turn {turn_delta:+.3f} m, fade {fade_delta:+.3f} m). They are "
                "opposing stability effects and must never collapse into one "
                f"same-sign curve. Seed moments are {seed.turn_moment} turn / "
                f"{seed.fade_moment} fade.")


# The ground model resolves contact geometry, not world geometry. Rotating the
# whole problem must rotate the answer and change nothing else.
GROUND_EQUIVARIANCE_TOLERANCE = 1e-9

GROUND_EQUIVARIANCE_CASES = (
    ((10.0, 0.0, -1.5), (0.0, 0.0, 1.0), (0.0, 0.0, 1.0), 700.0, "fairway"),
    ((10.0, 0.0, -1.5), (0.0, 0.0, 1.0), (0.0, 0.0, 1.0), 700.0, "rough"),
    ((7.0, 0.0, -2.0), (0.0, 0.0, 1.0), (0.0, 1.0, 0.0), 500.0, "fairway"),
    ((12.0, -3.0, -4.0), (0.08, -0.05, 1.0), (0.2, 0.1, 0.97), -640.0, "fairway"),
    ((4.0, 1.0, -0.8), (-0.1, 0.06, 1.0), (0.0, 0.3, 0.95), 300.0, "rough"),
)

# Arbitrary axes, not only world Z: a term that leaks world X or world Z into the
# response survives a yaw-only test.
GROUND_EQUIVARIANCE_ROTATIONS = (
    ((0.0, 0.0, 1.0), 0.7),
    ((1.0, 0.0, 0.0), 0.4),
    ((0.3, -0.8, 0.5), 2.1),
    ((-0.6, 0.2, 0.77), -1.3),
)


def assert_ground_frame_equivariance() -> None:
    for velocity, surface_normal, disc_normal, spin_rpm, surface in GROUND_EQUIVARIANCE_CASES:
        plain = resolve_ground_impact(
            velocity=velocity, surface_normal=surface_normal,
            disc_normal=disc_normal, spin_rpm=spin_rpm,
            base_restitution=0.16, base_friction=0.46, surface=surface)
        for axis, angle in GROUND_EQUIVARIANCE_ROTATIONS:
            turned = resolve_ground_impact(
                velocity=rotate(velocity, axis, angle),
                surface_normal=rotate(surface_normal, axis, angle),
                disc_normal=rotate(disc_normal, axis, angle),
                spin_rpm=spin_rpm, base_restitution=0.16, base_friction=0.46,
                surface=surface)
            require(turned.state == plain.state,
                    f"ground state changed with world orientation on {surface}: "
                    f"{plain.state} became {turned.state}")
            expected = rotate(plain.velocity_mps, axis, angle)
            error = max(abs(a-b) for a, b in zip(expected, turned.velocity_mps))
            require(error <= GROUND_EQUIVARIANCE_TOLERANCE,
                    f"ground response is not frame-equivariant on {surface}: "
                    f"rotating the impact moved the outgoing velocity by "
                    f"{error:.3e} m/s. Something is being measured against a "
                    "world axis instead of the surface normal.")
            for field in ("spin_multiplier", "incidence_deg", "edge_angle_deg",
                          "restitution", "friction"):
                a = getattr(plain, field)
                b = getattr(turned, field)
                require(abs(a-b) <= GROUND_EQUIVARIANCE_TOLERANCE,
                        f"ground {field} changed with world orientation on "
                        f"{surface}: {a} vs {b}")


# A disc arriving straight down the surface normal has 90 degrees of incidence by
# definition, whatever the surface tilt or approach speed, and must leave along
# that normal with no tangential component. This pins the normal/tangent split
# itself: a scale error there is frame-equivariant, so the rotation test above
# cannot see it, yet it moves the outgoing velocity on virtually every oblique
# impact.
NORMAL_INCIDENCE_TOLERANCE_DEG = 0.01
NORMAL_INCIDENCE_TANGENT_TOLERANCE = 1e-9

NORMAL_INCIDENCE_CASES = (
    ((0.0, 0.0, 1.0), 12.0),
    ((0.2, -0.15, 1.0), 12.0),
    ((0.2, -0.15, 1.0), 4.0),
    ((-0.25, 0.1, 1.0), 8.0),
)


def assert_ground_normal_incidence() -> None:
    for tilt, speed in NORMAL_INCIDENCE_CASES:
        normal = norm(tilt)
        impact = resolve_ground_impact(
            velocity=mul(normal, -speed), surface_normal=normal,
            disc_normal=normal, spin_rpm=0.0, base_restitution=0.16,
            base_friction=0.46, surface="fairway")
        require(abs(impact.incidence_deg - 90.0) <= NORMAL_INCIDENCE_TOLERANCE_DEG,
                f"a head-on impact at {speed} m/s on normal {normal} reported "
                f"{impact.incidence_deg:.4f} deg of incidence, not 90. The "
                "normal/tangent split is wrong.")
        tangential = sub(impact.velocity_mps,
                         mul(normal, dot(impact.velocity_mps, normal)))
        require(mag(tangential) <= NORMAL_INCIDENCE_TANGENT_TOLERANCE,
                f"a head-on impact at {speed} m/s left {mag(tangential):.6f} m/s "
                "of tangential velocity; it must leave along the normal.")


# Release-quality shape: the perfect band must be a genuine dead zone, the penalty
# must never reward a worse release, and neither handedness nor style may fork a
# magnitude channel.
RELEASE_SHAPE_TOLERANCE = 1e-12
RELEASE_GRID = tuple(i/240.0 for i in range(241))


def assert_release_shape() -> None:
    for power in (0.2, 0.82, 1.0):
        for style in ("backhand", "forehand"):
            previous = None
            for timing_error in RELEASE_GRID:
                current = resolve_release(
                    timing_error=timing_error, power=power, throw_style=style)
                if previous is not None:
                    for field in ("speed_multiplier", "spin_multiplier",
                                  "speed_mps", "spin_rpm"):
                        before = getattr(previous, field)
                        after = getattr(current, field)
                        require(after <= before + RELEASE_SHAPE_TOLERANCE,
                                f"{style} {field} rose from {before} to {after} "
                                f"as timing error grew to {timing_error:.4f}: a "
                                "worse release was rewarded")
                previous = current

            for timing_error in (0.0, 0.31, 0.62, 1.0):
                right = resolve_release(timing_error=timing_error, power=power,
                                        throw_style=style, handedness="right")
                left = resolve_release(timing_error=timing_error, power=power,
                                       throw_style=style, handedness="left")
                for field in ("quality", "speed_multiplier", "spin_multiplier",
                              "speed_mps", "spin_rpm"):
                    a = getattr(right, field)
                    b = getattr(left, field)
                    require(abs(a-b) <= RELEASE_SHAPE_TOLERANCE,
                            f"{style} {field} forked by handedness at timing "
                            f"error {timing_error}: {a} vs {b}")
                require(abs(right.aim_offset_deg + left.aim_offset_deg)
                        <= RELEASE_SHAPE_TOLERANCE,
                        f"{style} aim offset did not mirror by handedness at "
                        f"timing error {timing_error}")

    # Style sets different speed and spin ceilings, but it must not reshape the
    # penalty. Compare the shape channels only -- speed_mps and spin_rpm carry the
    # ceilings and legitimately differ.
    for power in (0.2, 0.82, 1.0):
        for timing_error in (0.0, 0.2, 0.55, 1.0):
            backhand = resolve_release(timing_error=timing_error, power=power,
                                       throw_style="backhand")
            forehand = resolve_release(timing_error=timing_error, power=power,
                                       throw_style="forehand")
            for field in ("quality", "speed_multiplier", "spin_multiplier",
                          "hyzer_offset_deg", "nose_offset_deg",
                          "launch_offset_deg"):
                a = getattr(backhand, field)
                b = getattr(forehand, field)
                require(abs(a-b) <= RELEASE_SHAPE_TOLERANCE,
                        f"{field} was reshaped by throw style at timing error "
                        f"{timing_error}: backhand {a} vs forehand {b}. Style "
                        "may change the ceilings, not the penalty shape.")
            require(abs(backhand.aim_offset_deg + forehand.aim_offset_deg)
                    <= RELEASE_SHAPE_TOLERANCE,
                    f"aim offset did not mirror across throw style at timing "
                    f"error {timing_error}")

    # Inside the perfect band the release is unpenalized, so the solver must not
    # see the timing error at all. This catches a timing term wired straight into
    # the flight model rather than through the release.
    baseline = simulate(power=0.82, hyzer_deg=3.0, timing_error=0.0)
    for fraction in (-1.0, -0.5, 0.0, 0.5, 1.0):
        timing_error = fraction*RELEASE_PERFECT_ERROR
        inside = simulate(power=0.82, hyzer_deg=3.0, timing_error=timing_error)
        for field, a, b in (
                ("carry", baseline.carry_m, inside.carry_m),
                ("lateral", baseline.lateral_m, inside.lateral_m),
                ("peak", baseline.peak_m, inside.peak_m),
                ("flight time", baseline.flight_s, inside.flight_s),
        ):
            require(abs(a-b) <= REFLECTION_TOLERANCE,
                    f"timing error {timing_error:+.4f} is inside the perfect "
                    f"band but changed {field}: {a} vs {b}. Timing must reach "
                    "the solver only through the release.")


# Wind sign. A crosswind toward +Y must leave the disc further toward +Y than the
# opposite crosswind does. Deliberately narrow -- only the two named baselines --
# because the ordering legitimately reverses for throws whose own fade dominates.
CROSSWIND_MIN_SPREAD_M = 5.0


def assert_crosswind_sign() -> None:
    for style in ("backhand", "forehand"):
        toward = simulate(throw_style=style, power=0.82, hyzer_deg=3.0,
                          wind=(0.0, 10.0, 0.0))
        against = simulate(throw_style=style, power=0.82, hyzer_deg=3.0,
                           wind=(0.0, -10.0, 0.0))
        require(toward.termination == "landed" and against.termination == "landed",
                f"{style} crosswind cases did not land")
        spread = toward.lateral_m - against.lateral_m
        require(spread >= CROSSWIND_MIN_SPREAD_M,
                f"{style} crosswind pushed the wrong way: +10 m/s toward +Y "
                f"finished at {toward.lateral_m:.2f} m against "
                f"{against.lateral_m:.2f} m for -10 m/s (spread {spread:.2f} m, "
                f"minimum {CROSSWIND_MIN_SPREAD_M} m). Check the sign of the "
                "relative airflow.")


def self_test() -> None:
    """Prove the step-independence guard fails on a step-dependent solver.

    Spin decay is applied per second. Reapplying it per step instead is the exact
    mistake the guard exists to catch, so the guard must reject it.
    """
    for label, throw in STEP_INDEPENDENCE_CASES:
        assert_step_independent(fixed_step_convergence(**throw), label)

    # The solver applies decay as exp(-k*dt) per step, so over a flight of length
    # T it retains exp(-k*T) regardless of step size. Applying the same decay once
    # per step instead would retain exp(-k*T/dt) -- the classic frame-rate bug.
    # Setting k to original/dt for each rate reproduces exactly that: every step
    # then decays by a constant exp(-original), independent of how long it lasts.
    seed = Aero()
    faults = (
        # Spin decay: applied as exp(-k*dt) per step, so a flight of length T
        # retains exp(-k*T) at any step size. Once per step instead retains
        # exp(-k*T/dt), and passing k/dt reproduces exactly that.
        ("per-step spin decay",
         lambda dt: Aero(spin_decay=seed.spin_decay/dt)),
        # Precession: the attitude update integrates normal_rate*dt. Applying
        # normal_rate once per step is the same mistake in the term that steers
        # turn and fade, and it corrupts lateral rather than carry first.
        ("per-step precession",
         lambda dt: Aero(turn_moment=seed.turn_moment/dt,
                         fade_moment=seed.fade_moment/dt)),
    )

    checks = 0
    for name, aero_for_rate in faults:
        samples = fixed_step_convergence(
            aero_for_rate=aero_for_rate, power=0.82, hyzer_deg=3.0)
        try:
            assert_step_independent(samples, f"injected {name}")
        except (EnvelopeFailure, AssertionError):
            checks += 1
        else:
            carries = [r.carry_m for _, r in samples]
            raise AssertionError(
                f"guard accepted a {name}; carries were "
                + ", ".join(f"{c:.3f}" for c in carries)
            )

    assert checks == len(faults), "self-test did not exercise the guard"
    print(f"REFERENCE FLIGHT STEP-INDEPENDENCE SELF-TEST PASS: "
          f"{checks}/{len(faults)}")


def check() -> None:
    baseline = simulate(throw_style="backhand", handedness="right", hyzer_deg=3.0)
    rhbh_flat = simulate(throw_style="backhand", handedness="right", hyzer_deg=0.0)
    lhbh_flat = simulate(throw_style="backhand", handedness="left", hyzer_deg=0.0)
    rhfh_flat = simulate(throw_style="forehand", handedness="right", hyzer_deg=0.0)
    lhfh_flat = simulate(throw_style="forehand", handedness="left", hyzer_deg=0.0)
    perfect = resolve_release(timing_error=0.0, power=0.82)
    early = resolve_release(timing_error=-0.75, power=0.82)
    late = resolve_release(timing_error=0.75, power=0.82)
    left_backhand_late = resolve_release(
        timing_error=0.75, power=0.82,
        throw_style="backhand", handedness="left")
    fairway_skip = resolve_ground_impact(
        velocity=(10.0,0.0,-1.5), surface_normal=(0.0,0.0,1.0), disc_normal=(0.0,0.0,1.0),
        spin_rpm=700.0, base_restitution=0.16, base_friction=0.46, surface="fairway")
    rough_landing = resolve_ground_impact(
        velocity=(10.0,0.0,-1.5), surface_normal=(0.0,0.0,1.0), disc_normal=(0.0,0.0,1.0),
        spin_rpm=700.0, base_restitution=0.16, base_friction=0.46, surface="rough")
    edge_roll = resolve_ground_impact(
        velocity=(7.0,0.0,-2.0), surface_normal=(0.0,0.0,1.0), disc_normal=(0.0,1.0,0.0),
        spin_rpm=500.0, base_restitution=0.16, base_friction=0.46, surface="fairway")
    base_borderline = resolve_ground_impact(
        velocity=(7.4,0.0,-1.0), surface_normal=(0.0,0.0,1.0), disc_normal=(0.0,0.0,1.0),
        spin_rpm=650.0, base_restitution=0.16*0.72, base_friction=0.46*1.18, surface="fairway")
    crystal_borderline = resolve_ground_impact(
        velocity=(7.4,0.0,-1.0), surface_normal=(0.0,0.0,1.0), disc_normal=(0.0,0.0,1.0),
        spin_rpm=650.0, base_restitution=0.16*1.20, base_friction=0.46*0.86, surface="fairway")
    skip_budget_landing = resolve_ground_impact(
        velocity=(10.0,0.0,-1.5), surface_normal=(0.0,0.0,1.0), disc_normal=(0.0,0.0,1.0),
        spin_rpm=700.0, base_restitution=0.16, base_friction=0.46, surface="fairway",
        prior_consecutive_skips=ground_profile("fairway").max_consecutive_skips)

    # Wide guard rails: this catches catastrophic regressions without pretending
    # the fallback coefficients have already been calibrated to measured discs.
    assert 55.0 <= baseline.carry_m <= 125.0, f"baseline carry outside prototype envelope: {baseline.carry_m:.1f} m"
    assert 2.0 <= baseline.peak_m <= 30.0, f"baseline peak outside prototype envelope: {baseline.peak_m:.1f} m"
    assert 2.0 <= baseline.flight_s <= 15.0, f"baseline flight time outside prototype envelope: {baseline.flight_s:.1f} s"

    # Each style must mirror laterally by hand without changing that style's
    # speed/spin magnitude. Style remains an independent performance input.
    assert rhbh_flat.lateral_m * lhbh_flat.lateral_m < 0.0, (
        "backhand handedness did not mirror lateral tendency: "
        f"RHBH={rhbh_flat.lateral_m:.1f}m LHBH={lhbh_flat.lateral_m:.1f}m"
    )
    assert rhfh_flat.lateral_m * lhfh_flat.lateral_m < 0.0, (
        "forehand handedness did not mirror lateral tendency: "
        f"RHFH={rhfh_flat.lateral_m:.1f}m LHFH={lhfh_flat.lateral_m:.1f}m"
    )
    assert math.isclose(
        abs(rhbh_flat.lateral_m), abs(lhbh_flat.lateral_m), rel_tol=0.0, abs_tol=0.05
    ), "backhand mirror changed lateral magnitude"
    assert math.isclose(
        abs(rhfh_flat.lateral_m), abs(lhfh_flat.lateral_m), rel_tol=0.0, abs_tol=0.05
    ), "forehand mirror changed lateral magnitude"
    assert math.isclose(perfect.speed_mps, resolve_release(
        timing_error=0.0, power=0.82, handedness="left").speed_mps, abs_tol=1e-6), (
        "backhand release magnitude changed with handedness"
    )
    assert math.isclose(late.aim_offset_deg, -left_backhand_late.aim_offset_deg, abs_tol=1e-6), (
        "backhand timing aim did not mirror with handedness"
    )

    # Release quality must preserve the calibrated perfect baseline, penalize
    # magnitude symmetrically, and make early/late angle errors oppose.
    assert math.isclose(perfect.speed_mps, 26.54, abs_tol=1e-6), "perfect release changed baseline speed"
    assert math.isclose(perfect.spin_rpm, 915.0, abs_tol=1e-6), "perfect release changed baseline spin"
    assert math.isclose(early.speed_mps, late.speed_mps, abs_tol=1e-6), "early/late speed penalty is asymmetric"
    assert math.isclose(early.spin_rpm, late.spin_rpm, abs_tol=1e-6), "early/late spin penalty is asymmetric"
    assert math.isclose(early.aim_offset_deg, -late.aim_offset_deg, abs_tol=1e-6), "early/late aim does not oppose"
    assert 0.84 <= late.speed_multiplier < 1.0, "release speed penalty escaped bounds"
    assert 0.78 <= late.spin_multiplier < 1.0, "release spin penalty escaped bounds"

    # The ground classifier must respond to impact geometry, surface, and plastic
    # without branching into separate handedness implementations.
    assert fairway_skip.state == "skip" and fairway_skip.velocity_mps[2] > 0.0, "shallow fairway skip regressed"
    assert rough_landing.state == "slide", "rough no longer suppresses the reference skip"
    assert edge_roll.state == "edge_roll", "edge-on impact no longer starts a roll"
    assert base_borderline.state == "slide" and crystal_borderline.state == "skip", "plastic skip threshold regressed"
    assert skip_budget_landing.state == "slide", "fairway skip budget no longer suppresses low-hop chatter"
    assert mag(skip_budget_landing.velocity_mps) < 7.1, "skip-out slide retained too much rebound energy"
    negative_spin = resolve_ground_impact(
        velocity=(10.0,0.0,-1.5), surface_normal=(0.0,0.0,1.0), disc_normal=(0.0,0.0,1.0),
        spin_rpm=-700.0, base_restitution=0.16, base_friction=0.46, surface="fairway")
    assert negative_spin.state == fairway_skip.state and negative_spin.velocity_mps == fairway_skip.velocity_mps, (
        "ground response forked on spin sign"
    )

    # One throw proves the integrator is time-scaled for one trajectory. These
    # cover the terms that only engage elsewhere: the opposing turn/fade weights
    # at high and low speed, forehand's mirrored spin, the hyzer and nose clamps,
    # and the wind term, which enters the aerodynamic force rather than the
    # integration and so fails differently.
    convergence = None
    for label, throw in STEP_INDEPENDENCE_CASES:
        samples = fixed_step_convergence(**throw)
        assert_step_independent(samples, label)
        if convergence is None:
            convergence = samples
    carries = [r.carry_m for _, r in convergence]

    assert_handedness_reflection()
    assert_turn_and_fade_oppose()
    assert_ground_frame_equivariance()
    assert_ground_normal_incidence()
    assert_release_shape()
    assert_crosswind_sign()

    print("Reference flight envelope OK")
    print(f"  Apex RHBH 82% / 3 deg hyzer: carry={baseline.carry_m:.1f}m ({baseline.carry_m*3.28084:.0f}ft), peak={baseline.peak_m:.1f}m, flight={baseline.flight_s:.2f}s")
    print(
        "  Flat mirror: "
        f"RHBH/LHBH={rhbh_flat.lateral_m:.1f}/{lhbh_flat.lateral_m:.1f}m, "
        f"RHFH/LHFH={rhfh_flat.lateral_m:.1f}/{lhfh_flat.lateral_m:.1f}m"
    )
    print(f"  Release model: perfect={perfect.speed_mps:.2f}m/s, 75% miss={late.speed_mps:.2f}m/s, aim={late.aim_offset_deg:+.2f}deg")
    print(f"  Ground model: fairway={fairway_skip.state}, rough={rough_landing.state}, edge={edge_roll.state}, base/crystal={base_borderline.state}/{crystal_borderline.state}")
    print(
        f"  Fixed step {CONVERGENCE_RATES_HZ[0]}-{CONVERGENCE_RATES_HZ[-1]} Hz: "
        f"{len(STEP_INDEPENDENCE_CASES)} throws converge, "
        f"baseline carry spread={max(carries)-min(carries):.3f}m"
    )
    print(
        f"  Invariants: handedness reflection {len(REFLECTION_CASES)}, "
        f"turn/fade opposition {len(OPPOSITION_CASES)}, ground equivariance "
        f"{len(GROUND_EQUIVARIANCE_CASES)}x{len(GROUND_EQUIVARIANCE_ROTATIONS)}, "
        f"normal incidence {len(NORMAL_INCIDENCE_CASES)}, "
        "release shape, crosswind sign"
    )


if __name__ == "__main__":
    try:
        if "--self-test" in sys.argv:
            self_test()
            sys.exit(0)
        check()
    except AssertionError as exc:
        print(f"Reference flight envelope FAILED: {exc}")
        sys.exit(1)
