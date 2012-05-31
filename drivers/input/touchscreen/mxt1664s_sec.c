/*
 *  drivers/input/touchscreen/mxt1664s_sec.c
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/err.h>

#include <linux/i2c/mxt1664s.h>

#include "mxt1664s_dev.h"

#if TSP_SEC_SYSFS

/* TODO:
*	need to add dignostic fucntions for read reference
*	and data form IC to usc T37 register
*/

static void set_default_result(struct mxt_data_sysfs *data)
{
	char delim = ':';

	memset(data->cmd_result, 0x00, ARRAY_SIZE(data->cmd_result));
	memcpy(data->cmd_result, data->cmd, strlen(data->cmd));
	strncat(data->cmd_result, &delim, 1);
}

static void set_cmd_result(struct mxt_data_sysfs *data, char *buff, int len)
{
	strncat(data->cmd_result, buff, len);
}

static void not_support_cmd(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct i2c_client *client = data->client;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	char buff[16] = {0};

	set_default_result(sysfs_data);
	sprintf(buff, "%s", "NA");
	set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));
	sysfs_data->cmd_state = CMD_STATUS_NOT_APPLICABLE;
	dev_info(&client->dev, "%s: \"%s(%d)\"\n", __func__,
				buff, strnlen(buff, sizeof(buff)));
}

static void fw_update(void *device_data)
{
	/* ....  */
}

static void get_fw_ver_bin(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct i2c_client *client = data->client;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	char buff[40] = {0};

	set_default_result(sysfs_data);
	snprintf(buff, sizeof(buff), "version: %#02x build: %#02x",
			MXT_FIRM_VERSION, MXT_FIRM_BUILD);

	set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));
	sysfs_data->cmd_state = CMD_STATUS_OK;
	dev_info(&client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct i2c_client *client = data->client;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	char buff[40] = {0};
	int ver, build;

	set_default_result(sysfs_data);

	ver = data->info.version;
	build = data->info.build;
	snprintf(buff, sizeof(buff), "version: %#02x build: %#02x", ver, build);

	set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));
	sysfs_data->cmd_state = CMD_STATUS_OK;
	dev_info(&client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_config_ver(void *device_data)
{
	not_support_cmd(device_data);
}

static void get_threshold(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct i2c_client *client = data->client;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	char buff[16] = {0};
	u8 threshold;

	set_default_result(sysfs_data);

	threshold = mxt_read_object(data,
		TOUCH_MULTITOUCHSCREEN_T9, 7, &threshold);

	if (threshold < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));
		sysfs_data->cmd_state = CMD_STATUS_FAIL;
		return;
	}
	snprintf(buff, sizeof(buff), "%d", threshold);

	set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));
	sysfs_data->cmd_state = CMD_STATUS_OK;
	dev_info(&client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void module_off_master(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct i2c_client *client = data->client;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	char buff[3] = {0};

	mutex_lock(&data->lock);
	if (data->mxt_enabled) {
		if (data->pdata->power_off())
			snprintf(buff, sizeof(buff), "%s", "NG");
		else
			snprintf(buff, sizeof(buff), "%s", "OK");

		disable_irq(client->irq);
		data->mxt_enabled = false;
	} else
		snprintf(buff, sizeof(buff), "%s", "OK");

	mutex_unlock(&data->lock);

	set_default_result(sysfs_data);
	set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));

	if (strncmp(buff, "OK", 2) == 0)
		sysfs_data->cmd_state = CMD_STATUS_OK;
	else
		sysfs_data->cmd_state = CMD_STATUS_FAIL;

	dev_info(&client->dev, "%s: %s\n", __func__, buff);
}

static void module_on_master(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct i2c_client *client = data->client;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	char buff[3] = {0};

	mutex_lock(&data->lock);
	if (!data->mxt_enabled) {
		if (data->pdata->power_on())
			snprintf(buff, sizeof(buff), "%s", "NG");
		else
			snprintf(buff, sizeof(buff), "%s", "OK");

		enable_irq(client->irq);
		data->mxt_enabled = true;
	} else {
		snprintf(buff, sizeof(buff), "%s", "OK");
	}
	mutex_unlock(&data->lock);

	set_default_result(sysfs_data);
	set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));

	if (strncmp(buff, "OK", 2) == 0)
		sysfs_data->cmd_state = CMD_STATUS_OK;
	else
		sysfs_data->cmd_state = CMD_STATUS_FAIL;

	dev_info(&client->dev, "%s: %s\n", __func__, buff);

}

