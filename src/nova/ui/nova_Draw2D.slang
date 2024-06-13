#include "nova_Draw2DShared.slang"

#ifdef __cplusplus
namespace {
#endif

struct PushConstants
{
    float2       inv_half_extent;
    float2            center_pos;
    nova::draw::Rectangle *rects;
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

    VertexOutput out;
    out.tex = delta * box.half_extent;
    out.rect_id = rect_id;
    out.pos = float4(((delta * box.half_extent) + box.center_pos - pc.center_pos) * pc.inv_half_extent, 0, 1);
    return out;
}

[shader("fragment")]
float4 Fragment(in VertexOutput vout)
{
    nova::draw::Rectangle box = pc.rects[vout.rect_id];

    float2 abs_pos = abs(vout.tex);
    float2 corner_focus = box.half_extent - float2(box.corner_radius);

    float4 sampled = box.tex_tint.a > 0
        ? box.tex_tint * box.tex_handle.Sample((vout.tex / box.half_extent) * box.tex_half_extent + box.tex_center_pos)
        : float4(0);

    float4 center_color = float4(
        sampled.rgb * sampled.a + box.center_color.rgb * (1 - sampled.a),
        sampled.a + box.center_color.a * (1 - sampled.a));

    if (abs_pos.x > corner_focus.x && abs_pos.y > corner_focus.y) {
        float dist = length(abs_pos - corner_focus);
        if (dist > box.corner_radius + 0.5) {
            discard;
        }

        return (dist > box.corner_radius - box.border_width + 0.5)
            ? float4(box.border_color.rgb, box.border_color.a * (1 - max(0, dist - (box.corner_radius - 0.5))))
            : lerp(center_color, box.border_color, max(0, dist - (box.corner_radius - box.border_width - 0.5)));
    } else {
        return (abs_pos.x > box.half_extent.x - box.border_width || abs_pos.y > box.half_extent.y - box.border_width)
            ? box.border_color
            : center_color;
    }
}

#endif