// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2013, The Linux Foundation. All rights reserved.

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>

struct mdss_dsi_samsung_1080p_cmd_fa2 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;

	bool prepared;
	bool enabled;
};

static inline struct mdss_dsi_samsung_1080p_cmd_fa2 *to_mdss_dsi_samsung_1080p_cmd_fa2(struct drm_panel *panel)
{
	return container_of(panel, struct mdss_dsi_samsung_1080p_cmd_fa2, panel);
}

#define dsi_generic_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_generic_write(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

#define dsi_dcs_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static void mdss_dsi_samsung_1080p_cmd_fa2_reset(struct mdss_dsi_samsung_1080p_cmd_fa2 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(5);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(5);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(7);
}

static int mdss_dsi_samsung_1080p_cmd_fa2_on(struct mdss_dsi_samsung_1080p_cmd_fa2 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	dsi_generic_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	dsi_generic_write_seq(dsi, 0xfc, 0x5a, 0x5a);
	dsi_dcs_write_seq(dsi, 0xf2);
	dsi_dcs_write_seq(dsi, 0xf9);
	msleep(5);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(20);

	dsi_generic_write_seq(dsi, 0xca,
			      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80,
			      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
			      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
			      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00,
			      0x00);
	dsi_generic_write_seq(dsi, 0xb2, 0x00, 0x0e, 0x00, 0x0e);
	dsi_generic_write_seq(dsi, 0xb6,
			      0x98, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
			      0x55, 0x54, 0x20, 0x00, 0x0a, 0xaa, 0xaf, 0x0f,
			      0x02, 0x22, 0x22, 0x10);
	dsi_generic_write_seq(dsi, 0xb5, 0x41);
	dsi_generic_write_seq(dsi, 0xf7, 0x03);
	dsi_generic_write_seq(dsi, 0xf7, 0x00);
	dsi_generic_write_seq(dsi, 0xb0, 0x02);
	dsi_generic_write_seq(dsi, 0xfd, 0x0a);
	dsi_generic_write_seq(dsi, 0xfe, 0x80);
	dsi_generic_write_seq(dsi, 0xfe, 0x00);
	dsi_generic_write_seq(dsi, 0x35, 0x00);
	dsi_generic_write_seq(dsi, 0xbd, 0x05, 0x02, 0x02);
	dsi_generic_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	dsi_generic_write_seq(dsi, 0xfc, 0xa5, 0xa5);

	return 0;
}

static int mdss_dsi_samsung_1080p_cmd_fa2_off(struct mdss_dsi_samsung_1080p_cmd_fa2 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	msleep(10);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	return 0;
}