static void get_chip_vendor(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct i2c_client *client = data->client;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	char buff[16] = {0};

	set_default_result(sysfs_data);

	snprintf(buff, sizeof(buff), "%s", "ATMEL");
	set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));
	sysfs_data->cmd_state = CMD_STATUS_OK;
	dev_info(&client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_chip_name(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct i2c_client *client = data->client;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	char buff[16] = {0};

	set_default_result(sysfs_data);

	snprintf(buff, sizeof(buff), "%s", "MXT1664S");
	set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));
	sysfs_data->cmd_state = CMD_STATUS_OK;
	dev_info(&client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_x_num(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct i2c_client *client = data->client;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	char buff[16] = {0};
	int val;

	set_default_result(sysfs_data);
	val = data->info.matrix_xsize;
	if (val < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));
		sysfs_data->cmd_state = CMD_STATUS_FAIL;

		dev_info(&client->dev,
			 "%s: fail to read num of x (%d).\n", __func__, val);

		return;
	}
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));
	sysfs_data->cmd_state = CMD_STATUS_OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_y_num(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct i2c_client *client = data->client;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	char buff[16] = {0};
	int val;

	set_default_result(sysfs_data);
	val = data->info.matrix_ysize;
	if (val < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));
		sysfs_data->cmd_state = CMD_STATUS_FAIL;

		dev_info(&client->dev,
			 "%s: fail to read num of y (%d).\n", __func__, val);

		return;
	}
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(sysfs_data, buff, strnlen(buff, sizeof(buff)));
	sysfs_data->cmd_state = CMD_STATUS_OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void run_reference_read(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	set_default_result(sysfs_data);
	sysfs_data->cmd_state = CMD_STATUS_OK;

}

static void get_reference(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	set_default_result(sysfs_data);
	/* add read function */
	sysfs_data->cmd_state = CMD_STATUS_OK;

}

static void run_delta_read(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	set_default_result(sysfs_data);
	/* add read function */
	sysfs_data->cmd_state = CMD_STATUS_OK;

}

static void get_delta(void *device_data)
{
	struct mxt_data *data = (struct mxt_data *)device_data;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	set_default_result(sysfs_data);
	/* add read function */
	sysfs_data->cmd_state = CMD_STATUS_OK;
}

#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func
#define TOSTRING (x) #x

struct tsp_cmd {
	struct list_head	list;
	const char		*cmd_name;
	void			(*cmd_func)(void *device_data);
};

struct tsp_cmd tsp_cmds[] = {
	{TSP_CMD("fw_update", fw_update),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_config_ver", get_config_ver),},
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("module_off_master", module_off_master),},
	{TSP_CMD("module_on_master", module_on_master),},
	{TSP_CMD("module_off_slave", not_support_cmd),},
	{TSP_CMD("module_on_slave", not_support_cmd),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("run_reference_read", run_reference_read),},
	{TSP_CMD("get_reference", get_reference),},
	{TSP_CMD("run_delta_read", run_delta_read),},
	{TSP_CMD("get_delta", get_delta),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},
};

/* Functions related to basic interface */
static ssize_t store_cmd(struct device *dev, struct device_attribute
				  *devattr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0};
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;
	int ret;

	if (sysfs_data->cmd_is_running == true) {
		dev_err(&client->dev, "tsp_cmd: other cmd is running.\n");
		goto err_out;
	}

	/* check lock  */
	mutex_lock(&sysfs_data->cmd_lock);
	sysfs_data->cmd_is_running = true;
	mutex_unlock(&sysfs_data->cmd_lock);

	sysfs_data->cmd_state = CMD_STATUS_RUNNING;

	for (i = 0; i < ARRAY_SIZE(sysfs_data->cmd_param); i++)
		sysfs_data->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(sysfs_data->cmd, 0x00, ARRAY_SIZE(sysfs_data->cmd));
	memcpy(sysfs_data->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &sysfs_data->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr,
			&sysfs_data->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				ret = kstrtoint(buff, 10,\
					sysfs_data->cmd_param + param_cnt);
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	dev_info(&client->dev, "cmd = %s\n", tsp_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		dev_info(&client->dev, "cmd param %d= %d\n", i,
						sysfs_data->cmd_param[i]);

	tsp_cmd_ptr->cmd_func(data);
err_out:
	return count;
}

static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	char buff[16] = {0};

	dev_info(&data->client->dev, "tsp cmd: status:%d\n",
			sysfs_data->cmd_state);

	if (sysfs_data->cmd_state == CMD_STATUS_WAITING)
		snprintf(buff, sizeof(buff), "WAITING");

	else if (sysfs_data->cmd_state == CMD_STATUS_RUNNING)
		snprintf(buff, sizeof(buff), "RUNNING");

	else if (sysfs_data->cmd_state == CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "OK");

	else if (sysfs_data->cmd_state == CMD_STATUS_FAIL)
		snprintf(buff, sizeof(buff), "FAIL");

	else if (sysfs_data->cmd_state == CMD_STATUS_NOT_APPLICABLE)
		snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", buff);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute
				    *devattr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_data_sysfs *sysfs_data = data->sysfs_data;

	dev_info(&data->client->dev,
		"tsp cmd: result: %s\n", sysfs_data->cmd_result);

	mutex_lock(&sysfs_data->cmd_lock);
	sysfs_data->cmd_is_running = false;
	mutex_unlock(&sysfs_data->cmd_lock);

	sysfs_data->cmd_state = CMD_STATUS_WAITING;

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", sysfs_data->cmd_result);
}

