"""Editor bootstrap: creates the Space map and the base materials.

Run (Mac):
  "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" OmniSpace.uproject -run=pythonscript -script="Tools/ue_setup.py"
Safe to re-run; it only creates what is missing unless FORCE=1.
"""
import os
import unreal

FORCE = os.environ.get("FORCE") == "1"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"[omnispace-setup] {msg}")


def ensure_material(name, build):
    path = f"/Game/Materials/{name}"
    if EAL.does_asset_exist(path) and not FORCE:
        log(f"exists {path}")
        return unreal.load_asset(path)
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mat = AT.create_asset(name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    build(mat)
    MEL.recompile_material(mat)
    EAL.save_asset(path)
    log(f"created {path}")
    return mat


def vec_param(mat, name, default, x, y):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(*default))
    return n


def scalar_param(mat, name, default, x, y):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", default)
    return n


def custom(mat, code, inputs, x, y, out_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    n.set_editor_property("code", code)
    n.set_editor_property("output_type", out_type)
    ins = []
    for name in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    n.set_editor_property("inputs", ins)
    n.set_editor_property("include_file_paths", ["/Project/noise.ush"])
    return n


# Helper functions live in Shaders/noise.ush, mapped to /Project/ by the game module.
NOISE_HLSL = ""


def build_planet(mat):
    c0 = vec_param(mat, "C0", (0.2, 0.25, 0.35, 1), -900, -300)
    c1 = vec_param(mat, "C1", (0.35, 0.5, 0.35, 1), -900, -150)
    c2 = vec_param(mat, "C2", (0.8, 0.75, 0.6, 1), -900, 0)
    typ = scalar_param(mat, "Type", 0, -900, 150)
    bands = scalar_param(mat, "Bands", 0, -900, 250)
    seed = scalar_param(mat, "Seed", 0, -900, 350)
    atmo = vec_param(mat, "Atmo", (0.4, 0.7, 1, 1), -900, 450)
    has_atmo = scalar_param(mat, "HasAtmo", 0, -900, 600)
    clouds = scalar_param(mat, "Clouds", 0, -900, 900)
    ring_in = scalar_param(mat, "RingIn", 1.5, -900, 1050)
    ring_out = scalar_param(mat, "RingOut", 2.5, -900, 1150)
    ring_seed = scalar_param(mat, "RingSeed", 0, -900, 1250)
    ring_op = scalar_param(mat, "RingOpacity", 0, -900, 1350)   # 0 = no rings
    sunl, _viewl = sun_local_nodes(mat, -1600, 1500)
    # object-space position normalised = sphere direction
    localpos = MEL.create_material_expression(mat, unreal.MaterialExpressionTransformPosition, -900, -450)
    localpos.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    localpos.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1100, -450)
    MEL.connect_material_expressions(wp, "", localpos, "")
    code = NOISE_HLSL + """
float3 p = normalize(LocalPos);
float3 sp = p + Seed;
float3 col; float3 emis = 0; float spec = 0;
int t = (int)round(Type);
if (t == 9 || t == 10 || t == 8) {
  float giant = t == 10 ? 0.55 : 1.0;
  float warp = fbm(sp * float3(1.3, 4.0, 1.3), 4);
  float lat = p.z + warp * 0.035 * (0.6 + Bands);
  float b = sin(lat * (8.0 + Bands * 10.0) * 3.14159 + Seed * 3.0) * 0.5 + 0.5;
  float fine = fbm(float3(lat * 60.0, sp.x * 0.45, sp.y * 0.45), 4) * 0.5 + 0.5;
  float m = lerp(0.5, b, Bands * giant) * 0.65 + fine * 0.35;
  col = lerp(C0, C1, smoothstep(0.15, 0.6, m));
  col = lerp(col, C2, smoothstep(0.55, 0.95, m + fine * 0.15));
  float3 sc = normalize(float3(0.8, 0.5, -0.32));
  float storm = smoothstep(0.16, 0.0, distance(p, sc) + fbm(sp * 6.0, 3) * 0.04);
  col = lerp(col, C0 * 0.8 + C2 * 0.25, storm * 0.7 * Bands * giant);
} else if (t == 5 || t == 6 || t == 7) {
  float h = fbm(sp * 1.7, 7);
  float detail = fbm(sp * 14.0, 4);
  float sea = t == 5 ? 0.22 : (t == 7 ? -0.12 : 0.04);
  if (h < sea) { col = lerp(C0 * 0.45, C0, smoothstep(sea - 0.45, sea, h)); spec = 1.0; }
  else { float e = smoothstep(sea, sea + 0.45, h + detail * 0.12); col = lerp(C1, C2, e); col = lerp(col, C1 * 0.7, smoothstep(0.2, 0.6, detail) * 0.4); }
  float ice = smoothstep(0.80, 0.86, abs(p.z) + fbm(sp * 5.0, 3) * 0.06);
  col = lerp(col, float3(0.92, 0.95, 1.0), ice);
} else if (t == 3) {
  float h = fbm(sp * 2.4, 6) * 0.5 + 0.5;
  col = lerp(C0, C1, h);
  float cr = pow(ridged(sp * 3.2, 6), 9.0);
  emis = C2 * cr * 3.0 + C2 * pow(ridged(sp * 11.0, 4), 14.0) * 1.5;
} else {
  float h = fbm(sp * 2.6, 7) * 0.5 + 0.5;
  float detail = fbm(sp * 22.0, 4);
  col = lerp(C0, C1, smoothstep(0.3, 0.68, h));
  col = lerp(col, C2, smoothstep(0.6, 0.9, h + detail * 0.08));
  float cr = ridged(sp * 6.0, 4);
  if (t == 1 || t == 0) col *= 0.75 + 0.35 * smoothstep(0.7, 0.95, cr);
  if (t == 4) { float cracks = pow(1.0 - abs(snoise(sp * 7.0)), 18.0); col = lerp(col, C0 * 0.7, cracks * 0.8); }
}
if (Clouds > 0.02) {
  float3 q = p * 2.2 + (Seed + 11.0);
  q += 0.25 * float3(fbm(q, 3), fbm(q + 5.2, 3), fbm(q + 9.1, 3));
  float n = fbm(q, 6) * 0.5 + 0.5;
  float a = smoothstep(1.0 - Clouds, 1.0 - Clouds + 0.28, n);
  col = lerp(col, float3(1, 1, 1), a * 0.92);
  spec = lerp(spec, 0.0, a);
}
// ring shadow: trace from this surface point toward the sun and see if it crosses the ring annulus
if (RingOpacity > 0.001) {
  float3 L = normalize(SunL);
  if (abs(L.z) > 1e-4) {
    float tt = -p.z / L.z;
    if (tt > 0.0) {
      float3 hit = p + tt * L;
      float u = (length(hit.xy) - RingIn) / (RingOut - RingIn);
      if (u > 0.0 && u < 1.0) col *= 1.0 - 0.85 * ringDensity(u, RingSeed, 0.0) * RingOpacity;
    }
  }
}
Spec = spec;
Emissive = emis;
return col;
"""
    # The Custom node returns the base colour and writes Spec/Emissive via extra outputs
    node = custom(mat, code, ["LocalPos", "C0", "C1", "C2", "Type", "Bands", "Seed", "Clouds", "RingIn", "RingOut", "RingSeed", "RingOpacity", "SunL"], -300, 0)
    outs = []
    for name, typ_ in (("Spec", unreal.CustomMaterialOutputType.CMOT_FLOAT1), ("Emissive", unreal.CustomMaterialOutputType.CMOT_FLOAT3)):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", typ_)
        outs.append(o)
    node.set_editor_property("additional_outputs", outs)
    MEL.connect_material_expressions(localpos, "", node, "LocalPos")
    for n, nm in ((c0, "C0"), (c1, "C1"), (c2, "C2"), (typ, "Type"), (bands, "Bands"), (seed, "Seed"), (clouds, "Clouds"), (ring_in, "RingIn"), (ring_out, "RingOut"), (ring_seed, "RingSeed"), (ring_op, "RingOpacity"), (sunl, "SunL")):
        MEL.connect_material_expressions(n, "", node, nm)
    MEL.connect_material_property(node, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(node, "Emissive", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # roughness: water is glossy
    one = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 400)
    one.set_editor_property("r", 1.0)
    sub = MEL.create_material_expression(mat, unreal.MaterialExpressionSubtract, -100, 400)
    MEL.connect_material_expressions(one, "", sub, "A")
    MEL.connect_material_expressions(node, "Spec", sub, "B")
    MEL.connect_material_property(sub, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # atmosphere rim via fresnel
    fres = MEL.create_material_expression(mat, unreal.MaterialExpressionFresnel, -300, 600)
    fres.set_editor_property("exponent", 2.6)
    mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, 600)
    mul2 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, 50, 600)
    MEL.connect_material_expressions(fres, "", mul, "A")
    MEL.connect_material_expressions(atmo, "", mul, "B")
    MEL.connect_material_expressions(mul, "", mul2, "A")
    MEL.connect_material_expressions(has_atmo, "", mul2, "B")
    add = MEL.create_material_expression(mat, unreal.MaterialExpressionAdd, 200, 500)
    MEL.connect_material_expressions(node, "Emissive", add, "A")
    MEL.connect_material_expressions(mul2, "", add, "B")
    MEL.connect_material_property(add, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property("two_sided", False)


def build_star(mat):
    color = vec_param(mat, "Color", (20, 18, 14, 1), -700, 0)
    seed = scalar_param(mat, "Seed", 0, -700, 150)
    time = scalar_param(mat, "Time", 0, -700, 250)
    localpos = MEL.create_material_expression(mat, unreal.MaterialExpressionTransformPosition, -700, -250)
    localpos.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    localpos.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, -250)
    MEL.connect_material_expressions(wp, "", localpos, "")
    code = NOISE_HLSL + """
float3 p = normalize(LocalPos);
float g = fbm(p * 9.0 + Time * 0.03 + Seed, 5) * 0.5 + 0.5;
float spots = smoothstep(0.62, 0.8, fbm(p * 2.5 + Time * 0.005 + Seed, 4) * 0.5 + 0.5);
return Color * (0.75 + 0.5 * g) * (1.0 - spots * 0.35);
"""
    node = custom(mat, code, ["LocalPos", "Color", "Seed", "Time"], -300, 0)
    MEL.connect_material_expressions(localpos, "", node, "LocalPos")
    MEL.connect_material_expressions(color, "", node, "Color")
    MEL.connect_material_expressions(seed, "", node, "Seed")
    MEL.connect_material_expressions(time, "", node, "Time")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    MEL.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def build_sky(mat):
    seed = scalar_param(mat, "Seed", 0, -700, 150)
    tint = vec_param(mat, "Nebula", (0.25, 0.18, 0.45, 1), -700, 300)
    localpos = MEL.create_material_expression(mat, unreal.MaterialExpressionTransformPosition, -700, -250)
    localpos.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    localpos.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, -250)
    MEL.connect_material_expressions(wp, "", localpos, "")
    code = NOISE_HLSL + """
float3 d = normalize(LocalPos);
// galactic plane: tilted band
float3 gn = normalize(float3(0.32, 0.18, 0.93));
float band = abs(dot(d, gn));
float3 gd = d + Seed;
float dust = fbm(gd * 2.2, 5) * 0.5 + 0.5;
float lanes = fbm(gd * 7.0 + 3.0, 4) * 0.5 + 0.5;
float mw = smoothstep(0.42, 0.0, band) * (0.35 + 0.65 * dust);
mw *= 1.0 - 0.75 * smoothstep(0.35, 0.75, lanes) * smoothstep(0.25, 0.0, band);
float3 col = float3(0.78, 0.72, 0.62) * mw * 0.28;
// faint nebulae
// faint nebulae: sparse, wispy, mostly hidden in the galactic band
float neb = pow(saturate(fbm(gd * 1.3 + 11.0, 5) * 0.5 + 0.5), 5.0) * (0.3 + 0.7 * smoothstep(0.6, 0.0, band));
col += Nebula * neb * 0.12;
col += float3(0.12, 0.25, 0.4) * pow(saturate(fbm(gd * 1.7 - 5.0, 4) * 0.5 + 0.5), 7.0) * 0.14;
// stars: three magnitude layers (sparse bright, medium, dense faint); more in the band
float dens = 1.0 + 1.0 * smoothstep(0.5, 0.0, band);
float s = starLayer(d, 24.0, 0.05 * dens, Seed) * 3.0;
s += starLayer(d, 70.0, 0.045 * dens, Seed + 9.0) * 1.4;
s += starLayer(d, 180.0, 0.05 * dens, Seed + 31.0) * 0.45;
float3 tintv = lerp(float3(0.75, 0.82, 1.0), float3(1.0, 0.85, 0.65), hash13(floor(d * 90.0 + Seed)));
col += tintv * s;
return col;
"""
    node = custom(mat, code, ["LocalPos", "Seed", "Nebula"], -300, 0)
    MEL.connect_material_expressions(localpos, "", node, "LocalPos")
    MEL.connect_material_expressions(seed, "", node, "Seed")
    MEL.connect_material_expressions(tint, "", node, "Nebula")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    # "Is Sky": rendered at the far plane, so the dome never occludes anything however small it is.
    mat.set_editor_property("is_sky", True)
    MEL.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def sun_local_nodes(mat, x, y):
    """SunL = normalize(SunPos - ObjectPos) in the primitive's local space; ViewL = camera vector in local space."""
    sunpos = vec_param(mat, "SunPos", (1, 0, 0, 1), x, y)
    op = MEL.create_material_expression(mat, unreal.MaterialExpressionObjectPositionWS, x, y + 150)
    sub = MEL.create_material_expression(mat, unreal.MaterialExpressionSubtract, x + 200, y)
    MEL.connect_material_expressions(sunpos, "", sub, "A")
    MEL.connect_material_expressions(op, "", sub, "B")
    norm = MEL.create_material_expression(mat, unreal.MaterialExpressionNormalize, x + 350, y)
    MEL.connect_material_expressions(sub, "", norm, "")
    tr = MEL.create_material_expression(mat, unreal.MaterialExpressionTransform, x + 500, y)
    tr.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    tr.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    MEL.connect_material_expressions(norm, "", tr, "")
    cam = MEL.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, x, y + 300)
    tr2 = MEL.create_material_expression(mat, unreal.MaterialExpressionTransform, x + 500, y + 300)
    tr2.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    tr2.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    MEL.connect_material_expressions(cam, "", tr2, "")
    return tr, tr2


