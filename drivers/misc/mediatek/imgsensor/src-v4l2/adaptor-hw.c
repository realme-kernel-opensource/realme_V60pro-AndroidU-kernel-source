// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

//#define DEBUG

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>

#include "kd_imgsensor_define_v4l2.h"
#include "adaptor.h"
#include "adaptor-hw.h"
#include <linux/clk-provider.h>

#define INST_OPS(__ctx, __field, __idx, __hw_id, __set, __unset) do {\
	if (__ctx->__field[__idx]) { \
		__ctx->hw_ops[__hw_id].set = __set; \
		__ctx->hw_ops[__hw_id].unset = __unset; \
		__ctx->hw_ops[__hw_id].data = (void *)__idx; \
	} \
} while (0)

static const char * const clk_names[] = {
	ADAPTOR_CLK_NAMES
};

static const char * const reg_names[] = {
	ADAPTOR_REGULATOR_NAMES
};

static const char * const state_names[] = {
	ADAPTOR_STATE_NAMES
};

static struct clk *get_clk_by_idx_freq(struct adaptor_ctx *ctx,
				unsigned long long idx, int freq)
{
	if (idx == CLK_MCLK) {
		switch (freq) {
		case 6:
			return ctx->clk[CLK_6M];
		case 12:
			return ctx->clk[CLK_12M];
		case 13:
			return ctx->clk[CLK_13M];
		case 19:
			return ctx->clk[CLK_19_2M];
		case 24:
			return ctx->clk[CLK_24M];
		case 26:
			return ctx->clk[CLK_26M];
		case 52:
			return ctx->clk[CLK_52M];
		}
	} else if (idx == CLK1_MCLK1) {
		switch (freq) {
		case 6:
			return ctx->clk[CLK1_6M];
		case 12:
			return ctx->clk[CLK1_12M];
		case 13:
			return ctx->clk[CLK1_13M];
		case 19:
			return ctx->clk[CLK1_19_2M];
		case 24:
			return ctx->clk[CLK1_24M];
		case 26:
			if (ctx->aov_mclk_ulposc_flag)
				return ctx->clk[CLK1_26M_ULPOSC];
			else
				return ctx->clk[CLK1_26M];
		case 52:
			return ctx->clk[CLK1_52M];
		}
	}

	return NULL;
}

static int set_mclk(struct adaptor_ctx *ctx, void *data, int val)
{
	int ret;
	struct clk *mclk, *mclk_src;
	unsigned long long idx;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};
	scnprintf(payload, sizeof(payload), "NULL$$EventField@@%s$$FieldData@@0x=%x$$detailData@@sn=%s",
		acquireEventField(EXCEP_CLOCK), (CAM_RESERVED_ID << 20 | CAM_MODULE_ID << 12 | EXCEP_CLOCK), ctx->subdrv->name);
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	idx = (unsigned long long)data;
	mclk = ctx->clk[idx];
	mclk_src = get_clk_by_idx_freq(ctx, idx, val);
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]+ idx(%llu),val(%d)\n", __func__, idx, val);
#endif
	ret = clk_prepare_enable(mclk);
	if (ret) {
		dev_info(ctx->dev,
			"clk_prepare_enable(%s),ret(%d)(fail)\n",
			clk_names[idx], ret);
		#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
		cam_olc_raise_exception(EXCEP_CLOCK, payload);
		#endif /* OPLUS_FEATURE_CAMERA_COMMON */
		return ret;
	}
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev,
		"clk_prepare_enable(%s),ret(%d)(correct)\n",
		clk_names[idx], ret);
#endif
	ret = clk_set_parent(mclk, mclk_src);
	if (ret) {
		dev_info(ctx->dev,
			"mclk(%s) clk_set_parent (%s),ret(%d)(fail)\n",
			__clk_get_name(mclk), __clk_get_name(mclk_src), ret);
		return ret;
	}
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev,
		"[%s]- clk_set_parent(%s),ret(%d)(correct)\n",
		__func__, __clk_get_name(mclk_src), ret);
#endif
	return 0;
}

static int unset_mclk(struct adaptor_ctx *ctx, void *data, int val)
{
	struct clk *mclk, *mclk_src;
	unsigned long long idx;

	idx = (unsigned long long)data;
	mclk = ctx->clk[idx];
	mclk_src = get_clk_by_idx_freq(ctx, idx, val);
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]+ idx(%llu),val(%d)\n", __func__, idx, val);
#endif
	clk_disable_unprepare(mclk);
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev,
		"[%s]- clk_disable_unprepare(%s)\n",
		__func__, clk_names[idx]);
