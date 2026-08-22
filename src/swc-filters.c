/*
Selected Windows Capture — censor filter (blur, mosaic)
*/

#include "swc-filters.h"

#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <util/platform.h>

#include <string.h>

enum swc_censor_kind {
	SWC_CENSOR_BLUR = 0,
	SWC_CENSOR_MOSAIC = 1,
};

enum swc_blur_style {
	SWC_BLUR_GAUSSIAN = 0,
	SWC_BLUR_BOX = 1,
	SWC_BLUR_KAWASE = 2,
	SWC_BLUR_HORIZONTAL = 3,
	SWC_BLUR_VERTICAL = 4,
};

enum swc_mosaic_style {
	SWC_MOSAIC_SQUARE = 0,
	SWC_MOSAIC_HEX = 1,
	SWC_MOSAIC_DIAMOND = 2,
	SWC_MOSAIC_CIRCLE = 3,
	SWC_MOSAIC_BAR_H = 4,
	SWC_MOSAIC_BAR_V = 5,
};

struct swc_censor {
	obs_source_t *context;
	gs_effect_t *blur_fx;
	gs_effect_t *mosaic_fx;
	gs_texrender_t *rt[2];
	gs_eparam_t *blur_image;
	gs_eparam_t *blur_texel;
	gs_eparam_t *blur_radius;
	gs_eparam_t *mosaic_texel;
	gs_eparam_t *mosaic_block;
	int kind;
	int style;
	double strength;
	double block;
};

static gs_effect_t *load_effect(const char *file)
{
	char *path = obs_module_file(file);
	gs_effect_t *effect = NULL;

	obs_enter_graphics();
	if (path)
		effect = gs_effect_create_from_file(path, NULL);
	obs_leave_graphics();
	bfree(path);
	return effect;
}

static const char *mosaic_tech(int style)
{
	switch (style) {
	case SWC_MOSAIC_HEX:
		return "Hex";
	case SWC_MOSAIC_DIAMOND:
		return "Diamond";
	case SWC_MOSAIC_CIRCLE:
		return "Circle";
	case SWC_MOSAIC_BAR_H:
		return "BarH";
	case SWC_MOSAIC_BAR_V:
		return "BarV";
	case SWC_MOSAIC_SQUARE:
	default:
		return "Square";
	}
}

static void set_visible(obs_properties_t *props, const char *name, bool vis)
{
	obs_property_t *p = obs_properties_get(props, name);
	if (p)
		obs_property_set_visible(p, vis);
}

static void apply_kind_visibility(obs_properties_t *props, obs_data_t *settings)
{
	int kind = settings ? (int)obs_data_get_int(settings, "kind") : SWC_CENSOR_MOSAIC;

	set_visible(props, "blur_style", kind == SWC_CENSOR_BLUR);
	set_visible(props, "mosaic_style", kind == SWC_CENSOR_MOSAIC);
	set_visible(props, "strength", kind == SWC_CENSOR_BLUR);
	set_visible(props, "block", kind == SWC_CENSOR_MOSAIC);
}

static bool kind_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	apply_kind_visibility(props, settings);
	return true;
}

static const char *censor_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("CensorFilter");
}

static void censor_update(void *data, obs_data_t *settings)
{
	struct swc_censor *f = data;

	f->kind = (int)obs_data_get_int(settings, "kind");
	if (f->kind == SWC_CENSOR_BLUR)
		f->style = (int)obs_data_get_int(settings, "blur_style");
	else
		f->style = (int)obs_data_get_int(settings, "mosaic_style");
	f->strength = obs_data_get_double(settings, "strength");
	f->block = obs_data_get_double(settings, "block");
}

static void censor_destroy(void *data)
{
	struct swc_censor *f = data;

	obs_enter_graphics();
	if (f->blur_fx)
		gs_effect_destroy(f->blur_fx);
	if (f->mosaic_fx)
		gs_effect_destroy(f->mosaic_fx);
	if (f->rt[0])
		gs_texrender_destroy(f->rt[0]);
	if (f->rt[1])
		gs_texrender_destroy(f->rt[1]);
	obs_leave_graphics();
	bfree(f);
}