def build_tunnel(mat):
    """Hyperspace tunnel seen from inside a long cylinder: streaks racing past, tinted by the destination star."""
    color = vec_param(mat, "Color", (0.6, 0.7, 1.0, 1), -700, 0)
    time = scalar_param(mat, "Time", 0, -700, 150)
    localpos = MEL.create_material_expression(mat, unreal.MaterialExpressionTransformPosition, -700, -250)
    localpos.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    localpos.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, -250)
    MEL.connect_material_expressions(wp, "", localpos, "")
    code = NOISE_HLSL + """
// cylinder mesh: radius 50, z in -50..50 (its axis is the ship's forward)
float ang = atan2(LocalPos.y, LocalPos.x);
float z = LocalPos.z / 50.0;
float3 q = float3(ang * 3.0, z * 4.0 - Time * 2.5, 0.0);
float streak = pow(saturate(fbm(q, 4) * 0.5 + 0.5), 7.0);
float fine = pow(saturate(snoise(float3(ang * 24.0, z * 40.0 - Time * 9.0, 2.0)) * 0.5 + 0.5), 10.0);
float pulse = 0.75 + 0.25 * sin(Time * 4.0 + z * 6.0);
float3 col = Color * (streak * 5.0 + fine * 3.0) * pulse + Color * 0.04;
// fade toward the far ends so the cylinder caps never show
col *= smoothstep(1.0, 0.6, abs(z));
return col;
"""
    node = custom(mat, code, ["LocalPos", "Color", "Time"], -300, 0)
    MEL.connect_material_expressions(localpos, "", node, "LocalPos")
    MEL.connect_material_expressions(color, "", node, "Color")
    MEL.connect_material_expressions(time, "", node, "Time")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    MEL.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def build_rings(mat):
    color = vec_param(mat, "Color", (0.8, 0.78, 0.74, 1), -700, 0)
    opacity = scalar_param(mat, "Opacity", 0.7, -700, 150)
    inner = scalar_param(mat, "Inner", 0.5, -700, 250)
    seed = scalar_param(mat, "Seed", 0, -700, 350)
    planet = scalar_param(mat, "Planet", 0.4, -700, 450)   # planet radius / ring outer radius
    suncol = vec_param(mat, "SunColor", (1, 0.97, 0.9, 1), -700, 550)
    localpos = MEL.create_material_expression(mat, unreal.MaterialExpressionTransformPosition, -700, -200)
    localpos.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    localpos.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, -200)
    MEL.connect_material_expressions(wp, "", localpos, "")
    sunl, viewl = sun_local_nodes(mat, -1500, 700)
    code = NOISE_HLSL + """
float3 P = LocalPos / 50.0;               // plane mesh is 100 cm: P in units of the outer radius
float r = length(P.xy);
float t = (r - Inner) / (1.0 - Inner);
float fw = fwidth(t); // before any branch: derivatives need uniform control flow
if (t < 0.0 || t > 1.0) { Alpha = 0; return 0; }
float d = ringDensity(t, Seed, fw);
float3 L = normalize(SunL);
float3 V = normalize(ViewL);
// planet shadow: ray from the ring point toward the sun hits the planet sphere at the origin
float sh = 1.0;
float t0 = -dot(P, L);
if (t0 > 0.0) { float d2 = dot(P, P) - t0 * t0; sh = smoothstep(Planet * Planet * 0.9, Planet * Planet * 1.03, d2); }
float lit = 0.12 + 0.88 * abs(L.z);
float grain = 0.8 + 0.25 * snoise(float3(t * 40.0, 3.0, Seed)) * saturate(1.0 - fw * 60.0);
// thin, icy rings glow when you look at the sun through them
float back = pow(saturate(dot(-V, L)), 6.0) * 0.7 * (1.0 - d * 0.7);
float3 col = Color * SunColor * grain * (lit * sh + 0.025) + Color * SunColor * back * sh;
Alpha = d * Opacity;
return col;
"""
    node = custom(mat, code, ["LocalPos", "Color", "Opacity", "Inner", "Seed", "Planet", "SunColor", "SunL", "ViewL"], -300, 0)
    o = unreal.CustomOutput()
    o.set_editor_property("output_name", "Alpha")
    o.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    node.set_editor_property("additional_outputs", [o])
    MEL.connect_material_expressions(localpos, "", node, "LocalPos")
    for n, nm in ((color, "Color"), (opacity, "Opacity"), (inner, "Inner"), (seed, "Seed"), (planet, "Planet"), (suncol, "SunColor"), (sunl, "SunL"), (viewl, "ViewL")):
        MEL.connect_material_expressions(n, "", node, nm)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    MEL.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(node, "Alpha", unreal.MaterialProperty.MP_OPACITY)