#endif
	return 0;
}

static int set_reg(struct adaptor_ctx *ctx, void *data, int val)
{
	unsigned long long ret, idx;
	struct regulator *reg;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};
	#endif  /* OPLUS_FEATURE_CAMERA_COMMON */

	idx = (unsigned long long)data;

#ifdef OPLUS_FEATURE_CAMERA_COMMON
	/*Added by rentianzhi@CamDrv, release the hw resource for Explorer AON driver, 20220124*/
	if (ctx->support_explorer_aon_fl == 0) {
		// re-get reg everytime due to pmic limitation
		if(ctx->regulator[idx] == NULL) {
			ctx->regulator[idx] = devm_regulator_get_optional(ctx->dev, reg_names[idx]);
			if (IS_ERR(ctx->regulator[idx])) {
				ctx->regulator[idx] = NULL;
				dev_dbg(ctx->dev, "no reg %s\n", reg_names[idx]);
				return -EINVAL;
			}
		}
	}
#else /*OPLUS_FEATURE_CAMERA_COMMON*/
	if(ctx->regulator[idx] == NULL) {
		// re-get reg everytime due to pmic limitation
		ctx->regulator[idx] = devm_regulator_get_optional(ctx->dev, reg_names[idx]);
		if (IS_ERR(ctx->regulator[idx])) {
			ctx->regulator[idx] = NULL;
			dev_dbg(ctx->dev, "no reg %s\n", reg_names[idx]);
			return -EINVAL;
		}
	}
#endif /*OPLUS_FEATURE_CAMERA_COMMON*/

	reg = ctx->regulator[idx];
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]+ idx(%llu),val(%d)\n", __func__, idx, val);
#endif
#ifdef OPLUS_FEATURE_CAMERA_COMMON
	if(val == 0) {
		ret = regulator_disable(reg);
		if (ctx->support_explorer_aon_fl == 0) {
			devm_regulator_put(ctx->regulator[idx]);
			ctx->regulator[idx] = NULL;
		}
		dev_info(ctx->dev, "%s regulator_disable ret:%d  support_explorer_aon_fl(%d)",
			__func__, ret, ctx->support_explorer_aon_fl);
		return ret;
	}
#endif
	ret = regulator_set_voltage(reg, val, val);
	if (ret) {
		#ifndef OPLUS_FEATURE_CAMERA_COMMON
		dev_dbg(ctx->dev,
			"regulator_set_voltage(%s),val(%d),ret(%llu)(fail)\n",
			reg_names[idx], val, ret);
		#else /*OPLUS_FEATURE_CAMERA_COMMON*/
		dev_err(ctx->dev,
			"regulator_set_voltage(%s),val(%d),ret(%llu)(fail)\n",
			reg_names[idx], val, ret);
		#endif /*OPLUS_FEATURE_CAMERA_COMMON*/
	}
#if IMGSENSOR_LOG_MORE
	else
		dev_info(ctx->dev,
			"regulator_set_voltage(%s),val(%d),ret(%llu)(correct)\n",
			reg_names[idx], val, ret);
#endif
	ret = regulator_enable(reg);
	if (ret) {
		#ifndef OPLUS_FEATURE_CAMERA_COMMON
		dev_dbg(ctx->dev,
			"regulator_enable(%s),ret(%llu)(fail)\n",
			reg_names[idx], ret);
		#else /*OPLUS_FEATURE_CAMERA_COMMON*/
		dev_err(ctx->dev,
			"regulator_enable(%s),ret(%llu)(fail)\n",
			reg_names[idx], ret);
		#endif /*OPLUS_FEATURE_CAMERA_COMMON*/
		#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
		scnprintf(payload, sizeof(payload), "NULL$$EventField@@%s$$FieldData@@0x%x$$detailData@@sn=%s, reg_names=%s",
			acquireEventField(EXCEP_VOLTAGE), (CAM_RESERVED_ID << 20 | CAM_MODULE_ID << 12 | EXCEP_VOLTAGE),
			ctx->subdrv->name, reg_names[idx]);
		cam_olc_raise_exception(EXCEP_VOLTAGE, payload);
		#endif /* OPLUS_FEATURE_CAMERA_COMMON */
		return ret;
	}
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev,
		"[%s]- regulator_enable(%s),ret(%llu)(correct)\n",
		__func__, reg_names[idx], ret);
