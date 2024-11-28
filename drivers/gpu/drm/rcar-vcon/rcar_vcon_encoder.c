// SPDX-License-Identifier: GPL-2.0+
/*
 * rcar_vcon_encoder.c  --  R-Car Video Converter Encoder
 *
 * Copyright (C) 2023-2024 Renesas Electronics Corporation
 *
 */

#include <linux/export.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>

#include <drm/drm_bridge.h>
#include <drm/drm_crtc.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_panel.h>
#include <drm/drm_simple_kms_helper.h>

#include "rcar_vcon_drv.h"
#include "rcar_vcon_encoder.h"
#include "rcar_vcon_kms.h"

/* -----------------------------------------------------------------------------
 * Encoder
 */

static const struct drm_encoder_funcs rcar_vcon_encoder_funcs = {
};

int rcar_vcon_encoder_init(struct rcar_vcon_device *rvcon,
			   enum rcar_vcon_output output,
			   struct device_node *enc_node)
{
	struct rcar_vcon_encoder *renc;
	struct drm_bridge *bridge;
	struct device_node *node;
	int ret;

	switch (output) {
	case RCAR_VCON_OUTPUT_DP0:
	case RCAR_VCON_OUTPUT_DP1:
	case RCAR_VCON_OUTPUT_DP2:
	case RCAR_VCON_OUTPUT_DP3:
		node = of_graph_get_port_by_id(enc_node, rvcon->info->routes[output].port);
		if (!node)
			return -ENODEV;

		of_node_put(node);

		break;
	default:
		node = enc_node;
	}

	/*
	 * create a panel bridge.
	 */
	bridge = of_drm_find_bridge(node);
	if (!bridge) {
		if (output == RCAR_VCON_OUTPUT_DP0 ||
		    output == RCAR_VCON_OUTPUT_DP1 ||
		    output == RCAR_VCON_OUTPUT_DP2 ||
		    output == RCAR_VCON_OUTPUT_DP3) {
#if IS_ENABLED(CONFIG_DRM_RCAR_DW_DP)
			ret = -EPROBE_DEFER;
#else
			ret = 0;
#endif
			goto done;
		} else {
			ret = EOPNOTSUPP;

			goto done;
		}
	}

	dev_dbg(rvcon->dev, "initializing encoder %pOF for output %u\n",
		node, output);

	renc = drmm_encoder_alloc(&rvcon->ddev, struct rcar_vcon_encoder, base,
				  &rcar_vcon_encoder_funcs, DRM_MODE_ENCODER_NONE,
				  NULL);
	if (IS_ERR(renc))
		return PTR_ERR(renc);

	renc->output = output;
	renc->bridge = bridge;

	/*
	 * Attach the bridge to the encoder. The bridge will create the
	 * connector.
	 */
	ret = drm_bridge_attach(&renc->base, bridge, NULL, 0);

done:
	return ret;
}