def build_hull(mat):
    """Panelled hull: triplanar panel grid in the part's own frame (world pos relative to the
    part, un-spun by the Spin parameter the station actor updates), with seams, per-panel tint,
    scattered lit windows and roughness variation."""
    color = vec_param(mat, "Color", (0.35, 0.38, 0.42, 1), -900, 0)
    windows = scalar_param(mat, "Windows", 0.0, -900, 150)       # 0..1 density of lit windows
    winc = vec_param(mat, "WindowColor", (1.0, 0.85, 0.6, 1), -900, 300)
    spin = scalar_param(mat, "Spin", 0.0, -900, 450)              # radians, station rotation about Z
    panel = scalar_param(mat, "PanelM", 6.0, -900, 600)           # panel size in metres
    seed = scalar_param(mat, "Seed", 0.0, -900, 750)
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, -300)
    op = MEL.create_material_expression(mat, unreal.MaterialExpressionObjectPositionWS, -900, -150)
    nrm = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, -450)
    code = NOISE_HLSL + """
float3 rel = WorldPos - ObjectPos;                 // cm, relative to this part
float cs = cos(-Spin), sn = sin(-Spin);
float3 p = float3(rel.x * cs - rel.y * sn, rel.x * sn + rel.y * cs, rel.z) / 100.0 + Seed; // metres, un-spun
float3 n = abs(normalize(Normal));
float3 w = pow(n, 6.0); w /= (w.x + w.y + w.z);
float3 col = 0; float seam = 0; float win = 0; float rough = 0; float tintv = 0;
float2 uvs[3] = { p.yz, p.xz, p.xy };
[unroll] for (int i = 0; i < 3; i++) {
  float2 uv = uvs[i] / PanelM;
  float2 cell = floor(uv);
  float2 f = frac(uv);
  float h = hash13(float3(cell, i * 7.0 + Seed));
  // sub-panel split for variety
  float split = step(0.5, hash13(float3(cell, 3.0 + i)));
  float2 f2 = split > 0.5 ? frac(uv * float2(2.0, 1.0)) : f;
  float2 e = min(f2, 1.0 - f2);
  float s = 1.0 - smoothstep(0.0, 0.035, min(e.x, e.y));
  // rivet/line detail along one edge
  float stripe = smoothstep(0.012, 0.0, abs(f.y - 0.08)) * step(0.6, h);
  float t = 0.82 + 0.36 * h;
  // windows: a rectangle in some cells
  float wsel = step(1.0 - Windows * 0.35, hash13(float3(cell, 11.0 + i)));
  float2 wc = abs(f - float2(0.5, 0.35));
  float wr = step(wc.x, 0.22) * step(wc.y, 0.06) * wsel;
  seam += w[i] * max(s, stripe * 0.6);
  tintv += w[i] * t;
  win += w[i] * wr;
  rough += w[i] * (0.4 + 0.35 * hash13(float3(cell, 5.0 + i)) + 0.25 * s);
}
float grime = fbm(p * 0.35, 3) * 0.5 + 0.5;
col = Color * tintv * (1.0 - seam * 0.55) * (0.8 + 0.3 * grime);
Roughness = saturate(rough + grime * 0.15);
Emissive = WindowColor * win * 4.0 * (0.6 + 0.4 * hash13(floor(p * 0.5)));
return col;
"""
    node = custom(mat, code, ["WorldPos", "ObjectPos", "Normal", "Color", "Windows", "WindowColor", "Spin", "PanelM", "Seed"], -300, 0)
    outs = []
    for name, typ_ in (("Roughness", unreal.CustomMaterialOutputType.CMOT_FLOAT1), ("Emissive", unreal.CustomMaterialOutputType.CMOT_FLOAT3)):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", typ_)
        outs.append(o)
    node.set_editor_property("additional_outputs", outs)
    MEL.connect_material_expressions(wp, "", node, "WorldPos")
    MEL.connect_material_expressions(op, "", node, "ObjectPos")
    MEL.connect_material_expressions(nrm, "", node, "Normal")
    for n, nm in ((color, "Color"), (windows, "Windows"), (winc, "WindowColor"), (spin, "Spin"), (panel, "PanelM"), (seed, "Seed")):
        MEL.connect_material_expressions(n, "", node, nm)
    MEL.connect_material_property(node, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(node, "Roughness", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(node, "Emissive", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    metal = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 400)
    metal.set_editor_property("r", 0.55)
    MEL.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)


def build_emissive(mat):
    color = vec_param(mat, "EmissiveColor", (3, 1.6, 0.4, 1), -500, 0)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def ensure_map():
    path = "/Game/Maps/Space"
    if EAL.does_asset_exist(path) and not FORCE:
        log("map exists")
        return
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.new_level(path)
    world = unreal.EditorLevelLibrary.get_editor_world()
    ws = world.get_world_settings() if hasattr(world, "get_world_settings") else None
    if ws:
        ws.set_editor_property("enable_world_bounds_checks", False)
        ws.set_editor_property("kill_z", -1e12)
        ws.set_editor_property("default_game_mode", unreal.load_class(None, "/Script/OmniSpace.OmniGameMode"))
    les.save_current_level()
    log("map created")


ensure_material("M_Hull", build_hull)
ensure_material("M_Emissive", build_emissive)
ensure_material("M_Star", build_star)
ensure_material("M_Rings", build_rings)
ensure_material("M_Planet", build_planet)
ensure_material("M_Sky", build_sky)
ensure_material("M_Tunnel", build_tunnel)
ensure_map()
log("done")