#endif
	return 0;
}

static int unset_reg(struct adaptor_ctx *ctx, void *data, int val)
{
	unsigned long long ret, idx;
	struct regulator *reg;

	idx = (unsigned long long)data;
	reg = ctx->regulator[idx];
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]+ idx(%llu),val(%d)\n", __func__, idx, val);
#endif
	ret = regulator_disable(reg);
	if (ret) {
		#ifndef OPLUS_FEATURE_CAMERA_COMMON
		dev_dbg(ctx->dev,
			"disable(%s),ret(%llu)(fail)\n",
			reg_names[idx], ret);
		#else /*OPLUS_FEATURE_CAMERA_COMMON*/
		dev_err(ctx->dev,
			"disable(%s),ret(%llu)(fail)\n",
			reg_names[idx], ret);
		#endif /*OPLUS_FEATURE_CAMERA_COMMON*/
		return ret;
	}

#ifdef OPLUS_FEATURE_CAMERA_COMMON
	/*Added by rentianzhi@CamDrv, release the hw resource for Explorer AON driver, 20220124*/
	if (ctx->support_explorer_aon_fl == 0) {
		// always put reg due to pmic limitation
		devm_regulator_put(ctx->regulator[idx]);
		ctx->regulator[idx] = NULL;
	}
#else /*OPLUS_FEATURE_CAMERA_COMMON*/
	// always put reg due to pmic limitation
	devm_regulator_put(ctx->regulator[idx]);
	ctx->regulator[idx] = NULL;
#endif /*OPLUS_FEATURE_CAMERA_COMMON*/

#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev,
		"[%s]- disable(%s),ret(%llu)(correct)\n",
		__func__, reg_names[idx], ret);
#endif
	return 0;
}

static int set_state(struct adaptor_ctx *ctx, void *data, int val)
{
	unsigned long long idx, x;
	int ret;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	idx = (unsigned long long)data;
	x = idx + val;
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]+ idx(%llu),val(%d)\n", __func__, idx, val);
#endif
	ret = pinctrl_select_state(ctx->pinctrl, ctx->state[x]);
	if (ret < 0) {
		dev_info(ctx->dev,
			"select(%s),ret(%d)(fail)\n",
			state_names[x], ret);
		#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
		scnprintf(payload, sizeof(payload), "NULL$$EventField@@%s$$FieldData@@0x%x$$detailData@@sn=%s, state_names=%s",
			acquireEventField(EXCEP_GPIO), (CAM_RESERVED_ID << 20 | CAM_MODULE_ID << 12 | EXCEP_GPIO),
			ctx->subdrv->name, state_names[idx]);
		cam_olc_raise_exception(EXCEP_GPIO, payload);
		#endif /* OPLUS_FEATURE_CAMERA_COMMON */
		return ret;
	}
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev,
		"[%s]- select(%s),ret(%d)(correct)\n",
		__func__, state_names[x], ret);
#endif
	return 0;
}

static int unset_state(struct adaptor_ctx *ctx, void *data, int val)
{
	return set_state(ctx, data, 0);
}

static int set_state_div2(struct adaptor_ctx *ctx, void *data, int val)
{
	return set_state(ctx, data, val >> 1);
}

static int set_state_boolean(struct adaptor_ctx *ctx, void *data, int val)
{
	return set_state(ctx, data, !!val);
}

static int set_state_mipi_switch(struct adaptor_ctx *ctx, void *data, int val)
{
	return set_state(ctx, (void *)STATE_MIPI_SWITCH_ON, 0);
}

static int unset_state_mipi_switch(struct adaptor_ctx *ctx, void *data,
	int val)
{
	return set_state(ctx, (void *)STATE_MIPI_SWITCH_OFF, 0);
}