static void *censor_create(obs_data_t *settings, obs_source_t *context)
{
	struct swc_censor *f = bzalloc(sizeof(*f));

	f->context = context;
	f->blur_fx = load_effect("blur.effect");
	f->mosaic_fx = load_effect("mosaic.effect");
	if (!f->blur_fx)
		blog(LOG_ERROR, "[selected-windows-capture] Could not load blur.effect");
	if (!f->mosaic_fx)
		blog(LOG_ERROR, "[selected-windows-capture] Could not load mosaic.effect");

	obs_enter_graphics();
	f->rt[0] = gs_texrender_create(GS_RGBA16F, GS_ZS_NONE);
	f->rt[1] = gs_texrender_create(GS_RGBA16F, GS_ZS_NONE);
	obs_leave_graphics();

	if (f->blur_fx) {
		f->blur_image = gs_effect_get_param_by_name(f->blur_fx, "image");
		f->blur_texel = gs_effect_get_param_by_name(f->blur_fx, "texel");
		f->blur_radius = gs_effect_get_param_by_name(f->blur_fx, "radius");
	}
	if (f->mosaic_fx) {
		f->mosaic_texel = gs_effect_get_param_by_name(f->mosaic_fx, "texel");
		f->mosaic_block = gs_effect_get_param_by_name(f->mosaic_fx, "block");
	}

	censor_update(f, settings);
	return f;
}

static bool rt_begin(gs_texrender_t *rt, uint32_t cx, uint32_t cy)
{
	struct vec4 clear;

	if (!rt)
		return false;
	gs_texrender_reset(rt);
	if (!gs_texrender_begin(rt, cx, cy))
		return false;
	vec4_zero(&clear);
	gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
	gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);
	return true;
}

static bool blur_pass(struct swc_censor *f, gs_texrender_t *dst, gs_texture_t *src, const char *tech, uint32_t cx,
		      uint32_t cy, float tx, float ty, float radius)
{
	struct vec2 texel;

	if (!dst || !src || !tech)
		return false;
	if (!rt_begin(dst, cx, cy))
		return false;
	vec2_set(&texel, tx, ty);
	gs_effect_set_texture(f->blur_image, src);
	gs_effect_set_vec2(f->blur_texel, &texel);
	gs_effect_set_float(f->blur_radius, radius);
	gs_blend_state_push();
	gs_enable_blending(false);
	while (gs_effect_loop(f->blur_fx, tech))
		gs_draw_sprite(src, 0, cx, cy);
	gs_blend_state_pop();
	gs_texrender_end(dst);
	return gs_texrender_get_texture(dst) != NULL;
}

static void swap_rt(struct swc_censor *f)
{
	gs_texrender_t *tmp = f->rt[0];
	f->rt[0] = f->rt[1];
	f->rt[1] = tmp;
}

static gs_texture_t *render_dual_kawase(struct swc_censor *f, gs_texture_t *tex, uint32_t cx, uint32_t cy,
				       int levels)
{
	uint32_t ws[8];
	uint32_t hs[8];
	int i;

	if (levels < 1)
		return tex;
	if (levels > 5)
		levels = 5;

	ws[0] = cx;
	hs[0] = cy;
	for (i = 1; i <= levels; i++) {
		ws[i] = ws[i - 1] > 2 ? ws[i - 1] / 2 : 2;
		hs[i] = hs[i - 1] > 2 ? hs[i - 1] / 2 : 2;
	}

	for (i = 0; i < levels; i++) {
		if (!blur_pass(f, f->rt[1], tex, "KawaseDown", ws[i + 1], hs[i + 1], 1.0f / (float)ws[i],
			       1.0f / (float)hs[i], (float)i))
			return NULL;
		tex = gs_texrender_get_texture(f->rt[1]);
		swap_rt(f);
		if (!tex)
			return NULL;
	}

	for (i = levels; i > 0; i--) {
		if (!blur_pass(f, f->rt[1], tex, "KawaseUp", ws[i - 1], hs[i - 1], 1.0f / (float)ws[i],
			       1.0f / (float)hs[i], (float)(i - 1)))
			return NULL;
		tex = gs_texrender_get_texture(f->rt[1]);
		swap_rt(f);
		if (!tex)
			return NULL;
	}
	return tex;
}