static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);

static struct attribute *touchscreen_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	NULL,
};

static struct attribute_group touchscreen_attr_group = {
	.attrs = touchscreen_attributes,
};

#endif	/* TSP_SEC_SYSFS*/

#if TSP_ITDEV
static int mxt_read_block(struct i2c_client *client,
		   u16 addr,
		   u16 length,
		   u8 *value)
{
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg[2];
	__le16	le_addr;
	struct mxt_data *data = i2c_get_clientdata(client);

	if (data != NULL) {
		if ((data->last_read_addr == addr) &&
			(addr == data->msg_proc)) {
			if  (i2c_master_recv(client, value, length) == length) {
				if (data->debug_enabled)
					print_hex_dump(KERN_INFO, "MXT RX:",
						DUMP_PREFIX_NONE, 16, 1,
						value, length, false);
				return 0;
			} else
				return -EIO;
		} else {
			data->last_read_addr = addr;
		}
	}

	le_addr = cpu_to_le16(addr);
	msg[0].addr  = client->addr;
	msg[0].flags = 0x00;
	msg[0].len   = 2;
	msg[0].buf   = (u8 *) &le_addr;

	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = length;
	msg[1].buf   = (u8 *) value;
	if (i2c_transfer(adapter, msg, 2) == 2) {
		if (data->debug_enabled) {
			print_hex_dump(KERN_INFO, "MXT TX:", DUMP_PREFIX_NONE,
				16, 1, msg[0].buf, msg[0].len, false);
			print_hex_dump(KERN_INFO, "MXT RX:", DUMP_PREFIX_NONE,
				16, 1, msg[1].buf, msg[1].len, false);
		}
		return 0;
	} else
		return -EIO;
}

/* Writes a block of bytes (max 256) to given address in mXT chip. */

int mxt_write_block(struct i2c_client *client,
		    u16 addr,
		    u16 length,
		    u8 *value)
{
	int i;
	struct {
		__le16	le_addr;
		u8	data[256];

	} i2c_block_transfer;

	struct mxt_data *data = i2c_get_clientdata(client);

	if (length > 256)
		return -EINVAL;

	if (data != NULL)
		data->last_read_addr = -1;

	for (i = 0; i < length; i++)
		i2c_block_transfer.data[i] = *value++;

	i2c_block_transfer.le_addr = cpu_to_le16(addr);

	i = i2c_master_send(client, (u8 *) &i2c_block_transfer, length + 2);

	if (i == (length + 2)) {
		if (data->debug_enabled)
			print_hex_dump(KERN_INFO, "MXT TX:", DUMP_PREFIX_NONE,
				16, 1, &i2c_block_transfer, length+2, false);
		return length;
	} else
		return -EIO;
}

static ssize_t mem_access_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	int ret = 0;
	struct i2c_client *client =
		to_i2c_client(container_of(kobj, struct device, kobj));

	dev_info(&client->dev, "mem_access_read p=%p off=%lli c=%zi\n",
			buf, off, count);

	if (off >= 32768)
		return -EIO;

	if (off + count > 32768)
		count = 32768 - off;

	if (count > 256)
		count = 256;

	if (count > 0)
		ret = mxt_read_block(client, off, count, buf);

	return ret >= 0 ? count : ret;
}

static ssize_t mem_access_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	int ret = 0;
	struct i2c_client *client =
		to_i2c_client(container_of(kobj, struct device, kobj));

	dev_info(&client->dev, "mem_access_write p=%p off=%lli c=%zi\n",
			buf, off, count);

	if (off >= 32768)
		return -EIO;

	if (off + count > 32768)
		count = 32768 - off;

	if (count > 256)
		count = 256;

	if (count > 0)
		ret = mxt_write_block(client, off, count, buf);

	return ret >= 0 ? count : 0;
}