static int reinit_pinctrl(struct adaptor_ctx *ctx)
{
	int i;
	struct device *dev = ctx->dev;
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]+\n", __func__);
#endif
	/* pinctrl */
	ctx->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ctx->pinctrl)) {
		dev_err(dev, "fail to get pinctrl\n");
		return PTR_ERR(ctx->pinctrl);
	}

	/* pinctrl states */
	for (i = 0; i < STATE_MAXCNT; i++) {
		ctx->state[i] = pinctrl_lookup_state(
				ctx->pinctrl, state_names[i]);
		if (IS_ERR(ctx->state[i])) {
			ctx->state[i] = NULL;
			dev_dbg(dev, "no state %s\n", state_names[i]);
		}
	}
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]-\n", __func__);
#endif
	return 0;
}
#ifdef OPLUS_FEATURE_CAMERA_COMMON
/*Added by rentianzhi@CamDrv, release the hw resource for Explorer AON driver, 20220124*/
static int reinit_supply(struct adaptor_ctx *ctx)
{
	int i;
	struct device *dev = ctx->dev;
	/* supplies */
	for (i = 0; i < REGULATOR_MAXCNT; i++) {
		ctx->regulator[i] = devm_regulator_get_optional(
				dev, reg_names[i]);
		if (IS_ERR(ctx->regulator[i])) {
			ctx->regulator[i] = NULL;
			dev_dbg(dev, "In %s:no reg %s\n", __func__, reg_names[i]);
		}
	}
	return 0;
}

static int reinit_clk(struct adaptor_ctx *ctx)
{
	int i;
	struct device *dev = ctx->dev;
	/* clocks */
	for (i = 0; i < CLK_MAXCNT; i++) {
		ctx->clk[i] = devm_clk_get(dev, clk_names[i]);
		if (IS_ERR(ctx->clk[i])) {
			ctx->clk[i] = NULL;
			dev_dbg(dev, "In %s:no clk %s\n", __func__, clk_names[i]);
		}
	}
	return 0;
}
int aon_hw_power_status = 0;
int cam_hw_power_status = 0;
int adaptor_hw_aon_power_on(struct adaptor_ctx *ctx)
{
    int i;
    const struct subdrv_pw_seq_entry *ent;
    struct adaptor_hw_ops *op;

    dev_info(ctx->dev, "[%s]+\n", __func__);
    if (cam_hw_power_status == 1) {
        dev_info(ctx->dev, "[%s] cam already powered skip\n", __func__);
        return 0;
    }
    if (aon_hw_power_status == 1) {
        dev_info(ctx->dev, "[%s] aon already powered skip\n", __func__);
        return 0;
    }
    aon_hw_power_status = 1;

    /* may be released for mipi switch */
    if (!ctx->pinctrl)
        reinit_pinctrl(ctx);

    /*Added by rentianzhi@CamDrv, release the hw resource for Explorer AON driver, 20220124*/
    if (ctx->support_explorer_aon_fl == 1 && ctx->idx == IMGSENSOR_SENSOR_IDX_SUB) {
        dev_info(ctx->dev, "In %s:start to reinit supply and clk for front sensor.\n", __func__);
        /*reinit the supply*/
        reinit_supply(ctx);
        /*reinit the clk*/
        reinit_clk(ctx);
    }

    for (i = 0; i < ctx->subdrv->aon_pw_seq_cnt; i++) {
        ent = &ctx->subdrv->aon_pw_seq[i];
        op = &ctx->hw_ops[ent->id];
        if (!op->set) {
            dev_dbg(ctx->dev, "cannot set comp %d val %d\n",
                ent->id, ent->val);
            continue;
        }
        op->set(ctx, op->data, ent->val);
        if (ent->delay)
            mdelay(ent->delay);
    }
    dev_info(ctx->dev, "[%s]-\n", __func__);
    return 0;
}