static gs_texture_t *render_separable(struct swc_censor *f, gs_texture_t *tex, uint32_t cx, uint32_t cy,
				     const char *tech, bool axis_h, bool axis_v, int passes, float radius)
{
	int p;

	for (p = 0; p < passes; p++) {
		if (axis_h) {
			if (!blur_pass(f, f->rt[1], tex, tech, cx, cy, 1.0f / (float)cx, 0.0f, radius))
				return NULL;
			tex = gs_texrender_get_texture(f->rt[1]);
			swap_rt(f);
			if (!tex)
				return NULL;
		}
		if (axis_v) {
			if (!blur_pass(f, f->rt[1], tex, tech, cx, cy, 0.0f, 1.0f / (float)cy, radius))
				return NULL;
			tex = gs_texrender_get_texture(f->rt[1]);
			swap_rt(f);
			if (!tex)
				return NULL;
		}
	}
	return tex;
}

static gs_texture_t *render_kawase_full(struct swc_censor *f, gs_texture_t *tex, uint32_t cx, uint32_t cy, int passes,
					float radius)
{
	int p;
	float tx = 1.0f / (float)cx;
	float ty = 1.0f / (float)cy;

	for (p = 0; p < passes; p++) {
		if (!blur_pass(f, f->rt[1], tex, "KawaseDown", cx, cy, tx, ty, radius))
			return NULL;
		tex = gs_texrender_get_texture(f->rt[1]);
		swap_rt(f);
		if (!tex)
			return NULL;
		if (!blur_pass(f, f->rt[1], tex, "KawaseUp", cx, cy, tx, ty, radius))
			return NULL;
		tex = gs_texrender_get_texture(f->rt[1]);
		swap_rt(f);
		if (!tex)
			return NULL;
	}
	return tex;
}

static gs_texture_t *render_blur(struct swc_censor *f, gs_texture_t *tex, uint32_t cx, uint32_t cy)
{
	float px = 0.45f + (float)f->strength * 0.34f;
	int passes = (int)(px / 1.5f + 0.5f);
	float radius;
	int extra;

	if (passes < 1)
		passes = 1;
	if (passes > 10)
		passes = 10;
	radius = px / (1.5f * (float)passes);
	if (radius < 0.20f)
		radius = 0.20f;
	if (radius > 1.60f)
		radius = 1.60f;

	switch (f->style) {
	case SWC_BLUR_BOX:
		tex = render_separable(f, tex, cx, cy, "Box", true, true, passes, radius);
		break;
	case SWC_BLUR_HORIZONTAL:
		tex = render_separable(f, tex, cx, cy, "Gaussian", true, false, passes, radius);
		break;
	case SWC_BLUR_VERTICAL:
		tex = render_separable(f, tex, cx, cy, "Gaussian", false, true, passes, radius);
		break;
	case SWC_BLUR_KAWASE:
		tex = render_kawase_full(f, tex, cx, cy, passes, radius);
		break;
	case SWC_BLUR_GAUSSIAN:
	default:
		tex = render_separable(f, tex, cx, cy, "Gaussian", true, true, passes, radius);
		break;
	}

	extra = (int)((f->strength - 55.0) / 15.0);
	if (tex && extra > 0 && f->style != SWC_BLUR_HORIZONTAL && f->style != SWC_BLUR_VERTICAL)
		tex = render_dual_kawase(f, tex, cx, cy, extra);
	return tex;
}