static int mdss_dsi_samsung_1080p_cmd_fa2_prepare(struct drm_panel *panel)
{
	struct mdss_dsi_samsung_1080p_cmd_fa2 *ctx = to_mdss_dsi_samsung_1080p_cmd_fa2(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	mdss_dsi_samsung_1080p_cmd_fa2_reset(ctx);

	ret = mdss_dsi_samsung_1080p_cmd_fa2_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 0);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int mdss_dsi_samsung_1080p_cmd_fa2_unprepare(struct drm_panel *panel)
{
	struct mdss_dsi_samsung_1080p_cmd_fa2 *ctx = to_mdss_dsi_samsung_1080p_cmd_fa2(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = mdss_dsi_samsung_1080p_cmd_fa2_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 0);

	ctx->prepared = false;
	return 0;
}

static int mdss_dsi_samsung_1080p_cmd_fa2_enable(struct drm_panel *panel)
{
	struct mdss_dsi_samsung_1080p_cmd_fa2 *ctx = to_mdss_dsi_samsung_1080p_cmd_fa2(panel);
	int ret;

	if (ctx->enabled)
		return 0;

	ret = backlight_enable(ctx->backlight);
	if (ret < 0) {
		dev_err(&ctx->dsi->dev, "Failed to enable backlight: %d\n", ret);
		return ret;
	}

	ctx->enabled = true;
	return 0;
}

static int mdss_dsi_samsung_1080p_cmd_fa2_disable(struct drm_panel *panel)
{
	struct mdss_dsi_samsung_1080p_cmd_fa2 *ctx = to_mdss_dsi_samsung_1080p_cmd_fa2(panel);
	int ret;

	if (!ctx->enabled)
		return 0;

	ret = backlight_disable(ctx->backlight);
	if (ret < 0) {
		dev_err(&ctx->dsi->dev, "Failed to disable backlight: %d\n", ret);
		return ret;
	}

	ctx->enabled = false;
	return 0;
}

static const struct drm_display_mode mdss_dsi_samsung_1080p_cmd_fa2_mode = {
	.clock = (1080 + 162 + 10 + 36) * (1920 + 13 + 2 + 3) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 162,
	.hsync_end = 1080 + 162 + 10,
	.htotal = 1080 + 162 + 10 + 36,
	.vdisplay = 1920,
	.vsync_start = 1920 + 13,
	.vsync_end = 1920 + 13 + 2,
	.vtotal = 1920 + 13 + 2 + 3,
	.vrefresh = 60,
	.width_mm = 65,
	.height_mm = 115,
};

static int mdss_dsi_samsung_1080p_cmd_fa2_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &mdss_dsi_samsung_1080p_cmd_fa2_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	panel->connector->display_info.width_mm = mode->width_mm;
	panel->connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(panel->connector, mode);

	return 1;
}

static const struct drm_panel_funcs mdss_dsi_samsung_1080p_cmd_fa2_panel_funcs = {
	.disable = mdss_dsi_samsung_1080p_cmd_fa2_disable,
	.unprepare = mdss_dsi_samsung_1080p_cmd_fa2_unprepare,
	.prepare = mdss_dsi_samsung_1080p_cmd_fa2_prepare,
	.enable = mdss_dsi_samsung_1080p_cmd_fa2_enable,
	.get_modes = mdss_dsi_samsung_1080p_cmd_fa2_get_modes,
};

static int mdss_dsi_samsung_1080p_cmd_fa2_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct mdss_dsi_samsung_1080p_cmd_fa2 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		ret = PTR_ERR(ctx->reset_gpio);
		dev_err(dev, "Failed to get reset-gpios: %d\n", ret);
		return ret;
	}

	ctx->backlight = devm_of_find_backlight(dev);
	if (IS_ERR(ctx->backlight)) {
		ret = PTR_ERR(ctx->backlight);
		dev_err(dev, "Failed to get backlight: %d\n", ret);
		return ret;
	}

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &mdss_dsi_samsung_1080p_cmd_fa2_panel_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0) {
		dev_err(dev, "Failed to add panel: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		return ret;
	}

	return 0;
}

static int mdss_dsi_samsung_1080p_cmd_fa2_remove(struct mipi_dsi_device *dsi)
{
	struct mdss_dsi_samsung_1080p_cmd_fa2 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id mdss_dsi_samsung_1080p_cmd_fa2_of_match[] = {
	{ .compatible = "samsung,s6e3fa2" },
	{ }
};
MODULE_DEVICE_TABLE(of, mdss_dsi_samsung_1080p_cmd_fa2_of_match);

static struct mipi_dsi_driver mdss_dsi_samsung_1080p_cmd_fa2_driver = {
	.probe = mdss_dsi_samsung_1080p_cmd_fa2_probe,
	.remove = mdss_dsi_samsung_1080p_cmd_fa2_remove,
	.driver = {
		.name = "panel-mdss-dsi-samsung-1080p-cmd-fa2",
		.of_match_table = mdss_dsi_samsung_1080p_cmd_fa2_of_match,
	},
};
module_mipi_dsi_driver(mdss_dsi_samsung_1080p_cmd_fa2_driver);

MODULE_AUTHOR("Samuel Pascua <pascua.samuel.14@gmail.com>");
MODULE_DESCRIPTION("Panel driver for AMS510CV10 with S6E3FA2 controller");
MODULE_LICENSE("GPL v2");