int adaptor_hw_aon_power_off(struct adaptor_ctx *ctx)
{
    int i;
    const struct subdrv_pw_seq_entry *ent;
    struct adaptor_hw_ops *op;
    dev_info(ctx->dev, "[%s]+\n", __func__);
    if (cam_hw_power_status == 1) {
        dev_info(ctx->dev, "[%s] cam already powered skip\n", __func__);
        return 0;
    }

    if (aon_hw_power_status == 0) {
        dev_info(ctx->dev, "[%s] aon already powerdown skip\n", __func__);
        return 0;
    }
    aon_hw_power_status = 0;

    for (i = ctx->subdrv->aon_pw_seq_cnt - 1; i >= 0; i--) {
        ent = &ctx->subdrv->aon_pw_seq[i];
        op = &ctx->hw_ops[ent->id];
        if (!op->unset)
            continue;
        if (ent->id == HW_ID_MCLK)
            continue;
        op->unset(ctx, op->data, ent->val);
        //msleep(ent->delay);
    }

    /*Added by rentianzhi@CamDrv, release the hw resource for Explorer AON driver, 20220124*/
    if (ctx->support_explorer_aon_fl == 1 && ctx->idx == IMGSENSOR_SENSOR_IDX_SUB) {
        dev_dbg(ctx->dev, "In adaptor_hw_power_off:Start to release the supply and clk resource for front camera.\n");
        /* clocks */
        for (i = 0; i < CLK_MAXCNT; i++) {
            if (ctx->clk[i] != NULL) {
                devm_clk_put(ctx->dev, ctx->clk[i]);
                ctx->clk[i] = NULL;
                dev_dbg(ctx->dev, "In %s:Release clk:%s ok.\n", __func__, clk_names[i]);
            }
        }
        /* supplies */
        for (i = 0; i < REGULATOR_MAXCNT; i++) {
            if (ctx->regulator[i] != NULL) {
                devm_regulator_put(ctx->regulator[i]);
                ctx->regulator[i] = NULL;
                dev_dbg(ctx->dev, "In %s:Release regulator:%s ok.\n", __func__, reg_names[i]);
            }
        }
        /*pinctrl*/
        devm_pinctrl_put(ctx->pinctrl);
        ctx->pinctrl = NULL;
    }
    dev_info(ctx->dev, "[%s]-\n", __func__);
    return 0;
}

#endif
int do_hw_power_on(struct adaptor_ctx *ctx)
{
	int i;
	const struct subdrv_pw_seq_entry *ent;
	struct adaptor_hw_ops *op;
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]+\n", __func__);
#endif
#ifdef OPLUS_FEATURE_CAMERA_COMMON
	/*Added by fujiahao@CamDrv, check aon status, 20221110*/
	mutex_lock(&ctx->hw_mutex);
	if (aon_hw_power_status == 1) {
		dev_info(ctx->dev, "[%s] aon already powered powerdown aon\n", __func__);
		adaptor_hw_aon_power_off(ctx);
	}
	cam_hw_power_status = 1;
#endif
	if (ctx->sensor_ws) {
		if (ctx->aov_pm_ops_flag == 0) {
			ctx->aov_pm_ops_flag = 1;
			__pm_stay_awake(ctx->sensor_ws);
		}
	}
#if IMGSENSOR_LOG_MORE
	else
		dev_info(ctx->dev, "__pm_stay_awake(fail)\n");
#endif
	/* may be released for mipi switch */
	if (!ctx->pinctrl)
		reinit_pinctrl(ctx);
#ifdef OPLUS_FEATURE_CAMERA_COMMON
	/*Added by rentianzhi@CamDrv, release the hw resource for Explorer AON driver, 20220124*/
	if (ctx->support_explorer_aon_fl == 1 && ctx->idx == IMGSENSOR_SENSOR_IDX_SUB) {
		dev_info(ctx->dev, "In %s:start to reinit supply and clk for front sensor.\n", __func__);
		/*reinit the supply*/
		reinit_supply(ctx);
		/*reinit the clk*/
		reinit_clk(ctx);
	}
#endif

	op = &ctx->hw_ops[HW_ID_MIPI_SWITCH];
	if (op->set)
		op->set(ctx, op->data, 0);

	for (i = 0; i < ctx->subdrv->pw_seq_cnt; i++) {
		if (ctx->ctx_pw_seq)
			ent = &ctx->ctx_pw_seq[i]; // use ctx pw seq
		else
			ent = &ctx->subdrv->pw_seq[i];
		op = &ctx->hw_ops[ent->id];
		if (!op->set) {
			dev_dbg(ctx->dev, "cannot set comp %d val %d\n",
				ent->id, ent->val);
			continue;
		}
		op->set(ctx, op->data, ent->val);
		if (ent->delay)
			mdelay(ent->delay);
	}

	if (ctx->subdrv->ops->power_on)
		subdrv_call(ctx, power_on, NULL);

#ifdef OPLUS_FEATURE_CAMERA_COMMON
	/*Added by fujiahao@CamDrv, check aon status, 20221110*/
	mutex_unlock(&ctx->hw_mutex);
#endif
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]-\n", __func__);
#endif
	return 0;
}

int adaptor_hw_power_on(struct adaptor_ctx *ctx)
{
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]+\n", __func__);
#endif
#ifndef IMGSENSOR_USE_PM_FRAMEWORK
	dev_dbg(ctx->dev, "%s power ref cnt = %d\n", __func__, ctx->power_refcnt);
	ctx->power_refcnt++;
	if (ctx->power_refcnt > 1) {
		dev_dbg(ctx->dev, "%s already powered, cnt = %d\n", __func__, ctx->power_refcnt);
		return 0;
	}
