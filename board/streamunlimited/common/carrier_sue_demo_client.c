#include <common.h>
#include <asm/gpio.h>
#include <asm/arch/mx7-pins.h>

#include "device_interface.h"

#define FWUP_GPIO IMX_GPIO_NR(6, 20)

static const iomux_v3_cfg_t fwupdate_pads[] = {
	MX7D_PAD_SAI2_TX_BCLK__GPIO6_IO20 | MUX_PAD_CTRL(PAD_CTL_PUE | PAD_CTL_PUS_PU5KOHM),
};

static int demo_client_init(const struct sue_device_info *device)
{
	imx_iomux_v3_setup_multiple_pads(fwupdate_pads, ARRAY_SIZE(fwupdate_pads));
	gpio_direction_input(FWUP_GPIO);

	return 0;
}

static int demo_client_late_init(const struct sue_device_info *device)
{
	return 0;
}

static int demo_client_get_usb_update_request(const struct sue_device_info *device)
{
	return gpio_get_value(FWUP_GPIO);
}

struct sue_carrier_ops demo_client_ops = {
	.init = demo_client_init,
	.late_init = demo_client_late_init,
	.get_usb_update_request = demo_client_get_usb_update_request,
};
