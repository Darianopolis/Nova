#include "nova_Draw2DShared.slang"

#ifdef __cplusplus
namespace {
#endif

struct PushConstants
{
    float2             inv_half_extent;
    float2                  center_pos;
    const nova::draw::Rectangle* rects;
};

#ifdef __cplusplus
}
#else

[vk::push_constant] PushConstants pc;

static const float2 deltas[6] = {
    { -1, -1 }, { -1, 1 }, { 1, -1 }, { -1, 1 }, { 1, 1 }, { 1, -1 }
};

struct VertexOutput
{
    float2   tex;
    uint rect_id;
    float4   pos : SV_Position;
};

[shader("vertex")]
VertexOutput Vertex(uint global_id : SV_VertexID)
{
    uint rect_id = global_id / 6;
    uint vertex_id = global_id % 6;

    nova::draw::Rectangle box = pc.rects[rect_id];
    float2 delta = deltas[vertex_id];

    float sinRot = sin(box.rotation);
    float cosRot = cos(box.rotation);
    float2x2 rot = float2x2(
        float2(cosRot, sinRot),
        float2(-sinRot, cosRot));

    float2 half_extent = box.half_extent;
    if (box.rotation != 0.0)
        half_extent += 0.5;

    VertexOutput out;
    out.tex = delta * half_extent;
    out.rect_id = rect_id;
    out.pos = float4((mul(rot, (delta * half_extent)) + box.center_pos - pc.center_pos) * pc.inv_half_extent, 0, 1);
    return out;
}

float EvalRoundedBoxSDF(float2 pos, float2 half_extent, float radius) {
    float2 q = abs(pos) - half_extent + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

[shader("fragment")]
float4 Fragment(in VertexOutput vout)
{
    nova::draw::Rectangle box = pc.rects[vout.rect_id];

    float4 tex_tint = box.tex_tint.AsFloat4();

    float4 sampled = tex_tint.a > 0
        ? tex_tint * box.tex_handle.Sample((vout.tex / box.half_extent) * box.tex_half_extent + box.tex_center_pos)
        : float4(0);

    float4 center_color_base = box.center_color.AsFloat4();
    float4 center_color = float4(
        sampled.rgb * sampled.a + center_color_base.rgb * (1 - sampled.a),
        sampled.a + center_color_base.a * (1 - sampled.a));

    float4 border_color = box.border_color.AsFloat4();

    if (box.rotation != 0)
    {
        // Smooth SDF

        float dist = EvalRoundedBoxSDF(vout.tex, box.half_extent, box.corner_radius);
        float delta = fwidth(dist) * 0.5;

        if (dist > delta) {
            discard;
        }

        return lerp(
            lerp(
                center_color,
                border_color,
                smoothstep(-box.border_width - delta, -box.border_width + delta, dist)),
            float4(border_color.rgb, 0.f),
            smoothstep(-delta, delta, dist));
    }
    else
    {
        // Flat edges
        // TODO: Resolve pixel rounding issues

        float2 abs_pos = abs(vout.tex);
        float2 corner_focus = box.half_extent - float2(box.corner_radius);

        if (abs_pos.x > corner_focus.x && abs_pos.y > corner_focus.y) {
            float dist = length(abs_pos - corner_focus);
            if (dist > box.corner_radius + 0.5) {
                discard;
            }

            return (dist > box.corner_radius - box.border_width + 0.5)
                       ? float4(border_color.rgb, border_color.a * (1 - max(0, dist - (box.corner_radius - 0.5))))
                       : lerp(center_color, border_color, max(0, dist - (box.corner_radius - box.border_width - 0.5)));
        } else {
            return (abs_pos.x > box.half_extent.x - box.border_width || abs_pos.y > box.half_extent.y - box.border_width)
                       ? border_color
                       : center_color;
        }
    }
}

#endif