#endif
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]-\n", __func__);
#endif
	return do_hw_power_on(ctx);
}

int do_hw_power_off(struct adaptor_ctx *ctx)
{
	int i;
	const struct subdrv_pw_seq_entry *ent;
	struct adaptor_hw_ops *op;
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]+\n", __func__);
#endif
#ifdef OPLUS_FEATURE_CAMERA_COMMON
	/*Added by fujiahao@CamDrv, update cam status, 20221110*/
	mutex_lock(&ctx->hw_mutex);
	cam_hw_power_status = 0;
#endif

	/* call subdrv close function before pwr off */
	subdrv_call(ctx, close);

	if (ctx->subdrv->ops->power_off)
		subdrv_call(ctx, power_off, NULL);

#ifdef OPLUS_FEATURE_CAMERA_COMMON
	if(ctx->subdrv->pw_off_seq != NULL) {
		for (i = ctx->subdrv->pw_off_seq_cnt - 1; i >= 0; i--) {
			ent = &ctx->subdrv->pw_off_seq[i];
			op = &ctx->hw_ops[ent->id];
			if (!op->unset)
				continue;
			op->unset(ctx, op->data, ent->val);
			//msleep(ent->delay);
		}
	} else {
#endif
		for (i = ctx->subdrv->pw_seq_cnt - 1; i >= 0; i--) {
			if (ctx->ctx_pw_seq)
				ent = &ctx->ctx_pw_seq[i]; // use ctx pw seq
			else
				ent = &ctx->subdrv->pw_seq[i];
			op = &ctx->hw_ops[ent->id];
			if (!op->unset)
				continue;
			op->unset(ctx, op->data, ent->val);
			//msleep(ent->delay);
		}
#ifdef OPLUS_FEATURE_CAMERA_COMMON
	}
#endif

	op = &ctx->hw_ops[HW_ID_MIPI_SWITCH];
	if (op->unset)
		op->unset(ctx, op->data, 0);

	/* the pins of mipi switch are shared. free it for another users */
	if (ctx->state[STATE_MIPI_SWITCH_ON] ||
		ctx->state[STATE_MIPI_SWITCH_OFF]) {
		devm_pinctrl_put(ctx->pinctrl);
		ctx->pinctrl = NULL;
	}

	if (ctx->sensor_ws) {
		if (ctx->aov_pm_ops_flag == 1) {
			ctx->aov_pm_ops_flag = 0;
			__pm_relax(ctx->sensor_ws);
		}
	}
#if IMGSENSOR_LOG_MORE
	else
		dev_info(ctx->dev, "__pm_relax(fail)\n");

	dev_info(ctx->dev, "[%s]-\n", __func__);
#endif
#ifdef OPLUS_FEATURE_CAMERA_COMMON
	/*Added by rentianzhi@CamDrv, release the hw resource for Explorer AON driver, 20220124*/
	if (ctx->support_explorer_aon_fl == 1 && ctx->idx == IMGSENSOR_SENSOR_IDX_SUB) {
		dev_dbg(ctx->dev, "In adaptor_hw_power_off:Start to release the supply and clk resource for front camera.\n");
		/* clocks */
		for (i = 0; i < CLK_MAXCNT; i++) {
			if (ctx->clk[i] != NULL) {
				devm_clk_put(ctx->dev, ctx->clk[i]);
				ctx->clk[i] = NULL;
				dev_dbg(ctx->dev, "In %s:Release clk:%s ok.\n", __func__, clk_names[i]);
			}
		}
		/* supplies */
		for (i = 0; i < REGULATOR_MAXCNT; i++) {
			if (ctx->regulator[i] != NULL) {
				devm_regulator_put(ctx->regulator[i]);
				ctx->regulator[i] = NULL;
				dev_dbg(ctx->dev, "In %s:Release regulator:%s ok.\n", __func__, reg_names[i]);
			}
		}
		/*pinctrl*/
		devm_pinctrl_put(ctx->pinctrl);
		ctx->pinctrl = NULL;
	}
	mutex_unlock(&ctx->hw_mutex);