static ssize_t pause_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count = 0;

	count += sprintf(buf + count, "%d", data->driver_paused);
	count += sprintf(buf + count, "\n");

	return count;
}

static ssize_t pause_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->driver_paused = i;

		dev_info(&client->dev, "%s\n", i ? "paused" : "unpaused");
	} else {
		dev_info(&client->dev, "pause_driver write error\n");
	}

	return count;
}

static ssize_t debug_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count = 0;

	count += sprintf(buf + count, "%d", data->debug_enabled);
	count += sprintf(buf + count, "\n");

	return count;
}

static ssize_t debug_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	int i;
	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->debug_enabled = i;

		dev_info(&client->dev, "%s\n",
			i ? "debug enabled" : "debug disabled");
	} else {
		dev_info(&client->dev, "debug_enabled write error\n");
	}

	return count;
}

static ssize_t command_calibrate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret;

	/* send calibration command to the chip */
	ret = mxt_write_object(data, GEN_COMMANDPROCESSOR_T6,
		CMD_CALIBRATE_OFFSET, 1);

	return (ret < 0) ? ret : count;
}

static ssize_t command_reset_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret;

	/* send reset command to the chip */
	ret = mxt_write_object(data, GEN_COMMANDPROCESSOR_T6,
		CMD_RESET_OFFSET, 1);

	return (ret < 0) ? ret : count;
}

static ssize_t command_backup_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret;

	/* send backup command to the chip */
	ret = mxt_write_object(data, GEN_COMMANDPROCESSOR_T6,
		CMD_BACKUP_OFFSET, 0x55);

	return (ret < 0) ? ret : count;
}
#endif	/* TSP_ITDEV */

static ssize_t mxt_debug_setting(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	return 0;
}

static ssize_t mxt_object_setting(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	unsigned int object_type;
	unsigned int object_register;
	unsigned int register_value;
	u8 val;
	int ret;
	sscanf(buf, "%u%u%u", &object_type, &object_register, &register_value);
	dev_info(&client->dev, "object type T%d", object_type);
	dev_info(&client->dev, "object register ->Byte%d\n", object_register);
	dev_info(&client->dev, "register value %d\n", register_value);

	ret = mxt_write_object(data,
		(u8)object_type, (u8)object_register, (u8)register_value);

	if (ret) {
		dev_err(&client->dev, "fail to write T%d index:%d, value:%d\n",
			object_type, object_register, register_value);
		goto out;
	} else {
		ret = mxt_read_object(data,
				(u8)object_type, (u8)object_register, &val);

		if (ret) {
			dev_err(&client->dev, "fail to read T%d\n",
				object_type);
			goto out;
		} else
			dev_info(&client->dev, "T%d Byte%d is %d\n",
				object_type, object_register, val);
	}
out:
	return count;
}

static ssize_t mxt_object_show(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	struct mxt_object *object;
	unsigned int object_type;
	u8 val;
	u16 i;

	sscanf(buf, "%u", &object_type);
	dev_info(&client->dev, "object type T%d\n", object_type);

	object = mxt_get_object_info(data, object_type);
	if (object == NULL) {
		dev_err(&client->dev, "fail to get object_info\n");
		goto out;
	} else {
		for (i = 0; i < object->size; i++) {
			mxt_read_mem(data, object->start_address + i,
				1, &val);
			dev_info(&client->dev, "Byte %u --> %u\n", i, val);
		}
	}
out:
	return count;
}

#if TSP_ITDEV
/* Functions for mem_access interface */
struct bin_attribute mem_access_attr;

/* Sysfs files for libmaxtouch interface */
static DEVICE_ATTR(pause_driver, S_IRUGO | S_IWUGO,
	pause_show, pause_store);
static DEVICE_ATTR(debug_enable, S_IRUGO | S_IWUGO,
	debug_enable_show, debug_enable_store);
static DEVICE_ATTR(command_calibrate, S_IRUGO | S_IWUGO,
	NULL, command_calibrate_store);
static DEVICE_ATTR(command_reset, S_IRUGO | S_IWUGO,
	NULL, command_reset_store);
static DEVICE_ATTR(command_backup, S_IRUGO | S_IWUGO,
	NULL, command_backup_store);
#endif
static DEVICE_ATTR(object_show, S_IWUSR | S_IWGRP, NULL,
	mxt_object_show);