static void render_mosaic(struct swc_censor *f, uint32_t cx, uint32_t cy)
{
	struct vec2 texel;

	if (!f->mosaic_fx)
		return;
	if (!obs_source_process_filter_begin(f->context, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
		return;
	vec2_set(&texel, 1.0f / (float)cx, 1.0f / (float)cy);
	gs_effect_set_vec2(f->mosaic_texel, &texel);
	gs_effect_set_float(f->mosaic_block, (float)f->block);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	obs_source_process_filter_tech_end(f->context, f->mosaic_fx, 0, 0, mosaic_tech(f->style));
	gs_blend_state_pop();
}

static void censor_render(void *data, gs_effect_t *effect)
{
	struct swc_censor *f = data;
	obs_source_t *target;
	uint32_t cx;
	uint32_t cy;
	gs_texture_t *tex;
	gs_effect_t *draw;

	UNUSED_PARAMETER(effect);

	target = obs_filter_get_target(f->context);
	if (!target) {
		obs_source_skip_video_filter(f->context);
		return;
	}
	cx = obs_source_get_base_width(target);
	cy = obs_source_get_base_height(target);
	if (!cx || !cy) {
		obs_source_skip_video_filter(f->context);
		return;
	}

	if (f->kind != SWC_CENSOR_BLUR) {
		render_mosaic(f, cx, cy);
		return;
	}

	if (!f->blur_fx || f->strength < 0.5) {
		obs_source_skip_video_filter(f->context);
		return;
	}

	if (!rt_begin(f->rt[0], cx, cy)) {
		obs_source_skip_video_filter(f->context);
		return;
	}
	gs_blend_state_push();
	gs_enable_blending(false);
	obs_source_video_render(target);
	gs_blend_state_pop();
	gs_texrender_end(f->rt[0]);

	tex = gs_texrender_get_texture(f->rt[0]);
	if (tex)
		tex = render_blur(f, tex, cx, cy);
	if (!tex) {
		obs_source_skip_video_filter(f->context);
		return;
	}

	draw = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_blend_state_push();
	gs_enable_blending(true);
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	gs_effect_set_texture(gs_effect_get_param_by_name(draw, "image"), tex);
	while (gs_effect_loop(draw, "Draw"))
		gs_draw_sprite(tex, 0, 0, 0);
	gs_blend_state_pop();
}

static obs_properties_t *censor_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;
	obs_data_t *settings = NULL;

	if (data)
		settings = obs_source_get_settings(((struct swc_censor *)data)->context);

	p = obs_properties_add_list(props, "kind", obs_module_text("CensorFilter.Kind"), OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, obs_module_text("CensorFilter.Kind.Blur"), SWC_CENSOR_BLUR);
	obs_property_list_add_int(p, obs_module_text("CensorFilter.Kind.Mosaic"), SWC_CENSOR_MOSAIC);
	obs_property_set_modified_callback(p, kind_changed);

	p = obs_properties_add_list(props, "blur_style", obs_module_text("CensorFilter.BlurStyle"),
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, obs_module_text("BlurFilter.Mode.Gaussian"), SWC_BLUR_GAUSSIAN);
	obs_property_list_add_int(p, obs_module_text("BlurFilter.Mode.Box"), SWC_BLUR_BOX);
	obs_property_list_add_int(p, obs_module_text("BlurFilter.Mode.Kawase"), SWC_BLUR_KAWASE);
	obs_property_list_add_int(p, obs_module_text("BlurFilter.Mode.Horizontal"), SWC_BLUR_HORIZONTAL);
	obs_property_list_add_int(p, obs_module_text("BlurFilter.Mode.Vertical"), SWC_BLUR_VERTICAL);

	p = obs_properties_add_list(props, "mosaic_style", obs_module_text("CensorFilter.MosaicStyle"),
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, obs_module_text("MosaicFilter.Mode.Square"), SWC_MOSAIC_SQUARE);
	obs_property_list_add_int(p, obs_module_text("MosaicFilter.Mode.Hex"), SWC_MOSAIC_HEX);
	obs_property_list_add_int(p, obs_module_text("MosaicFilter.Mode.Diamond"), SWC_MOSAIC_DIAMOND);
	obs_property_list_add_int(p, obs_module_text("MosaicFilter.Mode.Circle"), SWC_MOSAIC_CIRCLE);
	obs_property_list_add_int(p, obs_module_text("MosaicFilter.Mode.BarH"), SWC_MOSAIC_BAR_H);
	obs_property_list_add_int(p, obs_module_text("MosaicFilter.Mode.BarV"), SWC_MOSAIC_BAR_V);

	obs_properties_add_float_slider(props, "strength", obs_module_text("CensorFilter.Strength"), 0.0, 100.0, 1.0);
	obs_properties_add_float_slider(props, "block", obs_module_text("CensorFilter.Block"), 2.0, 128.0, 1.0);

	apply_kind_visibility(props, settings);
	if (settings)
		obs_data_release(settings);
	return props;
}

static void censor_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "kind", SWC_CENSOR_MOSAIC);
	obs_data_set_default_int(settings, "blur_style", SWC_BLUR_GAUSSIAN);
	obs_data_set_default_int(settings, "mosaic_style", SWC_MOSAIC_SQUARE);
	obs_data_set_default_double(settings, "strength", 40.0);
	obs_data_set_default_double(settings, "block", 16.0);
}

static struct obs_source_info swc_censor_filter_info = {
	.id = "swc_censor_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
	.get_name = censor_get_name,
	.create = censor_create,
	.destroy = censor_destroy,
	.update = censor_update,
	.video_render = censor_render,
	.get_properties = censor_properties,
	.get_defaults = censor_defaults,
};

void swc_register_filters(void)
{
	obs_register_source(&swc_censor_filter_info);
}