#endif
	return 0;
}
int adaptor_hw_power_off(struct adaptor_ctx *ctx)
{
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]+\n", __func__);
#endif
#ifndef IMGSENSOR_USE_PM_FRAMEWORK
	if (!ctx->power_refcnt) {
		dev_dbg(ctx->dev, "%s power ref cnt = %d, skip due to not power on yet\n",
			__func__, ctx->power_refcnt);
		return 0;
	}
	dev_dbg(ctx->dev, "%s power ref cnt = %d\n", __func__, ctx->power_refcnt);
	ctx->power_refcnt--;
	if (ctx->power_refcnt > 0) {
		dev_dbg(ctx->dev, "%s skip due to cnt = %d\n", __func__, ctx->power_refcnt);
		return 0;
	}
	ctx->power_refcnt = 0;
	ctx->is_sensor_inited = 0;
	ctx->is_sensor_scenario_inited = 0;
	ctx->is_streaming = 0;
#endif
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]-\n", __func__);
#endif
	return do_hw_power_off(ctx);
}

int adaptor_hw_init(struct adaptor_ctx *ctx)
{
	int i;
	struct device *dev = ctx->dev;
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]+\n", __func__);
#endif
	/* clocks */
	for (i = 0; i < CLK_MAXCNT; i++) {
		ctx->clk[i] = devm_clk_get(dev, clk_names[i]);
		if (IS_ERR(ctx->clk[i])) {
			ctx->clk[i] = NULL;
			dev_dbg(dev, "no clk %s\n", clk_names[i]);
		}
	}

	/* supplies */
	for (i = 0; i < REGULATOR_MAXCNT; i++) {
		ctx->regulator[i] = devm_regulator_get_optional(
				dev, reg_names[i]);
		if (IS_ERR(ctx->regulator[i])) {
			ctx->regulator[i] = NULL;
			dev_dbg(dev, "no reg %s\n", reg_names[i]);
		}
	}

	/* pinctrl */
	ctx->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ctx->pinctrl)) {
		dev_err(dev, "fail to get pinctrl\n");
		return PTR_ERR(ctx->pinctrl);
	}

	/* pinctrl states */
	for (i = 0; i < STATE_MAXCNT; i++) {
		ctx->state[i] = pinctrl_lookup_state(
				ctx->pinctrl, state_names[i]);
		if (IS_ERR(ctx->state[i])) {
			ctx->state[i] = NULL;
			dev_dbg(dev, "no state %s\n", state_names[i]);
		}
	}

	/* install operations */

	INST_OPS(ctx, clk, CLK_MCLK, HW_ID_MCLK, set_mclk, unset_mclk);

	INST_OPS(ctx, clk, CLK1_MCLK1, HW_ID_MCLK1, set_mclk, unset_mclk);

	INST_OPS(ctx, regulator, REGULATOR_AVDD, HW_ID_AVDD,
			set_reg, unset_reg);

	INST_OPS(ctx, regulator, REGULATOR_DVDD, HW_ID_DVDD,
			set_reg, unset_reg);

	INST_OPS(ctx, regulator, REGULATOR_DOVDD, HW_ID_DOVDD,
			set_reg, unset_reg);

	INST_OPS(ctx, regulator, REGULATOR_AFVDD, HW_ID_AFVDD,
			set_reg, unset_reg);

	INST_OPS(ctx, regulator, REGULATOR_AFVDD1, HW_ID_AFVDD1,
			set_reg, unset_reg);

	INST_OPS(ctx, regulator, REGULATOR_AVDD1, HW_ID_AVDD1,
			set_reg, unset_reg);

	INST_OPS(ctx, regulator, REGULATOR_AVDD2, HW_ID_AVDD2,
			set_reg, unset_reg);

	INST_OPS(ctx, regulator, REGULATOR_DVDD1, HW_ID_DVDD1,
			set_reg, unset_reg);
	INST_OPS(ctx, regulator, REGULATOR_RST, HW_ID_RST,
			set_reg, unset_reg);

	if (ctx->state[STATE_MIPI_SWITCH_ON])
		ctx->hw_ops[HW_ID_MIPI_SWITCH].set = set_state_mipi_switch;

	if (ctx->state[STATE_MIPI_SWITCH_OFF])
		ctx->hw_ops[HW_ID_MIPI_SWITCH].unset = unset_state_mipi_switch;

	INST_OPS(ctx, state, STATE_MCLK_OFF, HW_ID_MCLK_DRIVING_CURRENT,
			set_state_div2, unset_state);

	INST_OPS(ctx, state, STATE_MCLK1_OFF, HW_ID_MCLK1_DRIVING_CURRENT,
			set_state_div2, unset_state);

	INST_OPS(ctx, state, STATE_RST_LOW, HW_ID_RST,
			set_state, unset_state);

	INST_OPS(ctx, state, STATE_PDN_LOW, HW_ID_PDN,
			set_state, unset_state);

	INST_OPS(ctx, state, STATE_AVDD_OFF, HW_ID_AVDD,
			set_state_boolean, unset_state);

	INST_OPS(ctx, state, STATE_DVDD_OFF, HW_ID_DVDD,
			set_state_boolean, unset_state);

	INST_OPS(ctx, state, STATE_DOVDD_OFF, HW_ID_DOVDD,
			set_state_boolean, unset_state);

	INST_OPS(ctx, state, STATE_AFVDD_OFF, HW_ID_AFVDD,
			set_state_boolean, unset_state);

	INST_OPS(ctx, state, STATE_AFVDD1_OFF, HW_ID_AFVDD1,
			set_state_boolean, unset_state);

	INST_OPS(ctx, state, STATE_AVDD1_OFF, HW_ID_AVDD1,
			set_state_boolean, unset_state);

	INST_OPS(ctx, state, STATE_AVDD2_OFF, HW_ID_AVDD2,
			set_state_boolean, unset_state);

	INST_OPS(ctx, state, STATE_DVDD1_OFF, HW_ID_DVDD1,
			set_state_boolean, unset_state);
	INST_OPS(ctx, state, STATE_RST1_LOW, HW_ID_RST1,
			set_state, unset_state);

	INST_OPS(ctx, state, STATE_PONV_LOW, HW_ID_PONV,
			set_state, unset_state);

	INST_OPS(ctx, state, STATE_SCL_AP, HW_ID_SCL,
			set_state, unset_state);

	INST_OPS(ctx, state, STATE_SDA_AP, HW_ID_SDA,
			set_state, unset_state);

	INST_OPS(ctx, state, STATE_EINT, HW_ID_EINT,
		 set_state, unset_state);

	/* the pins of mipi switch are shared. free it for another users */
	if (ctx->state[STATE_MIPI_SWITCH_ON] ||
		ctx->state[STATE_MIPI_SWITCH_OFF]) {
		devm_pinctrl_put(ctx->pinctrl);
		ctx->pinctrl = NULL;
	}