static DEVICE_ATTR(object_write, S_IWUSR | S_IWGRP, NULL,
	mxt_object_setting);
static DEVICE_ATTR(dbg_switch, S_IWUSR | S_IWGRP, NULL,
	mxt_debug_setting);

static struct attribute *libmaxtouch_attributes[] = {
#if TSP_ITDEV
	&dev_attr_pause_driver.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_command_calibrate.attr,
	&dev_attr_command_reset.attr,
	&dev_attr_command_backup.attr,
#endif
	&dev_attr_object_show.attr,
	&dev_attr_object_write.attr,
	&dev_attr_dbg_switch.attr,
	NULL,
};

static struct attribute_group libmaxtouch_attr_group = {
	.attrs = libmaxtouch_attributes,
};

int  __devinit mxt_sysfs_init(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);
	int i;
	int ret;
#if TSP_SEC_SYSFS
	struct mxt_data_sysfs *sysfs_data = NULL;
	struct device *fac_dev_ts;

	sysfs_data = kzalloc(sizeof(struct mxt_data_sysfs), GFP_KERNEL);
	if (sysfs_data == NULL) {
		dev_err(&client->dev, "failed to allocate sysfs data.\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&sysfs_data->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
		list_add_tail(&tsp_cmds[i].list, &sysfs_data->cmd_list_head);

	mutex_init(&sysfs_data->cmd_lock);
	sysfs_data->cmd_is_running = false;

	data->sysfs_data = sysfs_data;

	fac_dev_ts = device_create(sec_class,
			NULL, 0, data, "sec_touchscreen");
	if (IS_ERR(fac_dev_ts)) {
		dev_err(&client->dev, "Failed to create device for the sysfs\n");
		ret = IS_ERR(fac_dev_ts);
		goto free_mem;
	}
	ret = sysfs_create_group(&fac_dev_ts->kobj, &touchscreen_attr_group);
	if (ret) {
		dev_err(&client->dev, "Failed to create touchscreen sysfs group\n");
		goto free_mem;
	}
#endif

	ret = sysfs_create_group(&client->dev.kobj, &libmaxtouch_attr_group);
	if (ret) {
		dev_err(&client->dev, "Failed to create libmaxtouch sysfs group\n");
		goto free_mem;
	}

#if TSP_ITDEV
	sysfs_bin_attr_init(&mem_access_attr);
	mem_access_attr.attr.name = "mem_access";
	mem_access_attr.attr.mode = S_IRUGO | S_IWUGO;
	mem_access_attr.read = mem_access_read;
	mem_access_attr.write = mem_access_write;
	mem_access_attr.size = 65535;

	if (sysfs_create_bin_file(&client->dev.kobj, &mem_access_attr) < 0) {
		dev_err(&client->dev, "Failed to create device file(%s)!\n",
				mem_access_attr.attr.name);
		goto free_mem;
	}
#endif

	return 0;

free_mem:
#if TSP_SEC_SYSFS
	kfree(sysfs_data);
#endif
	return ret;
}

/* TODO: it will be applied */
#if TSP_BOOSTER
static void mxt_set_dvfs_off(struct work_struct *work)
{
	struct mxt_data *data =
		container_of(work, struct mxt_data, booster.dvfs_dwork.work);

	if (data->mxt_enabled) {
		disable_irq(client->irq);

		dev_info(&data->client->dev, "%s!!!\n", __func__);

		if (data->booster.touch_cpu_lock_status) {
			exynos_cpufreq_lock_free(DVFS_LOCK_ID_TSP);
			data->booster.touch_cpu_lock_status = false;
		}
		enable_irq(client->irq);
	}
}

static void mxt_set_dvfs_on(struct mxt_data *data)
{
	dev_info(&data->client->dev, "%s!!!\n", __func__);

	cancel_delayed_work(&data->booster.dvfs_dwork);
	exynos_cpufreq_lock(DVFS_LOCK_ID_TSP,
		data->booster.cpu_lv);
	data->booster.touch_cpu_lock_status = true;
}

static int mxt_init_dvfs(struct mxt_data *data)
{
	int ret;

	INIT_DELAYED_WORK(&data->booster.dvfs_dwork,
		mxt_set_dvfs_off);
	return exynos_cpufreq_get_level(TOUCH_BOOSTER_LIMIT_CLK,
		&data->booster.cpu_lv);
}
#endif /* - TOUCH_BOOSTER */

MODULE_DESCRIPTION("sec sysfs for the Atmel");
MODULE_LICENSE("GPL");