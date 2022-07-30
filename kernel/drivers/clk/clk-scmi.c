// SPDX-License-Identifier: GPL-2.0
/*
 * System Control and Power Interface (SCMI) Protocol based clock driver
 *
 * Copyright (C) 2018 ARM Ltd.
 */

#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/scmi_protocol.h>
#include <linux/slab.h>
#include <asm/div64.h>

struct scmi_clk {
	u32 id;
	struct clk_hw hw;
	const struct scmi_clock_info *info;
	const struct scmi_handle *handle;
};

#define to_scmi_clk(clk) container_of(clk, struct scmi_clk, hw)

static unsigned long scmi_clk_recalc_rate(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	int ret;
	u64 rate;
	struct scmi_clk *clk = to_scmi_clk(hw);

	ret = clk->handle->clk_ops->rate_get(clk->handle, clk->id, &rate);
	if (ret)
		return 0;
	return rate;
}

static long scmi_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate)
{
	u64 fmin, fmax, ftmp;
	struct scmi_clk *clk = to_scmi_clk(hw);

	/*
	 * We can't figure out what rate it will be, so just return the
	 * rate back to the caller. scmi_clk_recalc_rate() will be called
	 * after the rate is set and we'll know what rate the clock is
	 * running at then.
	 */
	if (clk->info->rate_discrete)
		return rate;

	fmin = clk->info->range.min_rate;
	fmax = clk->info->range.max_rate;
	if (rate <= fmin)
		return fmin;
	else if (rate >= fmax)
		return fmax;

	ftmp = rate - fmin;
	ftmp += clk->info->range.step_size - 1; /* to round up */
	do_div(ftmp, clk->info->range.step_size);

	return ftmp * clk->info->range.step_size + fmin;
}

static int scmi_clk_set_rate(struct clk_hw *hw, unsigned long rate,
			     unsigned long parent_rate)
{
	struct scmi_clk *clk = to_scmi_clk(hw);

	return clk->handle->clk_ops->rate_set(clk->handle, clk->id, rate);
}

static int scmi_clk_enable(struct clk_hw *hw)
{
	struct scmi_clk *clk = to_scmi_clk(hw);

	return clk->handle->clk_ops->enable(clk->handle, clk->id);
}

static void scmi_clk_disable(struct clk_hw *hw)
{
	struct scmi_clk *clk = to_scmi_clk(hw);

	clk->handle->clk_ops->disable(clk->handle, clk->id);
}

static const struct clk_ops scmi_clk_ops = {
	.recalc_rate = scmi_clk_recalc_rate,
	.round_rate = scmi_clk_round_rate,
	.set_rate = scmi_clk_set_rate,
	/*
	 * We can't provide enable/disable callback as we can't perform the same
	 * in atomic context. Since the clock framework provides standard API
	 * clk_prepare_enable that helps cases using clk_enable in non-atomic
	 * context, it should be fine providing prepare/unprepare.
	 */
	.prepare = scmi_clk_enable,
	.unprepare = scmi_clk_disable,
};

static int scmi_clk_ops_init(struct device *dev, struct scmi_clk *sclk)
{
	unsigned long min_rate, max_rate;
	struct clk *clk;

	struct clk_init_data init = {
		.flags = CLK_GET_RATE_NOCACHE | CLK_IS_ROOT,
		.num_parents = 0,
		.ops = &scmi_clk_ops,
		.name = sclk->info->name,
	};

	sclk->hw.init = &init;
	clk = devm_clk_register(dev, &sclk->hw);
	if (IS_ERR(clk))
		return -EINVAL;

	if (sclk->info->rate_discrete) {
		int num_rates = sclk->info->list.num_rates;

		if (num_rates <= 0)
			return -EINVAL;

		min_rate = sclk->info->list.rates[0];
		max_rate = sclk->info->list.rates[num_rates - 1];
	} else {
		min_rate = sclk->info->range.min_rate;
		max_rate = sclk->info->range.max_rate;
	}

	clk_hw_set_rate_range(&sclk->hw, min_rate, max_rate);
	return 0;
}

static int scmi_clocks_probe(struct scmi_device *sdev)
{
	int idx, count, err;
	struct clk **clks;
	struct clk_onecell_data *clk_data;
	struct device *dev = &sdev->dev;
	struct device_node *np = dev->of_node;
	const struct scmi_handle *handle = sdev->handle;

	if (!handle || !handle->clk_ops)
		return -ENODEV;

	count = handle->clk_ops->count_get(handle);
	if (count < 0) {
		dev_err(dev, "%s: invalid clock output count\n", np->name);
		return -EINVAL;
	}

	clk_data = devm_kzalloc(dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->clks = devm_kzalloc(dev, sizeof(*clk_data->clks) * count,
				      GFP_KERNEL);
	if (!clk_data->clks)
		return -ENOMEM;

	clk_data->clk_num = count;
	clks = clk_data->clks;

	for (idx = 0; idx < count; idx++) {
		struct scmi_clk *sclk;

		sclk = devm_kzalloc(dev, sizeof(*sclk), GFP_KERNEL);
		if (!sclk)
			return -ENOMEM;

		sclk->info = handle->clk_ops->info_get(handle, idx);
		if (!sclk->info) {
			dev_info(dev, "invalid clock info for idx %d\n", idx);
			continue;
		}

		sclk->id = idx;
		sclk->handle = handle;

		err = scmi_clk_ops_init(dev, sclk);
		if (err) {
			dev_err(dev, "failed to register clock %d\n", idx);
			kfree(sclk);
			clks[idx] = NULL;
		} else {
			dev_info(dev, "Registered clock:%s\n", sclk->info->name);
			clks[idx] = sclk->hw.clk;
		}
	}

	return of_clk_add_provider(np, of_clk_src_onecell_get,
					   clk_data);
}

static const struct scmi_device_id scmi_id_table[] = {
	{ SCMI_PROTOCOL_CLOCK },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_clocks_driver = {
	.name = "scmi-clocks",
	.probe = scmi_clocks_probe,
	.id_table = scmi_id_table,
};
#ifdef CONFIG_ARCH_ROCKCHIP
static int __init scmi_clocks_driver_init(void)
{
	return scmi_register(&scmi_clocks_driver);
}
subsys_initcall_sync(scmi_clocks_driver_init);

static void __exit scmi_clocks_driver_exit(void)
{
	scmi_unregister(&scmi_clocks_driver);
}
module_exit(scmi_clocks_driver_exit);
#else
module_scmi_driver(scmi_clocks_driver);
#endif

MODULE_AUTHOR("Sudeep Holla <sudeep.holla@arm.com>");
MODULE_DESCRIPTION("ARM SCMI clock driver");
MODULE_LICENSE("GPL v2");