#ifdef OPLUS_FEATURE_CAMERA_COMMON
	/*Added by rentianzhi@CamDrv, release the hw resource for Explorer AON driver, 20220124*/
	if (ctx->support_explorer_aon_fl == 1 && ctx->idx == IMGSENSOR_SENSOR_IDX_SUB) {
		dev_dbg(ctx->dev, "In adaptor_hw_power_off:Start to release the supply and clk resource for front camera.\n");
		/* clocks */
		for (i = 0; i < CLK_MAXCNT; i++) {
			if (ctx->clk[i] != NULL) {
				devm_clk_put(ctx->dev, ctx->clk[i]);
				ctx->clk[i] = NULL;
				dev_dbg(ctx->dev, "In %s:Release clk:%s ok.\n", __func__, clk_names[i]);
			}
		}
		/* supplies */
		for (i = 0; i < REGULATOR_MAXCNT; i++) {
			if (ctx->regulator[i] != NULL) {
				devm_regulator_put(ctx->regulator[i]);
				ctx->regulator[i] = NULL;
				dev_dbg(ctx->dev, "In %s:Release regulator:%s ok.\n", __func__, reg_names[i]);
			}
		}
		/*pinctrl*/
		devm_pinctrl_put(ctx->pinctrl);
		ctx->pinctrl = NULL;
	}
	mutex_init(&ctx->hw_mutex);
#endif

#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]-\n", __func__);
#endif
	return 0;
}

int adaptor_hw_sensor_reset(struct adaptor_ctx *ctx)
{
	dev_info(ctx->dev, "%s %d|%d|%d\n",
		__func__,
		ctx->is_streaming,
		ctx->is_sensor_inited,
		ctx->power_refcnt);

	if (ctx->is_streaming == 1 &&
		ctx->is_sensor_inited == 1 &&
		ctx->power_refcnt > 0) {

		do_hw_power_off(ctx);
		do_hw_power_on(ctx);

		return 0;
	}
	dev_info(ctx->dev, "%s skip to reset due to either integration or else\n",
		__func__);

	return -1;
}


